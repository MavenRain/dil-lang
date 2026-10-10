// Exercise the generated JS interface against states and trees evaluated in dil-lang.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const {spawnSync} = require('node:child_process');
const {pathToFileURL} = require('node:url');

const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'dil-app-'));
let checked = 0;
let failed = 0;

function test(name, f) {
  checked++;
  try { f(); }
  catch (e) {
    failed++;
    console.error(`FAIL ${name}: ${e.message}`);
  }
}

async function build(source, name, flags = []) {
  const module = path.join(tmp, `${name}.mjs`);
  const document = path.join(tmp, `${name}.json`);
  const r = spawnSync('build/langc', ['build', source, ...flags, '--js', module, '--json', document, '--selftest'], {encoding: 'utf8'});
  assert.ifError(r.error);
  assert.equal(r.signal, null);
  assert.equal(r.status, 0, r.stderr);
  assert.equal(r.stdout, '');
  const text = fs.readFileSync(document, 'utf8');
  return {m: await import(pathToFileURL(module).href), text, js: fs.readFileSync(module, 'utf8'), doc: JSON.parse(text)};
}

// Runs a build that dil-lang refuses: exit 1 and no output file. Returns stderr.
function refused(source, name, flags = []) {
  const module = path.join(tmp, `${name}.mjs`);
  const document = path.join(tmp, `${name}.json`);
  const r = spawnSync('build/langc', ['build', source, ...flags, '--js', module, '--json', document, '--selftest'], {encoding: 'utf8'});
  assert.ifError(r.error);
  assert.equal(r.status, 1, r.stderr);
  assert.ok(!fs.existsSync(module) && !fs.existsSync(document), 'a refused build writes no file');
  return r.stderr;
}

// Each definition of the shorter meaning A is a definition of the longer meaning B with the same value.
function extendsMeaning(a, b) {
  const longer = new Map(b.m.document.instances.map(i => [i.name, plain(i)]));
  for (const i of a.m.document.instances) assert.deepEqual(longer.get(i.name), plain(i), i.name);
  for (const k of Object.keys(a.m).filter(k => typeof a.m[k] !== 'function' && k !== 'document' && k !== 'entries'))
    assert.deepEqual(plain(b.m[k]), plain(a.m[k]), k);
  for (const k of Object.keys(a.m.entries)) assert.ok(k in b.m.entries, k);
}

const plain = v => JSON.parse(JSON.stringify(v, (_, x) => typeof x === 'bigint' ? Number(x) : x));
const host = v => JSON.parse(JSON.stringify(v), (k, x) => k === 'valNat' ? BigInt(x) : x);
const key = keyName => ({keyName});
const event = (k, evPayload) => ({evTarget: key(k), evPayload});
const value = text => ({tag: 'vText', valText: text});
const entry = text => ({first: key('label'), second: value(text)});

async function main() {
  const {m, text, js, doc} = await build('test/app/program.lang', 'program');
  const instance = name => doc.instances.find(i => i.name === name).value;
  test('manifest matches the C writer', () => assert.equal(m.encode(m.document), text));
  test('init and view match the C evaluator', () => {
    assert.deepEqual(plain(m.init), instance('init'));
    assert.deepEqual(plain(m.view(m.init)), doc.tree);
  });
  test('event sequence matches the C evaluator', () => {
    let state = m.init;
    for (const name of ['eEnabled', 'eCount', 'eLabel']) state = m.step(host(instance(name)), state);
    assert.deepEqual(plain(state), instance('after'));
    assert.deepEqual(plain(m.view(state)), instance('treeAfter'));
    assert.deepEqual(plain(m.init), instance('init'));
  });
  test('an unknown event preserves the state', () => assert.deepEqual(m.step(event('unknown', value('')), m.init), m.init));
  test('malformed values are refused', () => {
    for (const state of [null, {}, [undefined], [{first: key('label'), second: {tag: 'vNat', valNat: 1}}]]) {
      assert.throws(() => m.view(state), TypeError);
      assert.throws(() => m.step(event('unknown', value('')), state), TypeError);
    }
    for (const e of [null, {}, event('count', {tag: 'vNat', valNat: -1n}), event('count', {tag: 'vNat', valNat: 1n << 64n})])
      assert.throws(() => m.step(e, m.init), TypeError);
  });
  test('sparse states are refused by view and step', () => {
    for (const state of [new Array(1), [m.init[0], , m.init[2]]]) {
      assert.throws(() => m.view(state), TypeError);
      assert.throws(() => m.step(event('unknown', value('')), state), TypeError);
    }
  });
  test('unpaired surrogates are refused in values and keys', () => {
    for (const s of ['\ud800', '\udfff', 'x\ud800y', 'x\udfffy']) {
      assert.throws(() => m.view([entry(s)]), TypeError);
      assert.throws(() => m.step(event('label', value(s)), m.init), TypeError);
      assert.throws(() => m.view([{first: key(s), second: value('')}]), TypeError);
      assert.throws(() => m.step(event(s, value('')), m.init), TypeError);
    }
  });
  test('Unicode scalar values survive a round trip', () => {
    for (const s of ['', '\u0000\u001f', '\ud7ff\ue000', 'café ☃ 😀\n"\\', '\udbff\udfff']) {
      const state = m.step(event('label', value(s)), m.init);
      assert.equal(state[2].second.valText, s);
      assert.equal(m.view(state).compKids.txt, s);
    }
  });
  const recovery = await build('test/app/recovery.lang', 'recovery');
  const recoveryInit = structuredClone(recovery.m.init);
  const recoveredInstance = name => host(recovery.doc.instances.find(i => i.name === name).value);
  test('an overflowing interpolation omits its node and preserves its sibling', () => {
    assert.equal(recovery.m.encode(recovery.m.document), recovery.text);
    assert.deepEqual(recovery.m.view(recovery.m.init), recovery.doc.tree);
    assert.deepEqual(recovery.m.view(recovery.m.init), {tag: 'comp', compName: {nameText: 'Button'}, compProps: [], compKids: {tag: 'text', txt: 'keep', txtNext: {tag: 'tEnd'}}, compNext: {tag: 'tEnd'}});
  });
  test('an isolated transition overflow preserves the input state', () => {
    assert.deepEqual(recovery.m.step(recoveredInstance('eMore'), recovery.m.init), recoveryInit);
    assert.deepEqual(recovery.m.init, recoveryInit);
  });
  test('the omitted interpolation returns after a valid setter event', () => {
    const state = recovery.m.step(recoveredInstance('eSet'), recovery.m.init);
    assert.deepEqual(state, recoveredInstance('after'));
    assert.deepEqual(recovery.m.view(state), recoveredInstance('treeAfter'));
    assert.deepEqual(recovery.m.init, recoveryInit);
  });
  const raw = (await build('test/app/unisolated.lang', 'unisolated')).m;
  test('an unisolated transition overflow throws RangeError', () => {
    assert.throws(() => raw.step(event('more', {tag: 'vNat', valNat: 0n}), raw.init), RangeError);
  });
  // Partial programs: byte prefixes of program.lang, cut at marker text.
  const bytes = fs.readFileSync('test/app/program.lang');
  const find = (marker, from = 0) => {
    const i = bytes.indexOf(marker, from);
    assert.notEqual(i, -1, marker);
    return i;
  };
  const after = (marker, from = 0) => find(marker, from) + Buffer.byteLength(marker);
  const prefix = (name, end) => {
    const file = path.join(tmp, `${name}.lang`);
    fs.writeFileSync(file, bytes.subarray(0, end));
    return file;
  };
  const cutBuild = (name, end, flags) => build(prefix(name, end), name, flags);
  const partial = ['--partial'];
  const closedCut = after('(interpolate (cell kLabel (vText ""');
  const droppedCut = after('(cell kCount (vNat ');
  const tokenCut = after(':= re', find('def stateLaw'));
  const literalCut = find('😀', find('def eLabel')) + 2;
  const closed = await cutBuild('closed', closedCut, partial);
  const appLine = await cutBuild('app-line', find('def eEnabled'));
  const dropped = await cutBuild('dropped', droppedCut, partial);
  const noApp = await cutBuild('no-app', find('def app'));
  const boundary = prefix('boundary', find('def app'));
  const boundaryPartial = await build(boundary, 'boundary-partial', partial);
  const boundaryStrict = await build(boundary, 'boundary-strict');
  const token = await cutBuild('token', tokenCut, partial);
  const noStateLaw = await cutBuild('no-state-law', find('def stateLaw'));
  const literal = await cutBuild('literal', literalCut, partial);
  const noLabel = await cutBuild('no-label', find('def eLabel'));
  const lineStarts = [0, ...[...bytes.keys()].filter(i => bytes[i] === 0x0a).map(i => i + 1)];
  const cuts = [...new Set([...lineStarts, closedCut, droppedCut, tokenCut, literalCut])].sort((a, b) => a - b);
  const chain = await Promise.all(cuts.map(end => cutBuild(`chain-${end}`, end, partial)));
  const whole = await build('test/app/program.lang', 'program-partial', partial);
  const encoded = b => assert.equal(b.m.encode(b.m.document), b.text);
  const sameDocument = (a, b) => {
    encoded(a);
    assert.equal(a.text, b.text);
  };
  test('a cut inside an open form that closes keeps the item', () => {
    sameDocument(closed, appLine);
    assert.deepEqual(plain(closed.m.view(closed.m.init)), plain(appLine.m.view(appLine.m.init)));
    for (const name of ['eEnabled', 'eCount', 'eLabel'])
      assert.deepEqual(plain(closed.m.step(host(instance(name)), closed.m.init)), plain(appLine.m.step(host(instance(name)), appLine.m.init)));
  });
  test('a cut inside an open form that does not check drops the item', () => sameDocument(dropped, noApp));
  test('a cut at a definition boundary is its own complete prefix', () => {
    sameDocument(boundaryPartial, boundaryStrict);
    assert.equal(boundaryPartial.js, boundaryStrict.js);
  });
  test('a cut inside a token drops the token', () => sameDocument(token, noStateLaw));
  test('a cut inside a string literal drops the literal', () => sameDocument(literal, noLabel));
  test('partial meaning is monotone in the prefix order', () => {
    chain.forEach(encoded);
    chain.slice(1).forEach((longer, i) => extendsMeaning(chain[i], longer));
  });
  test('strict mode still refuses a cut source', () => {
    assert.match(refused(prefix('strict-cut', closedCut), 'strict-cut'), /^langc: PARSE_EXPECT: /);
    const line = after('def kCount : Key := key "count"');
    const stray = path.join(tmp, 'stray.lang');
    fs.writeFileSync(stray, Buffer.concat([bytes.subarray(0, line), Buffer.from(')'), bytes.subarray(line)]));
    const strict = refused(stray, 'stray-strict');
    assert.match(strict, /^langc: PARSE_EXPECT: /);
    assert.equal(refused(stray, 'stray-partial', partial), strict);
  });
  test('a complete source builds the same with and without --partial', () => {
    assert.equal(whole.text, text);
    assert.equal(whole.js, js);
  });
}

main().catch(e => { failed++; console.error(e); }).finally(() => {
  fs.rmSync(tmp, {recursive: true, force: true});
  console.log(`app regressions: ${checked} checked, ${failed} failed`);
  process.exitCode = failed ? 1 : 0;
});

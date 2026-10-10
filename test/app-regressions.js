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

async function build(source, name) {
  const module = path.join(tmp, `${name}.mjs`);
  const document = path.join(tmp, `${name}.json`);
  const r = spawnSync('build/langc', ['build', source, '--js', module, '--json', document, '--selftest'], {encoding: 'utf8'});
  assert.ifError(r.error);
  assert.equal(r.signal, null);
  assert.equal(r.status, 0, r.stderr);
  assert.equal(r.stdout, '');
  const text = fs.readFileSync(document, 'utf8');
  return {m: await import(pathToFileURL(module).href), text, doc: JSON.parse(text)};
}

const plain = v => JSON.parse(JSON.stringify(v, (_, x) => typeof x === 'bigint' ? Number(x) : x));
const host = v => JSON.parse(JSON.stringify(v), (k, x) => k === 'valNat' ? BigInt(x) : x);
const key = keyName => ({keyName});
const event = (k, evPayload) => ({evTarget: key(k), evPayload});
const value = text => ({tag: 'vText', valText: text});
const entry = text => ({first: key('label'), second: value(text)});

async function main() {
  const {m, text, doc} = await build('test/app/program.lang', 'program');
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
}

main().catch(e => { failed++; console.error(e); }).finally(() => {
  fs.rmSync(tmp, {recursive: true, force: true});
  console.log(`app regressions: ${checked} checked, ${failed} failed`);
  process.exitCode = failed ? 1 : 0;
});

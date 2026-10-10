# dil-lang specification

Status: complete for slices D0 to D5 (2026-10-10). Slice D6 adds this text
and no rule. Each rule gives the test that checks it. Section 9 gives the
open items.

## 1. Purpose

dil-lang is a language for reactive interfaces. A program describes one pure
view from a state to an interface tree, and one pure transition from an
event and a state to the next state (`design/DESIGN.md`, section Semantic
domain).

The compiler `langc` uses `domain/domain.lang`, embedded at build time, and
reads one program. It checks the type of each definition and proves each equality
definition. Then `langc build` writes an ES module, a JSON document or both
(section 7.1). The host is TinyCC with the lang-template kit `hosts/tcc-js`.
The target is JavaScript and JSON.

The type formers are F1 to F15 of `formers/FORMERS.md`. The core data types
and the core operations are only those of `design/DESIGN.md` (the design).
`domain/domain.lang` gives them in host forms. Each family, constructor,
field and definition name in that file is a core name.

These parts of the design are not in the language (`docs/PLAN.md`,
section 1): the tag syntax, the external data bindings of the environment,
and the operational matters (streaming, workers, keyed state across
recompiles). The manifest gives the keys of the state, so a runtime can keep
the state across recompiles.

## 2. Programs

A program is a sequence of definitions:

```
def NAME : TYPE := TERM
```

| ID | Rule | Test |
|---|---|---|
| S1 | The syntax is the expression syntax of the kit. There is no tag syntax. | each file in `examples/` (line `examples` of `make check`) |
| S2 | `Nat` is an unsigned 64-bit number. A literal above 18446744073709551615 is a refusal. | `test/parse/nat-range.lang` (`LEX_NAT_RANGE`) |
| S3 | A Nat result above 18446744073709551615 is a trap. For a Nat entry, `langc eval` prints `trap`, reports `EVAL_OVERFLOW` on stderr and exits 0. `langc build` refuses an unhandled trap in an instance with `EVAL_OVERFLOW`. The JS module uses `BigInt` and throws a `RangeError` on an unhandled overflow. | `test/eval/expect.txt` rows `entries addPrice 18446744073709551615 1` and `entries scale 4294967296 4294967296`; `test/emit/trap.lang`; `test/app-regressions.js` row `an unisolated transition overflow throws RangeError` |
| S4 | `Str` is a primitive. A literal is `"..."` with the escapes `\"`, `\\` and `\n`. The only Str operations are `strEq : Str -> Str -> Flag` and `showNat : Nat -> Str`. | `examples/text.lang` and the `text` rows of `test/eval/expect.txt`; the Str refusals below |
| S5 | A program cannot declare a family. Only `domain/domain.lang` can. | `test/check/data.lang` (`REFUSE_DATA`) |
| S6 | A program cannot add an unproved fact or use general recursion. Recursion comes only from `fold` and `unfold` (F6, F7). | `test/check/axiom.lang` (`REFUSE_AXIOM`); `test/check/rec.lang`, `test/check/self.lang`, `test/check/later.lang` (`REFUSE_REC`) |
| S7 | A definition cannot use a core name, a domain name, a reserved name or a name that a program defines again. | `test/check/core-name.lang`, `test/check/domain-name.lang`, `test/check/reserved.lang`, `test/check/duplicate.lang` (`REFUSE_NAME`) |
| S8 | A program cannot use `natSub`. | `test/check/nat-sub.lang` (`REFUSE_DESIGN`) |
| S9 | `langc check` proves each equality definition, including parameterized proof functions. `langc build --selftest` adds JS checks only for definitions whose type is `Eq A a b` and whose carrier has no function or type inside. It evaluates both sides and stops with an error when they differ. Other carriers are skipped with a comment; parameterized proof functions have no JS selftest. | `examples/laws.lang` (`eta` is skipped); `examples/trans.lang` (parameterized laws and closed instances); line `js` of `make check` |
| S10 | The compiler writes the target. A program cannot write it. A refused build writes no file. | `test/emit/expect.txt` (8 rows); line `js build refusals` of `make check` |

The refusal codes and their tests:

| Stage | Code | Test files |
|---|---|---|
| lex | `LEX_CHAR` | `test/parse/bad-char.lang` |
| lex | `LEX_NAT_RANGE` | `test/parse/nat-range.lang` |
| lex | `LEX_STR_OPEN` | `test/parse/string.lang` |
| lex | `LEX_STR_ESCAPE` | `test/parse/string-escape.lang` |
| lex | `LEX_STR_CONTROL` | `test/parse/string-newline.lang`, `test/parse/string-control.lang` |
| parse | `PARSE_EXPECT` | `test/parse/unclosed.lang`, `test/parse/no-body.lang`, `test/parse/no-keyword.lang`, `test/parse/fun-no-binder.lang`, `test/parse/type-no-level.lang` |
| parse | `PARSE_DEPTH` | row `deep.lang` of `test/parse/expect.txt` |
| check | `REFUSE_DATA`, `REFUSE_AXIOM`, `REFUSE_REC`, `REFUSE_NAME`, `REFUSE_DESIGN` | rules S5 to S8 |
| check | `NAME_UNKNOWN` | `test/check/unknown.lang` |
| check | `TYPE_MISMATCH` | `test/check/mismatch.lang`, `test/check/universe.lang`, `test/check/filter-sum.lang`, `test/check/bind-carrier.lang`, `test/check/dependent-result.lang`, `test/check/cumulative.lang`, `test/check/str-eq-nat.lang`, `test/check/show-nat-str.lang`, `test/check/str-not-nat.lang` |
| check | `TYPE_UNIVERSE` | `test/check/universe-range.lang`, `test/check/type2.lang` |
| check | `TYPE_INFER` | `test/check/no-type.lang`, `test/check/bare-fun.lang` |
| check | `TYPE_ARITY` | `test/check/arity.lang` |
| check | `TYPE_NOT_FUNCTION` | `test/check/not-function.lang` |
| check | `TYPE_REFL` | `test/check/bad-refl.lang`, `test/check/bad-eta.lang` |
| build | `EVAL_OVERFLOW` | `test/emit/trap.lang` |
| build | `JSON_VALUE` | `test/emit/function.lang` |
| build | `BUILD_APP` | section 7.3 |

## 3. Type formers

The type formers are F1 to F15 of `formers/FORMERS.md`. This language uses
the tcc-js column of the realization matrix. `formers/tcc-js.md` gives the
host form of each former. Each former is DONE on tcc-js. These three have a
limit:

| ID | Status on tcc-js | Effect on this language | Open item |
|---|---|---|---|
| F7 | DONE: `unfold` into List with a Nat step limit | An `unfold` stops after its step limit. | none |
| F14 | DONE: `Type 0` and `Type 1`, without cumulativity | A `Type 0` type is not a `Type 1` type (`test/check/cumulative.lang`). | none |
| F15 | DONE in `domain/` only | A program cannot declare a family (S5). A `UITree` node holds its next sibling (section 5). | none |

## 4. Structures

- **Monad**: `pure`, `map`, `bind` on Option, `Sum E` and List
  (`examples/monad.lang`, `examples/laws.lang`).
- **Algebra**: `fold` on Nat, List and each family, for example `joinTree`
  on `UITree`. `unfold` into List with a Nat step limit
  (`examples/algebra.lang`).
- **Filterable**: `filter` on Option and List (`examples/monad.lang`).
- **Monoid**: `Trans` under `idT` and `thenT`, and `Meaning` under `beside`
  (design, section Type-class morphisms). Section 6 gives the laws.

## 5. Core types

| Type | Meaning (design section) | Definition |
|---|---|---|
| `Key` | a component identifier (Semantic domain) | `family Key := key (keyName : Str)` |
| `Name` | the component name C of a tag (Compositional meaning function) | `family Name := name (nameText : Str)` |
| `Value` | a pure value (Semantic domain) | `family Value := vNat (valNat : Nat) \| vFlag (valFlag : Flag) \| vText (valText : Str)` |
| `State` | a partial map from Key to Value, with defaults (Semantic domain) | `def State : Type 0 := List (Prod Key Value)` |
| `Event` | an event with its payload (Semantic domain) | `family Event := event (evTarget : Key) (evPayload : Value)` |
| `Trans` | a transition, Event to State to State (Semantic domain) | `def Trans : Type 0 := Event -> State -> State` |
| `UITree` | an interface tree of Text and Comp nodes (Semantic domain) | `family UITree := tEnd \| text (txt : Str) (txtNext : UITree) \| comp (compName : Name) (compProps : List (Prod Name Value)) (compKids : UITree) (compNext : UITree)`, kit-forced |
| `Expr` | the meaning eval(e, s) of a pure expression (Compositional meaning function) | `def Expr : Type 0 := State -> Value` |
| `Meaning` | the denotation: a view and a transition (Semantic domain) | `def Meaning : Type 0 := Prod (State -> UITree) Trans` |

`UITree` is kit-forced. The kit refuses a `List UITree` field
(`DOMAIN_FAMILY`) and mutual families (`REFUSE_REC`). Thus each node holds
its next sibling: a `UITree` value is a list of sibling nodes, and `tEnd`
ends it (ruling 2026-10-09). The props of a node are pure values. A handler
is not in the view. It is in the transition (design, section Compositional
meaning function, event handlers). `docs/PLAN.md` section 2 names this type
`Iface` and has a `Prop` family. The domain uses `Meaning` and the props
type `List (Prod Name Value)`.

## 6. Core operations

| Operation | Type | Meaning (design section) |
|---|---|---|
| `keyEq` | `Key -> Key -> Flag` | key equality |
| `lookup` | `Key -> State -> Option Value` | the value of the first entry of the key |
| `getOr` | `Key -> Value -> State -> Value` | the value of the key, or a default (Semantic domain: totalized) |
| `set` | `Key -> Value -> State -> State` | the update s[k := v] |
| `idT` | `Trans` | the identity transition (static text) |
| `thenT` | `Trans -> Trans -> Trans` | sequential composition (Type-class morphisms) |
| `thenAll` | `List Trans -> Trans` | the monoidal combination of transitions (component tag) |
| `setT` | `Key -> Trans` | the update s[k := payload(e)] (state declaration) |
| `onTarget` | `Key -> Trans -> Trans` | runs a transition only on an event whose target is the key |
| `setter` | `Key -> Trans` | the setter of a cell as an event handler (event handlers) |
| `lit` | `Value -> Expr` | a constant (pure expressions) |
| `cell` | `Key -> Value -> Expr` | the value of a cell, or a default |
| `lift1` | `(Value -> Value) -> Expr -> Expr` | a function lifted pointwise (Functor morphism) |
| `lift2` | `(Value -> Value -> Value) -> Expr -> Expr -> Expr` | a function of two values lifted pointwise (Applicative morphism) |
| `showValue` | `Value -> Str` | the text of a value |
| `viewOf` | `Meaning -> State -> UITree` | the view of a meaning |
| `transOf` | `Meaning -> Trans` | the transition of a meaning |
| `staticText` | `Str -> Meaning` | static text: a constant view and the identity transition |
| `omitted` | `Meaning` | the omitted node: the view `tEnd` and the identity transition (Partial programs) |
| `isolate` | `Meaning -> Meaning` | error isolation (section 7.4) |
| `interpolate` | `Expr -> Meaning` | expression interpolation {e} |
| `joinTree` | `UITree -> UITree -> UITree` | the nodes of a tree, then the nodes of another |
| `beside` | `Meaning -> Meaning -> Meaning` | two meanings side by side: the views join, the transitions compose |
| `besideAll` | `List Meaning -> Meaning` | a list of meanings side by side (the children of a tag) |
| `component` | `Name -> List (Prod Name Expr) -> List Trans -> Meaning -> Meaning` | the component tag: props, handlers and kids |
| `initCell` | `Key -> Value -> State -> State` | the state with a fresh cell |
| `useState` | `Key -> Value -> Meaning -> Meaning` | the state declaration |
| `byState` | `(State -> Meaning) -> Meaning` | a meaning that the current state selects |
| `derive` | `Expr -> (Value -> Meaning) -> Meaning` | a derived binding |
| `conditional` | `(State -> Flag) -> Meaning -> Meaning -> Meaning` | the conditional |
| `loopOver` | `(State -> List Value) -> (Value -> Meaning) -> Meaning` | the loop |

The primitive `recover A d x : A` comes from the front end (slice D4,
section 7.4).

The rules of the design and their tests (equality definitions):

| Rule (design section) | Test |
|---|---|
| state operations (Semantic domain) | `examples/state.lang`: `keyEqSame`, `keyEqOther`, `lookupEmpty`, `lookupSet`, `setPresent`, `setAbsent`, `setTwice`, `setTwiceEmpty`, `getOrAbsent`, `getOrPresent`, `evTargetOf`, `evPayloadOf` |
| the transition monoid (Type-class morphisms) | `examples/trans.lang`: `leftUnitT`, `rightUnitT`, `assocT`, `leftUnitAt`, `rightUnitAt`, `assocAt`, `allNil`, `allTwo`, `setWrites` |
| static text | `examples/meaning.lang`: `textView`, `textStep`, `textAt` |
| expression interpolation | `examples/meaning.lang`: `interpView`, `interpStep`, `interpAt` |
| component tag | `examples/meaning.lang`: `tagView`, `tagStep`, `tagAt`, `tagStepAt` |
| state declaration | `examples/meaning.lang`: `declFresh`, `declKept`, `declStep`, `setterHit`, `setterMiss` |
| derived binding | `examples/meaning.lang`: `deriveView`, `deriveAt` |
| conditional | `examples/meaning.lang`: `condYes`, `condNoStep`, `condAt` |
| loop | `examples/meaning.lang`: `loopNone`, `loopView`, `loopStep` |
| event handlers | `examples/meaning.lang`: `handlerView`, `handlerAt` |
| nesting of children is a morphism | `examples/tcm.lang`: `nesting`, `nestingAt` |
| pure expressions lift pointwise | `examples/tcm.lang`: `litView`, `liftId`, `liftCompose`, `liftLit`, `lift2Lit`, `liftView`, `liftViewAt` |
| the meaning keeps the monoid operation | `examples/tcm.lang`: `keepsThen`, `keepsUnit`, `besideLeftView`, `besideLeftStep`, `besideRightStep`, `besideAssocStep`, `besideRightAt`, `besideAssocAt`, `besideStepAt` |

## 7. Target and instance encoding

### 7.1 Commands

```
langc check PROG [--partial]
langc eval PROG NAME [ARGS...]
langc build PROG [--partial] [-o OUT]
langc build PROG [--partial] [--js OUT.js [--selftest]] [--json OUT.json]
```

The exit status is 0 when the command succeeds, 1 when the program is
refused, and 2 for a usage or file error (`USAGE`). `--js` and `--json` need
two different files. `--selftest` needs `--js`.
An evaluated Nat entry trap prints `trap` and an `EVAL_OVERFLOW` diagnostic
while returning 0 (S3).

### 7.2 Instances and encoding

An instance is a top-level definition whose type is a data type (F14).
Functions, types, families and equality proofs are not instances. A build
refuses a function in an instance value (`JSON_VALUE`,
`test/emit/function.lang`). `docs/host/README.md` gives the JSON form of each
type (section The JSON document) and the JS form (section The JS module). In
the JS module:

| Type | JS value |
|---|---|
| `Nat` | a `BigInt` |
| `Flag` | a boolean |
| `Str` | a string without unpaired surrogates |
| `List A` | a dense array |
| a family value | an object as `encode` writes it, for example `{ tag: 'vNat', valNat: 2n }` |
| a function | a closure |
| a type, a proof | erased |

`encode(document)` in the module gives the JSON document byte for byte.
Tests: line `json` of `make check` (17 goldens in `test/json/`) and line `js`
(each module runs in Node, and `encode(document)` equals the golden).

### 7.3 App programs (slice D3)

| ID | Rule | Test |
|---|---|---|
| A1 | A program that defines `app : Meaning` is an app program. It must also define `init : State` and `events : List Key`. The three names are fixed. A program without `app` builds as before. | `examples/seats.lang`; `test/json/seats.json` |
| A2 | `langc build` refuses with `BUILD_APP` when `init` or `events` is missing, when `app`, `init` or `events` has another type, when a key has two entries in `init`, or when a key is listed twice in `events`. | `test/emit/app-no-init.lang`, `test/emit/app-no-events.lang`, `test/emit/app-type.lang`, `test/emit/app-init-type.lang`, `test/emit/app-dup-init.lang`, `test/emit/app-dup-events.lang` |
| A3 | The JSON document starts with the manifest: `cells` (the `init` entries in order, with the type of each value), `events` (the key names), `components` (the distinct component names of the tree in pre-order) and `tree` (`viewOf app init`). Then the instances follow. `app` is not an instance. | `test/app-regressions.js` row `manifest matches the C writer`; `examples/seats.lang`: `treeAtInit` |
| A4 | The JS module exports `init`, `view(state)` and `step(event, state)` over plain data in the JSON form. `view` gives the tree of a state, and `step` the next state. | `test/app-regressions.js` rows `init and view match the C evaluator`, `event sequence matches the C evaluator`; `examples/seats.lang`: `afterMore`, `afterSeq`, `viewAfterOpen`, `seatsAfter` |
| A5 | `onTarget k t` preserves its input state when the event target differs from `k`. `setter` uses this guard. A custom transition or `useState` can change the state even for an unknown target. | `examples/meaning.lang`: `setterMiss`; `test/app-regressions.js` row `an unknown event preserves the state`; `examples/seats.lang`: `afterUnknown` |
| A6 | A wrong argument to `view` or `step` throws a `TypeError`. This includes a malformed value, a sparse array and a string with an unpaired surrogate. | `test/app-regressions.js` rows `malformed values are refused`, `sparse states are refused by view and step`, `unpaired surrogates are refused in values and keys` |
| A7 | A Unicode scalar value goes through `view` and `step` without change. | `test/app-regressions.js` row `Unicode scalar values survive a round trip` |

### 7.4 Error isolation (slice D4)

| ID | Rule | Test |
|---|---|---|
| E1 | `recover A d x` gives `d` when `x` holds a Nat overflow, also in a nested data field. Otherwise it gives `x`. | `test/app/recovery.lang`: `caught`, `clean`, `nested`, `hidden` |
| E2 | `recover` does not call a function that the value holds. During checking, it stays stuck when a free variable prevents the trap check. | `test/app/recovery.lang`: `neutralFirst`, `neutralLast`, `delayed`, `partial` |
| E3 | `isolate m` applies `recover` to each view and transition result. A failed view gives `tEnd`. A failed transition gives its input state. When `m` has no trap at a closed state and a closed event, `isolate m` gives the same view and the same step as `m` (closed pointwise). | `test/app/recovery.lang`: `isolateUnit`, `isolateAgain`, `viewAt`, `stepAt`; `examples/guard.lang`: `stepAtInit`, `isolateFieldView`, `isolateFieldStep`, `isolateRawView`, `isolateRawStep` |
| E4 | `interpolate`, `component` and `byState` apply `isolate`. `derive`, `conditional` and `loopOver` get it from `byState`. The siblings of an omitted node stay. | `test/app-regressions.js` rows `an overflowing interpolation omits its node and preserves its sibling`, `an isolated transition overflow preserves the input state`; `examples/guard.lang`: `viewAtInit`, `countFromInit`; `test/json/guard.json` (the manifest tree has no node for the trap) |
| E5 | An omitted node can come back at a later state when its expression succeeds. | `test/app-regressions.js` row `the omitted interpolation returns after a valid setter event`; `test/app/recovery.lang`: `afterAt`, `treeAfterAt`; `examples/guard.lang`: `viewAfterSet`, `countAfter` |
| E6 | A raw meaning and an entry function still report an overflow that no `recover` or `isolate` handles. | `test/app/unisolated.lang`; `test/app-regressions.js` row `an unisolated transition overflow throws RangeError` |

### 7.5 Partial programs (slice D5)

| ID | Rule | Test |
|---|---|---|
| P1 | `langc check PROG --partial` and `langc build PROG --partial` read a program that the end of the file cuts. Strict mode is the default and refuses a cut program. `eval` does not take `--partial`. The domain file is always strict. | `test/app-regressions.js` row `strict mode still refuses a cut source`; `test/backend-regressions.c`: `partial_application_tests`, case `natAdd 2 ` without `--partial` (`TYPE_ARITY`) |
| P2 | `--partial` drops a string literal that the end of the file cuts. | `test/app-regressions.js` row `a cut inside a string literal drops the literal` |
| P3 | `--partial` drops a last token that touches the end of the file and can still grow: a name, a keyword, a Nat literal, `:`, or a single `-` or `=`. | `test/app-regressions.js` row `a cut inside a token drops the token` |
| P4 | A lexical error, and a parse error before the end of the file, stay refusals. | `test/app-regressions.js` row `strict mode still refuses a cut source` (a stray `)`) |
| P5 | After a parse error at the end of the file, `--partial` keeps the complete items. It also keeps the cut item when that item parses with each open `(` closed by a `)`. Only `(` is an open form. | `test/app-regressions.js` rows `a cut inside an open form that closes keeps the item`, `a cut at a definition boundary is its own complete prefix` |
| P6 | When the kept cut item does not check, `--partial` drops it and checks the other items again. A final application that still needs arguments is also a cut item. Errors in earlier definitions and in completed arguments stay refusals. | `test/app-regressions.js` row `a cut inside an open form that does not check drops the item`; `test/backend-regressions.c`: `partial_application_tests` (19 cases) |
| P7 | A complete program that ends with a newline gives the same JSON document and the same JS module with and without `--partial`. | `test/app-regressions.js` row `a complete source builds the same with and without --partial` |
| P8 | The tested prefix chain preserves emitted instances and exported data values, and retains earlier entry names. General monotonicity over all definitions is a known limit (O2; design, section Partial programs and streaming). | `test/app-regressions.js` row `partial meaning is monotone in the prefix order` |
| P9 | `check --partial` does not refuse a byte prefix of a program that checks. | `test/backend-regressions.c`: `partial_prefix_tests` (each byte prefix of `test/app/program.lang`) |

## 8. Host and target

The host is TinyCC 0.9.28rc (AArch64 Darwin). Node 23.10.0 runs the JS
modules (`probe/CAPABILITY.md`). The kit is `hosts/tcc-js` of lang-template.
These probe sections set the design: 3 (recursion and totality), 4 (Nat),
5 (how a result leaves the host) and 7 (parametric families).

The gate checks the output in three ways. `test/gate.sh` builds each example
with `--js --selftest`, runs the module in Node and compares
`encode(document)` with the JSON golden. `make check-clang` compiles the C
source with `cc -Wall -Wextra -Wswitch-enum -Werror`. `make doc-check` scans
the documents. `docs/VALIDATION.md` gives the counts.

## 9. Open items

- O1. Rules P1 to P9 come from the design calls of slice D5 and from the
  changes in commit 019afee. Ruled: keep as built (E0, 2026-10-10).
- O5. The isolation design calls of slice D4: direct or inherited
  isolation, and the rewrites of 10 laws. Ruled: keep as built (E0,
  2026-10-10).
- O6. The design calls of slices D2 and D3. Ruled: keep as built (E0,
  2026-10-10).
- O7. The JS evaluator has no fuel limit, and full JS parity and linked
  cells are not built (slice K2). Deferred to a lang-template kit series.

Slice E1 closed O4: `examples/guard.lang` gives the law "isolate keeps a
trap-free closed meaning" for the view and the step (rule E3).

Known limits. Slice E0 (2026-10-10) dropped these items:

- O2. The test of P8 uses one program (`test/app/program.lang`). A complete
  type definition can change when an arrow or an application extends it.
  For example, appending `-> Nat` after `def Alias : Type 0 := Nat` changes
  `Alias` from `Nat` to `Nat -> Nat`, even across a newline. The existing
  test compares emitted data and entry names, rather than erased types or
  entry behavior. Types erase, so this example leaves runtime data unchanged.
- O3. The checker has no reduction `recover A d d = d`.

## 10. Milestones

The slices of `docs/PLAN.md` (section 3) replace the milestones M0 to M3.
`docs/STATUS.md` gives each slice and its commit.

Status 2026-10-10: slices K0 to K3, D0 to D6 and E1 are committed. Slice E2,
the last slice, moves the temporary directory of `test/backend-regressions.c`
under `build/`.

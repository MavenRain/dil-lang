# dil-lang plan

## 1. Rulings

- The language uses only the base types and operations of the design in
  `design/DESIGN.md`. The type formers of lang-template (products, sums,
  options, lists, fold, functions, dependent types, equality and the rest)
  stay. A program cannot add a type former.
- Host: TinyCC. Target: JavaScript and JSON.
- Route: lang-template gets a generic host kit `hosts/tcc-js`, a fork of
  the `hosts/tcc-json` front end with a JavaScript writer. The kit has no
  dil-lang content. dil-lang is then made with
  `bin/new-lang.sh dil-lang tcc-js`.
- Output: `NAME.js` exports `init`, `view(state)` and `step(event, state)`.
  `view` returns the interface tree as plain data. `NAME.json` holds the
  state cells (key, type, initial value), the event names, the component
  names and the tree at the initial state.
- Numbers: unsigned 64-bit. The JavaScript output uses `BigInt` and stops
  with an error on overflow, the same as the C evaluator.
- Syntax: the expression syntax of the kit only. There is no tag syntax.
- The design rules for incomplete programs (a partial program means its
  longest complete prefix, with open forms closed) and for failed
  sub-expressions (the failed node is omitted, the tree around it does not
  change) are in scope, as slices D4 and D5.

## 2. Design choices

Kit (generic):

- `Str` is a primitive, like `Nat` and `Flag`. A string literal is
  `"..."` with the escapes `\"`, `\\` and `\n`. The operations are
  `strEq : Str -> Str -> Flag` and `showNat : Nat -> Str`. There are no
  others.
- The JavaScript writer makes one ES module export for each program
  definition. Functions become closures. Values have the same shapes as the
  JSON encoding of the kit. Types and equality proofs are erased.
- `langc build PROG --js OUT.js --json OUT.json`. A refused build writes
  neither file.
- Tests are written in the language. Each equality definition in a program
  is a test: `langc check` proves it, and `langc build --selftest` adds
  module code that evaluates both sides in JavaScript and stops with an
  error on a mismatch.

dil-lang domain:

| Type | Definition |
|---|---|
| `Key` | `key (keyName : Str)` |
| `Name` | `name (nameText : Str)` |
| `Value` | `vNat (n : Nat) \| vFlag (b : Flag) \| vText (s : Str)` |
| `State` | `List (Prod Key Value)`, a partial map from keys to values |
| `Event` | `event (evTarget : Key) (evPayload : Value)` |
| `Trans` | `Event -> State -> State`, a monoid under `idT` and `thenT` |
| `Prop` | `pval (pName : Key) (pVal : Value) \| pon (pName : Key) (pTarget : Key)` |
| `UITree` | `text (s : Str) \| comp (c : Name) (props : List Prop) (kids : List UITree)` |
| `Iface` | `Prod (State -> UITree) Trans`, the meaning of a program |

- State operations: `lookup`, `getOr`, `set` (the update `s[k := v]`).
- Meaning operations, one for each rule of the design: static text,
  interpolation, component tag, state cell with its setter, event handler.
  Conditionals and loops use the sum and list formers.
- A handler appears in the view only as its target key. Its function is in
  the transition, as the design requires.
- A program defines `app : Iface` and `init : State`. The build writes
  `view` (the first part of `app`), `step` (the second part) and `init`.
- External data bindings in the environment are not in scope.
- Keyed state across recompiles is an operational matter. The manifest
  gives the keys, so a runtime can keep state.

## 3. Slices

| Slice | Repository | Content |
|---|---|---|
| K0 | lang-template | Copy `hosts/tcc-json` to `hosts/tcc-js`. Gate counts equal those of tcc-json. |
| K1 | lang-template | The `Str` primitive. |
| K2 | lang-template | The JavaScript writer, `--js`, `--json`, `--selftest`. |
| K3 | lang-template | Kit documents and root integration (`bin/new-lang.sh` and the root gate). |
| D0 | dil-lang | Generate the language from the kit and merge it into this repository. |
| D1 | dil-lang | Domain types and the state, event and transition operations. |
| D2 | dil-lang | Meaning operations, a test for each design rule, and the morphism laws as proofs. |
| D3 | dil-lang | The target: `init`, `view`, `step` and the manifest. Event sequence tests on both backends. |
| D4 | dil-lang | Failed sub-expressions denote the omitted node. |
| D5 | dil-lang | Partial programs denote their longest complete prefix. |
| D6 | dil-lang | `SPEC.md`, status, validation and the final gate. |

# dil-lang status

The build implements slices K0 to K3 (the kit `hosts/tcc-js` in
lang-template) and slices D0 to D6 (this repository). The D series is
complete. Section Open gives the design calls that the maintainer has not
ruled and the items that are not built. `SPEC.md` gives each rule and its
test.

## Slices

| Slice | What is built | Commit |
|---|---|---|
| K0 | `hosts/tcc-js`, a copy of `hosts/tcc-json` | lang-template `fe07c5e` |
| K1 | The `Str` primitive: string literals, `strEq` and `showNat` | lang-template `fe07c5e` |
| K2 | The JS writer and the flags `--js`, `--json` and `--selftest` | lang-template `fe07c5e` |
| K3 | The kit documents, `bin/new-lang.sh` and the root gate | lang-template `fe07c5e` |
| (plan) | `design/DESIGN.md` and `docs/PLAN.md` | `1ac6787` |
| D0 | The compiler from the kit, with `make check` and `make doc-check` | `4fa2ced` |
| D1 | The state and event types, the state, event and transition operations, and the `natSub` refusal | `080ff5b` |
| D2 | `UITree`, `Expr`, `Meaning`, the meaning operations, a test for each design rule, and the morphism laws as proofs | `71b80a3` |
| D3 | App programs (`app`, `init`, `events`), the manifest, and the JS interface `init`, `view` and `step` | `1f0ab42` |
| D4 | `recover` and `isolate`, with isolation in `interpolate`, `component` and `byState` | `eb8fc1d` |
| D5 | `--partial` on `check` and `build` | `019afee` |
| D6 | `SPEC.md`, this file, `docs/VALIDATION.md`, `README.md` and the final gate | the D6 commit |

## Open

Design calls that the maintainer has not ruled:

1. Slice D5: the partial-program design calls DC-P1 to DC-P8 and the
   interpretations I1 to I6 (`SPEC.md`, rules P1 to P9).
2. Slice D4: the design calls DC1 to DC5, direct or inherited isolation, and
   the rewrites of 10 laws.
3. Slices D2 and D3: six design calls each.

Items that are not built:

4. A reduction `recover A d d = d` in the checker.
5. A definition for the law "isolate keeps a trap-free closed meaning".
   `isolateUnit` and `isolateAgain` test the view only.
6. A test of P8 on more than one program, and a test of the type-definition
   case (`SPEC.md`, O2).
7. The D4 app example is `test/app/recovery.lang` under the Node harness.
   It is not an example with a JSON golden and `expect.txt` rows.
8. From the kit slices: the shared front files of K2, JS fuel, JS parity,
   linked cells, `test/regressions.js` line 62, `src/json.c` line 1, and the
   root `make check` for the mech host.
9. From slice D0: `test/backend-regressions.c` writes under `/tmp`;
   `.gitignore` lists `build/` two times; the probe gaps; the template dash
   scan and the doc-check skip list.
10. The D3 commit holds 5 paths that the D3 record does not list.
11. The repository visibility.

Closed by commit 019afee: `check --partial` now has a test
(`partial_prefix_tests` in `test/backend-regressions.c`).

## Known limits

- `Nat` is unsigned 64-bit. A result above 18446744073709551615 is a trap.
- `unfold` stops after a Nat step limit.
- `Type 0` is not part of `Type 1` (no cumulativity).
- A `UITree` node holds its next sibling, because the kit refuses a
  `List UITree` field and mutual families.
- There is no tag syntax, and the environment has no external data bindings.
- The only Str operations are `strEq` and `showNat`.
- With `--partial`, only `(` is an open form. A complete type definition can
  change when a longer prefix extends it.
- The parser has a depth limit (`PARSE_DEPTH`).
- In the sandbox, `build/backend-regressions` exits 2 with no output. Run
  `make check` outside the sandbox.

## Internal boundaries

`src/front/` holds the front end. `lexer.c` makes the tokens, `parser.c`
makes the items, and `front.c` loads the domain file and the program. Each of
the three has a partial mode for `--partial`. `check.c` checks the types and
gives the refusals and primitive types. `eval.c` evaluates and holds the
primitive names and arities, including `recover`. `core.h` declares the
primitive operations. `base.c` provides arena allocation and diagnostics.

`src/json.c` writes the JSON document and `src/js.c` writes the ES module.
`src/main.c` reads the commands and the flags. `build/domain.c` holds the
domain from `domain/domain.lang`.

`test/backend-regressions.c` tests the C units and the partial modes.
`test/gate.sh` runs the examples, the `expect.txt` tables of `test/parse/`,
`test/check/`, `test/eval/` and `test/emit/`, the JSON goldens and the JS
modules. `test/regressions.js`, `test/printer-regressions.js` and
`test/app-regressions.js` run in Node.

A program cannot declare a family, use `natSub` or declare a definition with
a core name. `domain/domain.lang` declares the domain families and type aliases;
the front end provides builtin types such as `Nat`, `Flag` and `Str`.

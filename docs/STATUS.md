# dil-lang status

The build implements slices K0 to K3 (the kit `hosts/tcc-js` in
lang-template) and slices D0 to D6, E1 and E2 (this repository). Section Open
gives each open item and its ruling. `SPEC.md` gives each rule and its test.

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
| D6 | `SPEC.md`, this file, `docs/VALIDATION.md`, `README.md` and the final gate | `52192e3` |
| E1 | The isolate step law and the app example `examples/guard.lang`, with `test/json/guard.json` and 5 rows in `test/eval/expect.txt` | `451f05e` |
| E2 | The temporary directory of `test/backend-regressions.c` moves from `/tmp` to `build/`, so `make check` runs in the sandbox | the E2 commit |

## Open

The maintainer ruled each item in slice E0 (2026-10-10):

1. Slice D5: the partial-program design calls DC-P1 to DC-P8 and the
   interpretations I1 to I6 (`SPEC.md`, rules P1 to P9). Ruled: keep as
   built.
2. Slice D4: the design calls DC1 to DC5, direct or inherited isolation, and
   the rewrites of 10 laws. Ruled: keep as built.
3. Slices D2 and D3: six design calls each. Ruled: keep as built.
4. A reduction `recover A d d = d` in the checker. Dropped (section Known
   limits).
5. A definition for the law "isolate keeps a trap-free closed meaning".
   Closed by slice E1: `examples/guard.lang` tests the view and the step.
6. A test of P8 on more than one program, and a test of the type-definition
   case (`SPEC.md`, O2). Dropped (section Known limits).
7. An app example for slice D4 with a JSON golden and `expect.txt` rows.
   Closed by slice E1: `examples/guard.lang` and `test/json/guard.json`.
8. From the kit slices: the shared front files of K2, JS fuel, JS parity,
   linked cells, `test/regressions.js` line 62, `src/json.c` line 1, the
   root `make check` for the mech host, the template dash scan and the
   doc-check skip list. Deferred to a lang-template kit series.
9. From slice D0: `test/backend-regressions.c` wrote under `/tmp`. Closed
   by slice E2: the temporary directory is under `build/`, and `.gitignore`
   lists `build/` one time.
   The probe gaps are recorded only.
10. The D3 commit holds 5 paths that the D3 record does not list. Recorded
    only.
11. The repository visibility. The maintainer changes it to public.

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
- The checker has no reduction `recover A d d = d` (`SPEC.md`, O3).
- The test of P8 uses one program, and no test covers the type-definition
  case (`SPEC.md`, O2).

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

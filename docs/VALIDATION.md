# Validation

Date: 2026-10-10. TinyCC: 0.9.28rc (mob@0fb54300, AArch64 Darwin). Node:
23.10.0. Host executable: `build/langc` at commit 019afee. Slices D6 and E1
change no source file.

## Gate commands

Run each command outside the sandbox. In the sandbox,
`build/backend-regressions` exits 2 with no output, and `make check` exits 2.

`make check` exits 0. It builds `build/langc` and
`build/backend-regressions` with `tcc -Werror`, then gives these lines:

| Line | Count | What the count shows |
|---|---|---|
| backend regressions | 174 checked, 0 failed | The C tests in `test/backend-regressions.c` pass. They include the 19 cases of `partial_application_tests` and the byte prefixes of `partial_prefix_tests`. |
| examples | 17 checked, 0 failed | Each file in `examples/` checks, and each equality definition in it is proved. |
| parse refusals | 12 checked | Each row of `test/parse/expect.txt` gives its refusal code. |
| check refusals | 28 checked | Each row of `test/check/expect.txt` gives its refusal code. |
| evals | 35 checked | Each row of `test/eval/expect.txt` gives its value or its trap. |
| json | 17 builds checked | Each example builds a JSON document equal to its golden in `test/json/`. |
| build refusals | 8 checked | Each row of `test/emit/expect.txt` gives its refusal code and emits no stdout. A separate `trap.lang` case checks that `-o` leaves no file. |
| js | 17 modules checked | Each example builds with `--js --selftest`, runs in Node, and `encode(document)` equals its golden. |
| js evals | 19 checked, 16 not entries | Each eval row whose name is an entry gives the same value in the JS module. 16 rows are not entries. |
| js build refusals | 16 checked | The build refusals also hold for the JS output. |
| gate | 0 failures | `test/gate.sh` has no failure. |
| regressions | 6 checked, 0 failed | `test/regressions.js`: type safety, type metadata, binder scope and domain declarations. |
| printer regressions | 2 checked | `test/printer-regressions.js`. |
| app regressions | 20 checked, 0 failed | `test/app-regressions.js`: 8 rows for slice D3, 4 for D4 and 8 for D5. |

`make check-clang` exits 0. It compiles the C source and
`test/backend-regressions.c` with `cc -Wall -Wextra -Wswitch-enum -Werror`.

`make doc-check` exits 0 with 0 problems. It scans the documents.

## Slices

Slice D1 adds the domain types and operations. The backend regressions go
from 46 to 78.

Slice D2 adds the meaning operations. `examples/meaning.lang` has a test for
each design rule, and `examples/tcm.lang` gives the morphism laws as proofs.

Slice D3 adds app programs. `examples/seats.lang` and `test/json/seats.json`
check the manifest. The 6 `BUILD_APP` rows of `test/emit/expect.txt` check
the refusals. `test/app-regressions.js` gets 8 rows.

Slice D4 adds `recover` and `isolate`. `test/app/recovery.lang` holds the
guest proofs, and `test/app/unisolated.lang` tests an unhandled overflow.
The app regressions go from 8 to 12.

Slice D5 adds `--partial`. The app regressions go from 12 to 20, and the
backend regressions from 78 to 174. Strict mode stays the default, so no
example, golden, `expect.txt` row or refusal changes.

Slice D6 adds the documents. It changes no count.

Slice E1 adds `examples/guard.lang`, an app example for slice D4. Its count
cell starts at 18446744073709551615, so the interpolation of count + 1 traps
at init. The proofs show three items: the tree at init omits that node and
keeps its siblings; the node comes back after a set event; a bump step that
overflows keeps the state. Four more proofs give the law "isolate keeps a
trap-free closed meaning" for the view and the step. In
`test/json/guard.json`, the manifest tree has no node for the trap.
`test/eval/expect.txt` gets 5 rows, and 3 of them name an entry. The
examples, json and js lines go from 16 to 17, and the evals from 30 to 35.
The js evals go from 16 to 19 checked, and from 14 to 16 not entries.
Slice E1 changes no C file.

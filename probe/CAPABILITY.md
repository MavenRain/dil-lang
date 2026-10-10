# Host capability probe: tcc-js

Date: 2026-10-09. Host tree: `/Users/oobi/Documents/dil-lang`, generated
from lang-template at fe07c5e with `bin/new-lang.sh dil-lang tcc-js`, read
only. Driver: `build/langc` (built 2026-10-09 with `make build/langc`,
TinyCC 0.9.28rc AArch64 Darwin; Node 23.10.0 runs the modules). Other
sessions used the machine during the probe. Thus each wall time is an
upper bound.

To run the probe again: section 6 gives the program generator and the
guard command for each step; sections 1 to 3, 7 and 10 use the files
under `test/` and `examples/`. Each cost step runs below `probe/guard.py`,
which stops a command above 3500 MB resident memory or above 120 s. The
probe writes only below a new temporary directory (`$TMPDIR/d0-cost`), and
removes it. The largest process in the probe used 49.1 MB (`node`, N = 1000).

The domain is still the kit sample (`domain/domain.lang`). The dil-lang
domain replaces it from slice D1 (`docs/PLAN.md`).

## Questions

Each host must answer these questions. Each answer is one numbered section
below. An answer sets the status of a former in `formers/tcc-js.md`.

| Question | Section | Formers |
|---|---|---|
| Which commands check a file, print a normal form, build and run? | 1 | all |
| Does the host take several files, or must the driver join them? | 2 | all |
| Which recursion does the host accept, and how does it refuse the rest? | 3 | F6, F7 |
| Which `Nat` primitives exist? Does `Nat` have an eliminator? | 4 | F6, F7 |
| How does a result leave the host? | 5 | target |
| What do check and build cost (time and resident memory) at N = 100 and N = 1000? | 6 | all |
| Can a family take a parameter? Can a constructor of such a family appear in a term? | 7 | F4, F12, F15 |
| Can a program project a Sigma? Does the kernel accept Sigma eta? | 8 | F11 |
| How does a program use equality: transport, cong, an index type? | 9 | F12, F13 |
| How does the host report an axiom? | 10 | R2 |

## 1. Commands

| Job | Command | Result |
| --- | --- | --- |
| Check | `langc check PROG` | Prints `ok`, exit 0. A refusal writes one line `langc: CODE: NAME: message` to stderr, exit 1. |
| Print normal form | `langc eval PROG NAME [ARGS...]` | Applies `NAME` to the literal arguments and prints the normal form. A Nat overflow prints `trap`. |
| Build | `langc build PROG [-o OUT]` and `langc build PROG [--js OUT.js [--selftest]] [--json OUT.json]` | One JSON document to stdout or `OUT`; or the ES module, the JSON document, or both. |
| Run | `node OUT.js` | Exit 0 and no output. With `--selftest`, exit 1 and `Error('selftest: NAME: the two sides differ')` on a mismatch. |

- Source: `docs/host/README.md` (Commands, The JS module). Exit 2 is a
  usage or IO error (`langc: USAGE: ...`).
- Each command takes one `PROG` (section 2). `-o` does not mix with
  `--js` or `--json`. `--selftest` needs `--js`. The JS and JSON
  destinations must be different files.
- A build prepares each artifact in memory before it writes. A refused
  build writes no file (`docs/host/CAPABILITY.md`, Output).
- Not probed in D0: `langc eval` and `--selftest` on the section 6
  programs. `make check` runs both on the kit examples (`test/gate.sh`).

## 2. Several files

The host takes one file. The probe ran `langc check examples/sigma.lang
examples/equality.lang`: `langc: USAGE: -: too many arguments`, exit 2.
The domain is not a second file: `gen/embed.c` embeds `domain/domain.lang`
in the executable at build time (`docs/host/README.md`, The domain).

A driver that joins files must put each definition after the definitions
that it uses: a definition can use only earlier ones (rule R3, section 3).
A source file is at most 1 MiB (`src/front/front.h:7`).

## 3. Recursion and totality

The host accepts no general recursion. It accepts structural recursion
through `fold` over `Nat`, `List` and each domain family, and `unfold` into
`List` with a `Nat` step limit (formers F6 and F7 in `formers/tcc-js.md`;
`examples/algebra.lang`). The refusals (rule R3, `test/check/expect.txt`):

- `test/check/rec.lang`: `langc: REFUSE_REC: loop: def rec is refused
  (rule R3); use fold or unfold`
- `test/check/self.lang`: `langc: REFUSE_REC: loop: loop is this
  definition or a later one; a definition can use only earlier ones (rule
  R3)`
- `test/check/later.lang`: `langc: REFUSE_REC: early: late is this
  definition or a later one; a definition can use only earlier ones (rule
  R3)`

The checker and the evaluator stop at a depth of 2000, the parser at a
nesting of 1000, and the evaluator fuel is 20,000,000 steps for each
instance (`docs/host/CAPABILITY.md`, Limits).

## 4. Nat

- `Nat` is an unsigned 64-bit integer. The lexer refuses a literal above
  18446744073709551615 (`test/parse/nat-range.lang`).
- The primitives are `natAdd`, `natMul`, `natSub`, `natEq` and `natLe`
  (`src/front/check.c`). `natAdd` and `natMul` give a trap on overflow.
  `natSub` below zero gives 0 (truncated) (`docs/host/CAPABILITY.md`,
  Values).
- The eliminator is `fold`: a definition recurses on a `Nat` with `fold`
  (`examples/algebra.lang:4`, `square`).
- The largest value that leaves the host is 18446744073709551615: a decimal
  u64 in JSON, an exact BigInt in JS.
- Ruling R4 (Nat in JS = BigInt, exact u64, throw on overflow): the kit
  meets it. In JS a trap is the value `TRAP`, which stops a computation only
  when the computation needs it. An entry that returns a trap throws
  `RangeError('EVAL_OVERFLOW: NAME: ...')`, and `encode` throws the same
  error (`docs/host/README.md`, The JS module).

## 5. How a result leaves the host

- `langc build PROG [-o OUT]` writes the JSON document to stdout or `OUT`.
  `--json OUT.json` writes the same document.
- `langc build PROG --js OUT.js` writes one ES module. Its exports are
  `document` (the JSON document as a frozen JS object), `encode(v)` (the
  JSON text of a value; `encode(document)` is the same bytes as `--json`)
  and `entries` (one JS function for each entry definition)
  (`docs/host/README.md`, The JS module).
- An entry takes a BigInt for each `Nat` parameter and a boolean for each
  `Flag` parameter. These arguments are the only input path. External
  data bindings in `Env` stay out of scope until a USER ruling (O4).
- The dil-lang outputs in `README.md` (`init`, `view(state)`,
  `step(event, state)` and the static JSON part) are not kit exports. The
  D slices make them (`docs/PLAN.md`).

## 6. Cost

The program has N definitions: `def d0 : Nat := 1`, then `def dK : Nat :=
natAdd d(K-1) 1` for K from 1 to N - 1. The generator is
`awk -v n=N 'BEGIN { printf "def d0 : Nat := 1\n"; for (i = 1; i < n; i++)
printf "def d%d : Nat := natAdd d%d 1\n", i, i-1 }'`. Each step ran as
`python3 -I probe/guard.py --rss-mb 3500 --timeout 120 -- COMMAND`, and the
memory is the guard's `peak_child_mb`. Each step exited 0. The N = 1000
JSON document holds 1000 instances.

| Step | N = 100 | N = 1000 |
| --- | --- | --- |
| Check (`langc check`) | 0.026 s, 2.3 MB | 0.027 s, 6.1 MB |
| Build JSON (`langc build -o`) | 0.027 s, 2.3 MB | 0.027 s, 6.7 MB |
| Build JS (`langc build --js`) | 0.023 s, 2.4 MB | 0.027 s, 7.4 MB |
| Run (`node OUT.js`) | 0.202 s, 40.7 MB | 0.069 s, 49.1 MB |

The N = 100 run is slower than the N = 1000 run. The first Node start was
cold, and other work loaded the machine (see the header).

## 7. Parametric families

A family can take a parameter only in `domain/domain.lang` (rule R1). The
sample has `family Lot (size : Nat) := makeLot (lotPrice : Nat)`
(`domain/domain.lang:8`), and `examples/inventory.lang` uses its
constructor in terms (F15 in `formers/tcc-js.md`). A program cannot
declare a family: `test/check/data.lang` gives `langc: REFUSE_DATA: Coin:
a program cannot declare a family (rule R1); only domain/domain.lang can`.

The parameter of `Lot` is a `Nat` index. D0 did not probe a family with a
`Type` parameter (Open in the D0 record). The dil-lang domain types go in
`domain/domain.lang` from slice D1.

## 8. Sigma projection and eta

`witness` and `payload` check: `examples/sigma.lang` has `def offered :
Nat := witness offer` and `def proof : Eq Nat (witness offer) price :=
payload offer`. A Sigma type is `Sigma (n : Nat) (B)` or `(n : Nat) * B`,
and `pack` makes a value (F11 in `formers/tcc-js.md`).

D0 did not probe a Sigma pattern or Sigma eta (`pack (witness s) (payload
s)` against `s`). The kit forms are `pack`, `witness` and `payload` only.
The nearest kit test, `test/check/bad-eta.lang`, swaps the two parts of a
pair, and `refl` refuses it (`TYPE_REFL`). These two points are Open in
the D0 record.

## 9. Equality

A program writes `Eq A x y` and proves it with `refl`, `symm`, `trans`,
`transport` and `cong` (`examples/equality.lang`; F12 and F13 in
`formers/tcc-js.md`). For example, `transport P same u` moves `u : P
price` to `P cost`. `Eq A` takes each type `A`, so one family serves each
index type. A proof is `null` in JSON and `REFL` in JS.

With `--selftest`, each `Eq`-typed definition is a guest test: `langc
check` proves it, and the module compares its two sides in JS
(`docs/host/README.md`, The JS module). The dil-lang guest tests use this
form (USER rule 2026-10-09: tests in the guest language).

## 10. Axiom detection

No command lists axioms, because the checker refuses each axiom (rule R2).
For `test/check/axiom.lang` (`axiom magic : Nat`), `langc check` gives
`langc: REFUSE_AXIOM: magic: an axiom has no proof (rule R2)`, exit 1. For
a file with no axiom, `langc check examples/sigma.lang` prints `ok`, exit
0. Thus each file that checks has no axiom.

## Consequences for M0

1. The dil-lang driver gives one file to each command, and puts each
   definition after the definitions that it uses (sections 2 and 3).
2. The interface tree, the state and the events are domain families in
   `domain/domain.lang`, and each walk over them is a `fold` or an
   `unfold` (sections 3 and 7).
3. State cells that hold numbers are `Nat`: BigInt in JS, with a
   `RangeError` at an entry or `encode` on overflow (section 4, ruling R4).
4. `init`, `view` and `step` are entries or definitions over the kit
   exports `document`, `encode` and `entries` (section 5). External data
   bindings stay out (O4).
5. The syntax stays the kit expression syntax, with no tag sugar (ruling
   R5). D4 and D5 (an omitted node for a failed sub-expression, the
   longest complete prefix of a partial program) are in scope for their
   slices (ruling R6).
6. Each guest test is an `Eq`-typed definition, run with `--selftest`
   (section 9).

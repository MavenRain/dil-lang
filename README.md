# dil-lang

dil-lang is a small typed language for reactive interfaces. Its core types
and operations come from one denotational design (`design/DESIGN.md`). A
program means a pair: a pure view from state to an interface tree, and a
pure transition from an event and a state to the next state.

The compiler is written in C and builds with TinyCC. It compiles a program
to two files:

- `NAME.js`: an ES module that exports `init`, `view(state)` and
  `step(event, state)`.
- `NAME.json`: the static part. It holds the state cells with their keys and
  initial values, the event names, the component names and the interface tree
  at the initial state.

dil-lang is made from [lang-template](https://github.com/MavenRain/lang-template)
with the TinyCC JavaScript host kit `hosts/tcc-js`.

## Files

- [`SPEC.md`](SPEC.md): the specification. Each rule gives its test.
- `design/DESIGN.md`: the domain design. It is the only source of the core
  types and the core operations.
- `docs/PLAN.md`: the rulings, the design choices and the slices.
- `formers/FORMERS.md`: the type formers. `formers/tcc-js.md` gives their
  status on the tcc-js host.
- `probe/CAPABILITY.md`: the host probe.
- [`docs/STATUS.md`](docs/STATUS.md): each slice, its commit and the open items.
- [`docs/VALIDATION.md`](docs/VALIDATION.md): the gate commands and their counts.
- `docs/host/README.md`: how to build and test the host kit, and how to
  replace the sample domain in `domain/`.

## Status

Slices D0 to D6 are complete. [`docs/STATUS.md`](docs/STATUS.md) gives each
slice and its commit. [`docs/VALIDATION.md`](docs/VALIDATION.md) gives the
gate results.

## Commands

    langc check PROG [--partial]
    langc eval PROG NAME [ARGS...]
    langc build PROG [--partial] [-o OUT]
    langc build PROG [--partial] [--js OUT.js [--selftest]] [--json OUT.json]

- `check` checks the type of each definition and proves each equality
  definition.
- `eval` evaluates the definition NAME with the arguments ARGS and prints
  the value. A Nat entry overflow prints `trap`, reports `EVAL_OVERFLOW`
  on stderr and exits 0.
- `build` writes the output files. `-o OUT` writes the JSON document to OUT.
- `--js OUT.js` writes the ES module. `--json OUT.json` writes the JSON
  document. The two files must be different.
- `--selftest` adds JS checks for definitions whose type is `Eq A a b`
  and whose carrier has no function or type inside. It evaluates both sides
  and stops with an error when they differ. Other carriers are skipped with
  a comment; parameterized proof functions are checked only by the compiler.
  It needs `--js`.
- `--partial` reads a program that the end of the file cuts (section
  Partial programs). `check` and `build` take it. `eval` does not.

The exit status is 0 when the command succeeds, 1 when the program is
refused, and 2 for a usage or file error. A refused build writes no file.
`make check`, `make check-clang` and `make doc-check` are the gate.

## App programs

A program that defines `app : Meaning` is an app program (slice D3). It must
also define `init : State` and `events : List Key`; the three names are fixed.
`langc build` refuses with `BUILD_APP` when init or events is missing, when
app, init or events has another type, when a key has two entries in init (the
cells are a map), or when a key is listed twice in events. A program without
`app` builds as before.

The JSON document of an app program starts with the manifest, then the
instances; `app` itself is not an instance:

    {"dil-lang":1,"cells":[{"key":"seats","type":"Nat","init":2}],"events":["seats"],"components":["Panel"],"tree":{"tag":"comp",...},"instances":[...]}

`cells` lists the init entries in order, each with the type of its value
(`Nat`, `Flag` or `Str`); `events` the key names; `components` the distinct
component names of the tree in pre-order (a node, its kids, then its
siblings); `tree` the value of `viewOf app init`.

The JS module of an app program exports `init`, `view(state)` and
`step(event, state)` over plain data in the JSON form: a Nat is a BigInt, a
Flag a boolean, a Str a string without unpaired surrogates, a list a dense
array, and a family value an object as `encode` writes it (a Value is
`{ tag: 'vNat', valNat: 2n }`, a Key is
`{ keyName: 'seats' }`, an Event is `{ evTarget, evPayload }`). `view` gives
the tree of a state and `step` the next state; a wrong argument throws a
TypeError and an unhandled Nat overflow a RangeError. `document` carries the same
manifest, so `encode(document)` is the C document byte for byte. The example
is `examples/seats.lang`.

## Error isolation

`recover A default value` returns `default` when `value` contains a Nat
overflow, including one in a nested data field. Otherwise it returns the
value. It does not call functions stored in the value. During checking, it
stays stuck when a free variable prevents the trap check.

`isolate meaning` applies recovery to each view and transition result. A
failed view gives `tEnd`. A failed transition returns its input state.
`interpolate`, `component`, `derive`, `conditional`, `loopOver` and `byState`
apply isolation automatically. An omitted node can return at a later state
when its expression succeeds. Siblings outside that node remain present.

Raw meanings and entry functions still report overflows that no `recover`
or `isolate` handles. The guest proofs in `test/app/recovery.lang` test nested
recovery, omitted nodes, restored nodes and unchanged states. The app example
`examples/guard.lang` has a JSON golden; its manifest tree omits the node
that traps at init.

## Partial programs

`langc check PROG --partial` and `langc build PROG --partial ...` read a
program that the end of the file cuts, for example a file that an editor
writes. A partial program means its longest complete prefix, with its open
forms closed. Strict mode is the default. It refuses a cut program as before.
`eval` does not take `--partial`, and the domain file is always strict.

With `--partial`, the front end reads the program as follows:

- It drops a string literal that the end of the file cuts.
- It drops a last token that touches the end of the file and can still grow:
  a name, a keyword, a Nat literal, `:`, or a single `-` or `=`.
- It keeps a program that parses.
- It refuses a lexical error, and a parse error before the end of the file,
  as strict mode does.
- After a parse error at the end of the file, it keeps the complete items.
  It also keeps the cut item when that item parses with each open `(` closed
  by a `)`. Only `(` is an open form.
- When the kept cut item does not check, it drops that item and checks the
  other items again. A final application that still needs arguments, such
  as `def kCount : Key := key `, is also an unfinished item even though it
  parses. It drops that item and checks the complete prefix again. Errors
  in earlier definitions and completed arguments remain refusals.

A complete program that ends with a newline gives the same JSON document and
the same JS module with and without `--partial`.

When a program S is a prefix of a program T, each definition in the meaning
of S is a definition in the meaning of T with the same value. The test
`partial meaning is monotone in the prefix order` in `test/app-regressions.js`
examines this for each line prefix of `test/app/program.lang` and for each
cut that the other partial tests use. One limit is known: a complete type
definition can change when an arrow or an application extends it. Types
erase, so this does not change a runtime value.

## License

MIT (`LICENSE-MIT`) or Apache 2.0 (`LICENSE-APACHE`), at your option.

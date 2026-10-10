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

- `SPEC.md`: the specification. Fill each section.
- `design/DESIGN.md`: the domain design. It is the only source of the core
  types and the core operations.
- `docs/PLAN.md`: the rulings, the design choices and the slices.
- `formers/FORMERS.md`: the type formers. `formers/tcc-js.md` gives their
  status on the tcc-js host.
- `probe/CAPABILITY.md`: the host probe.
- `docs/STATUS.md` and `docs/VALIDATION.md`: the status and the gate results.
- `docs/host/README.md`: how to build and test the host kit, and how to
  replace the sample domain in `domain/`.

## Status

Slice D0. The repository holds the lang-template tcc-js kit with its sample
domain, and `make check` and `make doc-check` pass. The dil-lang domain
replaces the sample domain from slice D1. `docs/PLAN.md` gives the slices.

## License

MIT (`LICENSE-MIT`) or Apache 2.0 (`LICENSE-APACHE`), at your option.

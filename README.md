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

## Status

Planning. The compiler is not in this repository yet. `docs/PLAN.md` gives
the rulings, the design choices and the slices.

## License

MIT (`LICENSE-MIT`) or Apache 2.0 (`LICENSE-APACHE`), at your option.

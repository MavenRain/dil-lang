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
replaces the sample domain from slice D1. Slice D3 adds app programs (the section below). `docs/PLAN.md` gives the slices.

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
TypeError and a Nat overflow a RangeError. `document` carries the same
manifest, so `encode(document)` is the C document byte for byte. The example
is `examples/seats.lang`.

## License

MIT (`LICENSE-MIT`) or Apache 2.0 (`LICENSE-APACHE`), at your option.

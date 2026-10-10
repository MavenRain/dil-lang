**Denotational design of DIL** (in the sense of Conal Elliott)

Following Elliott’s methodology (“the instance’s meaning is the meaning’s instance”), we first fix a simple, pure mathematical model of what a DIL program *means*, then define the compositional meaning function so that it is a type-class morphism for the structures it inhabits. The operational story (server-side compilation to JS + JSON, sandboxed worker, native component catalog, streaming JSON-Patch, auto-closing of incomplete tags, `__dilSafe` error isolation, keyed state across recompiles, etc.) is deliberately excluded; those are *how* questions. The denotation answers only *what*.

### Semantic domain

Let  
- \(\mathbf{Key}\), \(\mathbf{Value}\) be suitable sets (component identifiers and pure values).  
- \(\mathbf{State} = \mathrm{Key} \rightharpoonup \mathbf{Value}\) (partial map; totalized with defaults when convenient).  
- \(\mathbf{Event}\) be the set of user/interaction events (slider changes, button clicks, form submits, …), each carrying a payload.  
- \(\mathbf{UITree}\) be the inductive type of interface trees:  
  \[
  \mathbf{UITree} ::= \mathrm{Text}(\mathrm{String}) \mid \mathrm{Comp}(\mathrm{Name},\ \mathrm{Props},\ [\mathbf{UITree}]) \mid \dots
  \]
  (Props are pure values or event handlers; the precise constructors match the component catalog.)

A DIL program (or fragment) denotes a *reactive interface*:

\[
\mu : \mathrm{DIL} \to \mathbf{Env} \to \bigl( \mathbf{State} \to \mathbf{UITree} \bigr) \times \bigl( \mathbf{Event} \to \mathbf{State} \to \mathbf{State} \bigr)
\]

where \(\mathbf{Env}\) supplies constants, the component catalog, and external data bindings. The first component is the pure view function; the second is the pure state-transition function. Both are total and referentially transparent.

Equivalently, one may view the denotation as a discrete signal transformer (or a coalgebra) from event streams to UI streams, with state as the pure accumulation. The two presentations are isomorphic.

### Compositional meaning function

Selected rules (the rest follow by the same pattern):

- **Markdown / static text**  
  \[
  \mu(\mathit{text}) = \lambda env.\ (\lambda s.\ \mathrm{Text}(\mathit{text}),\ \lambda e\,s.\ s)
  \]
  (constant view, identity transition).

- **Expression interpolation \(\{e\}\)**  
  \[
  \mu(\{e\}) = \lambda env.\ (\lambda s.\ \mathrm{eval}(e,s),\ \lambda e\,s.\ s)
  \]
  where \(\mathrm{eval}\) is the pure semantics of the JavaScript expression (referential transparency is required of the fragment that may appear inside interpolations).

- **Component tag** \(\langle C\ p_i = e_i \rangle\ \mathit{children}\ \langle /C \rangle\)  
  \[
  \begin{align*}
  \mu &= \lambda env.\ \bigl( \lambda s.\ \mathrm{Comp}(C,\ \{p_i \mapsto v_i(s)\},\ \mathit{kids}(s)),\ t \bigr)
  \end{align*}
  \]
  where \(v_i = \pi_1(\mu(e_i))\), \(\mathit{kids}\) is the pointwise application of the children’s view functions, and the transition \(t\) is the monoidal combination of the children’s transitions together with any event handlers supplied in the props (each handler is a pure \(\mathbf{Event} \to \mathbf{State} \to \mathbf{State}\)).

- **State declaration** \(\texttt{\{@body const [v, setv] = DIL.useState(init, \{key: k\})\}}\)  
  Introduces a fresh cell. The view of the remainder is interpreted in the extended state; the setter contributes the pure update  
  \[
  \lambda e\,s.\ s[k \mapsto \mathrm{payload}(e)]
  \]
  (keyed so that the cell survives recompilation). Derived bindings such as \(\texttt{const price = seats * 29}\) are pure functions of the current state.

- **Conditionals / loops**  
  Pure functions of their controlling expressions/lists (which themselves depend only on state). The denotation of a conditional is the denotation of the selected branch; a loop is the finite sequence of denotations of its body under the successive bindings.

- **Event handlers** (e.g. \(\texttt{onChange=\{setSeats\}}\))  
  Contribute pure state transformers to the transition component; they never appear inside the view function.

All rules are compositional: the meaning of a compound phrase is obtained solely by applying the semantic constructors to the meanings of its immediate sub-phrases.

### Type-class morphisms (the TCM principle)

The meaning function is required to be a homomorphism for every type class the constructs inhabit:

- Nesting of children is an applicative (or monoidal) morphism:  
  \[
  \mu(c\ \langle\ \mathit{children}\ \rangle) = c\ \langle\ \mu(\mathit{children})\ \rangle.
  \]
- Pure expressions form the identity morphism on values and lift pointwise through the view function (Functor/Applicative morphism).  
- State transitions form a monoid under sequential composition; \(\mu\) preserves the monoid operation.  
- Consequently every algebraic law that holds for the semantic model holds, for free, for DIL itself. There are no abstraction leaks.

### Partial programs and streaming

A (possibly incomplete) DIL string denotes the same reactive interface as its longest complete prefix, where open tags are treated as auto-closed. Thus \(\mu\) is monotonic with respect to the prefix order on strings. Error-isolated sub-expressions (the operational `__dilSafe`) denote the omitted node; the surrounding tree is unaffected. Both properties are purely denotational.

### Correctness criterion

An implementation (compiler + runtime) is correct precisely when the observable UI trees it produces and the state transitions it performs coincide with \(\mu\). All operational details—token-by-token generation, JSON-Patch streaming, sandboxed workers, native component mapping, keyed persistence across recompiles—are free to vary so long as this equality is maintained.

This design is independent of any particular host language, component catalog, or rendering backend. It can be refined into a fully formal development (e.g., in Lean) by making the semantic domain a co-algebra of a suitable functor and proving the TCM equations, but the essential denotational content is already fixed above.

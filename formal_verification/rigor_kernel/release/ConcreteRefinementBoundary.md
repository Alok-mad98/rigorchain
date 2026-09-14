# Concrete Refinement Boundary

Coq abstract → Coq concrete refinement of the unified kernel
transition. This is still Level 4 *inside the Coq model*.

**Public claim (unchanged):**

Formally verified critical consensus invariants
inside a Coq consensus kernel model,
with explicit extraction and runtime trust boundaries.

## PROVED

Coq abstract → Coq concrete refinement

- `kernel_projection : ConcreteKernelState -> AbstractKernelState`
- `concrete_step_refines_abstract_step`
- `concrete_state_preserves_invariant`

Both states and both steps are Gallina records / relations.
See `Refinement/ConcreteKernel/`.

## NOT CLAIMED

- C++ implementation refinement
- node refinement
- compiler correctness

Also not claimed: binary semantic equivalence, CompCert, full
implementation verification.

`PRODUCTION_REFINEMENT_NOT_CLAIMED=TRUE`
`COMPILER_CORRECTNESS=false`
`BINARY_SEMANTIC_EQUIVALENCE=false`

Open neighbour: OP-R2 (concrete *implementation* refinement) in
`release/OpenProperties.md`.

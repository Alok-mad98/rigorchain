# Extraction Trust Separation

**Public claim (unchanged):**

Formally verified critical consensus invariants
inside a Coq consensus kernel model,
with explicit extraction and runtime trust boundaries.

`EXTRACTION_FORMAL_PROOF=source_identity_only`
`EXTRACTION_EQUIVALENCE_NOT_CLAIMED=TRUE`

## Layers

| Layer | What | Status |
|---|---|---|
| 1 | Coq proof (definitions, theorems, `Print Assumptions` Closed) | **Covered** |
| 2 | Extracted functional program (faithful extract source identity) | **Covered as identity only** |
| 3 | Compiler (OCaml / gcc / future CompCert) | **Not covered** |
| 4 | Machine code / ABI / runtime | **Not covered** |

Currently only **Layer 1 → Layer 2** is in scope, and only as
source identity: extracted names are definitionally the Coq
validators.

Do not claim Layer 2 → Layer 3.
Do not claim Layer 3 → Layer 4.

```
Layer 1  Coq proofs / validators
   ↓     extraction_source_equivalence  (definitional)
Layer 2  extracted OCaml terms
   ✗     compiler correctness not proved
Layer 3  compiler
   ✗     machine / ABI not proved
Layer 4  machine code
```

## Flags (unchanged)

`EXTRACTION_SOURCE_IDENTITY_PROVED=TRUE`
`PRODUCTION_REFINEMENT_NOT_CLAIMED=TRUE`
`COMPILER_CORRECTNESS=false`
`BINARY_SEMANTIC_EQUIVALENCE=false`

See also `evidence/ExtractionBoundary.md` and
`Kernel/ExtractionBoundary.v`.

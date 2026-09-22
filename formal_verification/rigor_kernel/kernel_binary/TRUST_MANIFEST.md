# Binary Trust Manifest

```
ExtractedKernel.ml
        |
        v
compiler (ocamlopt)
        |
        v
binary (rigor-kernel-checker)
```

## Binary

`rigor-kernel-checker`

Built by `kernel_binary/build.sh` from:

- `extraction/ExtractedKernel.ml` (Coq-extracted validators)
- `kernel_binary/main.ml` (JSON vector driver)

## Binary contains

**YES**

- consensus kernel executable (the six `check_*` validators)

**NO**

- node
- network
- wallet
- database
- crypto (no SHA3 / Keccak / Ed25519 implementation)

## Trust note

The binary is a **generated executable artifact for testing**.
It is not a proved executable implementation. Running it on a host
assumes the OCaml extraction mapping, compiler, runtime, OS, and
CPU (see `evidence/TCBInventory.md`). Those assumptions are outside
the proof boundary (`evidence/RefinementBoundary.md`).

`EXTRACTION_FORMAL_PROOF=source_identity_only`
`EXTRACTION_EQUIVALENCE_NOT_CLAIMED=TRUE`
`PRODUCTION_REFINEMENT_NOT_CLAIMED=TRUE`
`BINARY_SEMANTIC_EQUIVALENCE=false`

Not claimed: extraction semantic preservation, binary semantic
equivalence, C++ consensus equivalence, full-node correctness,
network correctness, wallet correctness, cryptographic security.

# Extraction Boundary

**Public claim:** Formally verified critical consensus invariants
inside a Coq consensus kernel model, with explicit extraction and
runtime trust boundaries.

## What is proved (source identity)

The terms named as extraction targets **are** the Coq validators.

| Domain | Coq theorem | Meaning |
|---|---|---|
| UTXO | `utxo_extraction_source_eq` | `extracted_check_utxo = check_utxo = valid_tx_b` |
| ORU | `oru_extraction_source_eq` | `extracted_check_oru = check_oru` |
| RES | `res_extraction_source_eq` | `extracted_check_res = check_res` |
| Script | `script_extraction_source_eq` | `extracted_check_script = check_script` |
| PoW | `pow_extraction_source_eq` | `extracted_check_pow = check_pow` |
| ASERT | `asert_extraction_source_eq` | `extracted_check_asert = check_asert` |

Also proved: `validate_kernel` dispatches to those six functions;
core records are determined by their fields; each checker reflects
the Coq predicate (`*_extract_reflects_*`).
`Print Assumptions extraction_source_equivalence` is Closed.

`EXTRACTION_SOURCE_IDENTITY_PROVED=TRUE`
`EXTRACTION_FORMAL_PROOF=source_identity_only`

## Verified (Coq)

```
Spec
 ↓
Coq definitions
 ↓
Coq proofs
 ↓
Coq validators
        = extraction source terms
```

Faithful extract (`extracted/Extract.v`) does **not** import
`ExtrOcamlZInt`. Executable tests use inductive `Z` / `nat`.

## Not verified / not claimed

```
OCaml compiler
 ↓
runtime
 ↓
binary encoding
```

- OCaml compiler correctness
- Full extraction refinement (CompCert / CertiCoq)
- Binary semantic equivalence of `ocamlopt` output
- The legacy `extraction/` path still uses `ExtrOcamlZInt` (native
  `int`); that path is a separate test artifact, not this proof

`EXTRACTION_EQUIVALENCE_NOT_CLAIMED=TRUE`
`BINARY_SEMANTIC_EQUIVALENCE=false`
`COMPILER_CORRECTNESS=false`

Layering (P0-131~135): only Layer 1 → Layer 2 is covered, as
source identity. Layer 2 → Layer 3 (compiler) is **not** claimed.
See `release/ExtractionBoundary.md`.

Machine-checked: `Kernel/ExtractionBoundary.v`.
`extraction_in_proof_scope = false`
`extraction_full_equivalence = false`
`extraction_source_identity = true`

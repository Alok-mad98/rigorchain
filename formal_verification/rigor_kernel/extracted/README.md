# Faithful kernel extraction

Coq → OCaml of the six boolean validators, with Coq `Z` / `nat`
left as inductives (no `ExtrOcamlZInt`).

```
../scripts/extract_and_test.sh
```

`extracted-kernel-checker` is a generated test artifact.
`EXTRACTION_FORMAL_PROOF=source_identity_only`
`EXTRACTION_EQUIVALENCE_NOT_CLAIMED=TRUE`
`PRODUCTION_REFINEMENT_NOT_CLAIMED=TRUE`
`COMPILER_CORRECTNESS=false`

Differential verification of the comparable fragment:
`../scripts/diff_verify.sh`. Not a production-code refinement proof.

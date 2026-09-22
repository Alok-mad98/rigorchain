# Verified Compiler Path Report

Hand-written C99 fragment rehearsal of `verified_compiler/fragment_checker.c`
on INVALID-001..005 plus five positive smoke cases.

**Not a CompCert refinement of the Coq extract.**
**Not binary semantic equivalence.**

`VERIFIED_COMPILER=ABSENT`
`COMPILER=gcc` (untrusted_host_cc)
`C_FRAGMENT_REHEARSAL=PASS`
`COMPILER_CORRECTNESS=false`
`BINARY_SEMANTIC_EQUIVALENCE=false`
`EXTRACTION_FORMAL_PROOF=source_identity_only`
`EXTRACTION_EQUIVALENCE_NOT_CLAIMED=TRUE`
`PRODUCTION_REFINEMENT_NOT_CLAIMED=TRUE`

Default executable specification remains the faithful OCaml extract
(`scripts/extract_and_test.sh`). This C file is a hand-written C99
subset rehearsal. Coq rejects the same five vectors
(`Kernel/VerifiedCompilerBoundary.v`, `negative_vectors_rejected_in_coq`).

Known limit (live): `asert_llong_overflow` — C_fragment=INVALID,
Coq Z would be VALID. Fixed-width `long long` is not unbounded `Z`.

See `release/VerifiedCompilerPath.md`.

C_FRAGMENT_REHEARSAL=PASS


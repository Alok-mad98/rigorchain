# ASERT Scope

What the Coq ASERT kernel proves, and what it does not.

**Public claim:** Formally verified critical consensus invariants
inside a Coq consensus kernel model, with explicit extraction and
runtime trust boundaries.

## Verified

- Operator split: `exp_q` uses `Z.quot`; `num_shifts` uses `Z.div`
- Negative time-error preservation (`negative_time_error_preserved`)
  over unbounded Coq `Z` (Category A)
- `exp_q_clamped`, `factor_range` (Category A)
- `pow_asert_correctness` is Category B function-ness only
  (`anchor = parent → same next target`)
- `time_error_direction_preserved`: negative / zero / positive
  time-error use `Z.quot` (`Pow/ASERTBoundary.v`)

## Not verified

- Hash implementation
- keccak
- sha256 / SHA3
- miner implementation
- C++ `__int128` / `arith_uint256` equivalence
- Native 63-bit integer behaviour of the extracted binary
  (P0-113 F-01; extraction is outside the proof boundary)

Encoding contract and extraction source-identity are proved
(`Kernel/SerializationBoundary.v`, `Kernel/ExtractionBoundary.v`).
Cryptographic primitives and a full extraction refinement are **not**
proved. Inventory: `Kernel/CryptoAssumptions.v`.

`EXTRACTION_FORMAL_PROOF=source_identity_only`
`PRODUCTION_REFINEMENT_NOT_CLAIMED=TRUE`

See `TheoremClassification.md` and
`proofs/Pow_Correctness/ASERTCorrectness.v`.

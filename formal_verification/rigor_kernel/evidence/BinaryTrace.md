# Binary Trace

Test-harness path from a Coq theorem to a generated executable
artifact. This is **not** a theorem-to-binary refinement and does
**not** claim semantic preservation.

```
Proved:
  Spec → Coq theorem → Coq validator

Generated for testing (unproved):
  extraction → compiler → binary → golden vector
```

`EXTRACTION_FORMAL_PROOF=source_identity_only`
`EXTRACTION_EQUIVALENCE_NOT_CLAIMED=TRUE`
`PRODUCTION_REFINEMENT_NOT_CLAIMED=TRUE`
`BINARY_SEMANTIC_EQUIVALENCE=false`

The binary is a generated executable artifact for testing, not a
proved executable implementation.

## UTXO

```
Theorem (class A):
utxo_soundness

        ↓

Coq validator:
check_utxo / check_utxo_sound

        X  (proof boundary)

Test artifact:
check_utxo() in ExtractedKernel.ml
rigor-kernel-checker
utxo_valid.json / utxo_invalid.json
```

## ORU

```
Theorem (class A):
oru_conservation_extended
no_hidden_inflation

        ↓

Coq validator:
check_oru / check_oru_sound

        X  (proof boundary)

Test artifact:
oru_conservation.json / oru_invalid.json
```

## RES

```
Theorem (class A):
res_issuance_soundness

        ↓

Coq validator:
check_res / check_res_sound

        X  (proof boundary)

Test artifact:
res_schedule.json / res_invalid.json
```

## Script

```
Theorem (class A):
recognizer_pairwise_disjoint

Supporting (class B):
script_correctness

        ↓

Coq validator:
check_script / check_script_sound

        X  (proof boundary)

Test artifact:
script_classification.json
```

## PoW

```
Theorem (class A):
pow_valid_rejects_above_target
compact_decode_lower_bound

Supporting (class B):
pow_valid_deterministic
  (abstract PoW relation only; NOT a digest implementation proof)

        ↓

Coq validator:
check_pow / check_pow_sound
  (integer comparison on a supplied digest; no hash evaluation)

        X  (proof boundary)

Test artifact:
pow_target.json / pow_invalid.json
```

## ASERT

```
Theorem (class A):
negative_time_error_preserved
pow_asert_correctness

        ↓

Coq validator:
check_asert / check_asert_sound

        X  (proof boundary)

Test artifact:
asert_negative_time.json / asert_invalid.json
```

## Bridge (proved, Coq only)

```
validate_kernel input = true
        ↓
kernel_sound
        ↓
consensus_invariant input
```

`Print Assumptions kernel_sound` is Closed under the global context.
That closedness does **not** extend through extraction.

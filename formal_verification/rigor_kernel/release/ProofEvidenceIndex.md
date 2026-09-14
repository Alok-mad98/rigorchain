# Proof Evidence Index

Requirement → Coq theorem → proof file → Coq validator.

The proof chain **stops at the Coq validator**. Binary test vectors
are generated-artifact checks, not a refinement step.

Coverage IDs are P0-106 kernel IDs. This is kernel-model evidence, not
C++ line coverage.

```
Requirement
        ↓
Coq theorem
        ↓
Proof file
        ↓
Coq validator
```

```
        X  proof boundary

Binary test vector   (generated artifact; not proved)
        ↓
Harness result
```

## UTXO

```
K-U-13 (packages K-U-3 / K-U-6 / K-U-7)
        ↓
utxo_soundness
        ↓
proofs/UTXO_Soundness/UTXOSoundness.v
        ↓
Kernel/UTXOValidator.v  check_utxo / check_utxo_sound
```

## ORU

```
K-O-5 / K-O-10
        ↓
oru_conservation_extended
no_hidden_inflation
        ↓
proofs/ORU_Conservation/ORUConservation.v
        ↓
Kernel/ORUValidator.v  check_oru / check_oru_sound
```

## RES

```
K-R-11
        ↓
res_issuance_soundness
        ↓
proofs/RES_Issuance/RESIssuance.v
        ↓
Kernel/RESValidator.v  check_res / check_res_sound
```

## Script

```
K-S-11
        ↓
recognizer_pairwise_disjoint   (class A)
script_correctness             (class B supporting lemma)
        ↓
proofs/Script_Correctness/ScriptCorrectness.v
        ↓
Kernel/ScriptValidator.v  check_script / check_script_sound
```

## PoW

```
K-P-5 / K-P-7 plus compact lower bound
        ↓
pow_valid_rejects_above_target (class A)
compact_decode_lower_bound     (class A)
pow_valid_deterministic        (class B; abstract relation only)
        ↓
proofs/Pow_Correctness/PowCorrectness.v
Pow/CompactBits.v
        ↓
Kernel/PowValidator.v  check_pow / check_pow_sound
```

## ASERT

```
K-A-12
        ↓
pow_asert_correctness
negative_time_error_preserved
        ↓
proofs/Pow_Correctness/ASERTCorrectness.v
Pow/ASERTModel.v
        ↓
Kernel/ASERTValidator.v  check_asert / check_asert_sound
```

Per-ID tables: `../evidence/KernelCoverage.md`.
Test traces (not proofs): `../evidence/BinaryTrace.md`.
Classification: `../evidence/TheoremClassification.md`.

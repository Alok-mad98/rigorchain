# Kernel Completeness Matrix

Requirement → Coq definition → theorem → proof file → validator →
mutation case.

Coverage is **Coq kernel-model coverage**. Extraction **source
identity** is proved (`extraction_source_equivalence`). The OCaml
compiler and runtime remain outside the proof boundary.

**Public claim:** Formally verified critical consensus invariants
inside a Coq consensus kernel model, with explicit extraction and
runtime trust boundaries.

```
Requirement
        ↓
Coq definition
        ↓
Theorem
        ↓
Proof file
        ↓
Validator
        ↓
Mutation case
```

`KERNEL_COMPLETENESS_MATRIX=100%` means every row below has all six
cells filled.

## UTXO

```
K-U-13 (packages K-U-3 / K-U-6 / K-U-7)
        ↓
valid_tx / apply_tx / utxo_wf
        ↓
utxo_soundness
        ↓
proofs/UTXO_Soundness/UTXOSoundness.v
        ↓
Kernel/UTXOValidator.v   check_utxo
        ↓
M9  (apply_body no longer consumes inputs; executed via mutate_oru.sh)
M11 (apply_block nodupb guard dropped; mutate_utxo.sh)
M13 (apply_block_transition uniqueness guard dropped)
```

| Cell | Value |
|---|---|
| Requirement | K-U-13 |
| Coq definition | `valid_tx`, `apply_tx` |
| Theorem | `utxo_soundness` (A); `block_transition_utxo_soundness` (A); P0-120: `apply_block_accepted_inputs_unique` / `apply_block_rejects_duplicate_input` (B) |
| Proof file | `UTXOSoundness.v`, `UTXOTransitionSoundness.v`, `Kernel/SafetyExtensions.v` |
| Validator | `check_utxo` |
| Mutation | M9 (executed via `mutate_oru.sh`); M11 (nodupb guard, `mutate_utxo.sh`); M13 (`apply_block_transition` uniqueness) |
| Extraction source | `utxo_extraction_source_eq` |
| Extracted test | `utxo_valid.json` VALID; `INVALID-001` INVALID |

## ORU

```
K-O-5 / K-O-10
        ↓
ORU_supply_transition / fee
        ↓
oru_conservation_extended
no_hidden_inflation
        ↓
Supply/FeeDomain.v
proofs/ORU_Conservation/ORUConservation.v
        ↓
Kernel/ORUValidator.v   check_oru
        ↓
M3  (drop - burn)
M4  (fee sign flip)
```

| Cell | Value |
|---|---|
| Requirement | K-O-5 / K-O-10 |
| Coq definition | `new_supply`, `fee` |
| Theorem | `oru_conservation_extended` (A; `Supply/FeeDomain.v`), `no_hidden_inflation` (A) |
| Proof file | `FeeDomain.v`, `ORUConservation.v` |
| Validator | `check_oru` |
| Mutation | M3, M4 |
| Extraction source | `oru_extraction_source_eq` |
| Extracted test | `oru_conservation.json` VALID; `INVALID-002` INVALID |

## RES

```
K-R-11
        ↓
res_created / W(h)
        ↓
res_issuance_soundness
        ↓
proofs/RES_Issuance/RESIssuance.v
        ↓
Kernel/RESValidator.v   check_res
        ↓
M1  (W(h) + 1)
M2  (mint < W)
M10 (drop coinbase-only)
M12 (shrink RES_MAX_SUPPLY; schedule identity only)
M17 (inflate epoch_minted; schedule prefix only)
```

| Cell | Value |
|---|---|
| Requirement | K-R-11 |
| Coq definition | `res_created`, `W` |
| Theorem | `res_issuance_soundness` (A); `res_total_issuance_capped` (A); `epoch_supply_transition_sound` (A); P0-120: `res_mint_bounded_by_genesis` (B) |
| Proof file | `RESIssuance.v`, `Supply/SupplySchedule.v`, `Supply/SupplyTransition.v`, `Kernel/SafetyExtensions.v` |
| Validator | `check_res` |
| Mutation | M1, M2, M10, M12, M17 (M12/M17 = schedule-side only; not a runtime `S_RES` reject) |
| Extraction source | `res_extraction_source_eq` |
| Extracted test | `res_schedule.json` VALID; `INVALID-003` INVALID |

## Script

```
K-S-11 / K-S-4
        ↓
classify_script / classify_asset
        ↓
recognizer_pairwise_disjoint
        ↓
proofs/Script_Correctness/ScriptCorrectness.v
        ↓
Kernel/ScriptValidator.v   check_script
        ↓
M7  (witness v0 labelled T_MINT)
M14 (mint marker classified as converge)
```

| Cell | Value |
|---|---|
| Requirement | K-S-4 / K-S-11 |
| Coq definition | `classify_script` |
| Theorem | `recognizer_pairwise_disjoint` (A); `recognizer_trace_disjoint` (A); `script_trace_deterministic` (B) |
| Proof file | `ScriptCorrectness.v`, `Script/Semantics/ScriptTrace.v` |
| Validator | `check_script` |
| Mutation | M7, M14 (executed via `mutate_oru.sh`) |
| Extraction source | `script_extraction_source_eq` |
| Extracted test | `script_classification.json` VALID |

## PoW

```
K-P-7 plus compact lower bound
        ↓
hash_le_target / compact_encode
        ↓
pow_valid_rejects_above_target
compact_decode_lower_bound
        ↓
proofs/Pow_Correctness/PowCorrectness.v
Pow/CompactBits.v
        ↓
Kernel/PowValidator.v   check_pow
        ↓
M8  (invert target comparison)
M6  (claim decode(encode(t)) = t)
M15 (reverse meets_target)
```

| Cell | Value |
|---|---|
| Requirement | K-P-7 |
| Coq definition | `hash_le_target`, `compact_decode` |
| Theorem | `pow_valid_rejects_above_target`, `compact_decode_lower_bound`, `target_monotonic`, `pow_acceptance_monotonic` (A) |
| Proof file | `PowCorrectness.v`, `CompactBits.v`, `TargetRelation.v` |
| Validator | `check_pow` |
| Mutation | M8, M6, M15 |
| Extraction source | `pow_extraction_source_eq` |
| Extracted test | `pow_target.json` VALID; `INVALID-005` INVALID |

Abstract `H` only. Hash implementation is not covered
(`evidence/PowScope.md`).

## ASERT

```
K-A-12
        ↓
asert_next_target / exp_q_raw
        ↓
negative_time_error_preserved
exp_q_clamped
factor_range
        ↓
Pow/ASERTModel.v
proofs/Pow_Correctness/ASERTCorrectness.v
        ↓
Kernel/ASERTValidator.v   check_asert
        ↓
M5  (swap Z.quot and Z.div on exp_q_raw)
M16 (Z.quot → Z.div on exp_q_boundary)
```

| Cell | Value |
|---|---|
| Requirement | K-A-12 |
| Coq definition | `asert_next_target`, `exp_q_raw` |
| Theorem | `negative_time_error_preserved` (A); `exp_q_clamped`, `factor_range` (A); `time_error_direction_preserved` (A). `pow_asert_correctness` is Category C, function-ness only |
| Proof file | `ASERTModel.v`, `ASERTCorrectness.v`, `ASERTBoundary.v` |
| Validator | `check_asert` |
| Mutation | M5, M16 |
| Extraction source | `asert_extraction_source_eq` |
| Extracted test | `asert_negative_time.json` VALID; `INVALID-004` INVALID |

Deterministic integer model only. Hash / miner not covered
(`evidence/ASERTScope.md`).

## Score

| Domain | Requirement | Definition | Theorem | Proof | Validator | Mutation |
|---|---|---|---|---|---|---|
| UTXO | yes | yes | yes | yes | yes | M9, M11, M13 |
| ORU | yes | yes | yes | yes | yes | M3, M4 |
| RES | yes | yes | yes | yes | yes | M1, M2, M10, M12, M17 |
| Script | yes | yes | yes | yes | yes | M7, M14 |
| PoW | yes | yes | yes | yes | yes | M6, M8, M15 |
| ASERT | yes | yes | yes | yes | yes | M5, M16 |

Rows complete: 6 / 6. Each row has an extraction source-identity
theorem and at least one extracted executable vector.

Differential verification (production-logic shadow of the comparable
fragment) is recorded in `release/RefinementMapping.md` and
`evidence/DiffVerifyReport.md`. Coverage includes golden,
INVALID-001…005, and extra structured / random vectors around mapped
gaps). Count is regenerated in `evidence/DiffVerifyReport.md`.
`MATCH` means the two *implementations agreed*; it is **not** an
independent correctness assertion and **not** a production-code
refinement proof.

Independent auditor one-click: `release/independent_audit_replay.sh`.

Crypto assumptions are centralized in `Kernel/CryptoAssumptions.v`.

Verified-compiler path: documented rehearsal only
(`release/VerifiedCompilerPath.md`). Not in the checking TCB.

`KERNEL_COMPLETENESS_MATRIX=100%` is the six-row score above
(requirement / definition / theorem / proof / validator / mutation).
It is **not** a claim that every corollary has its own mutation, nor
that M7/M9 live in per-domain `mutate_script.sh` / `mutate_utxo.sh`
files — those IDs are executed by `mutation/mutate_oru.sh`.
M11 (`mutate_utxo.sh`) covers the `apply_block` `nodupb` guard.
M13–M17 cover the P0-120~125 deepening theorems (see
`release/KernelDeepeningReport.md`).
M18–M23 cover P0-126~130 completeness (transaction value-flow,
block composition, economic algebra, script subset step, PoW
pipeline, ASERT matrix). See `release/KernelCompletenessReport.md`.
M24–M25 cover the P0-131~135 refinement / machine skeleton
(`release/FormalTrustRoadmap.md`). Not a C++ refinement.
M26–M28 cover first-class ORU conservation, RES/ORU separation,
and economic refinement (model only).
M29–M30 cover Unified Kernel Transition: determinism of
`kernel_apply`, and UTXO/economic value-flow (no creation without
input or subsidy). Model only.
M31–M32 cover Coq abstract → Coq concrete refinement
(`kernel_projection` / `concrete_kernel_step`). Not C++ or node
refinement (OP-R2 remains open).
M33–M35 cover the executable-model projection, Coq-model
serialization, and executable-step determinism.
M36–M37 cover extraction-interface alignment and rejection of
the V1–V6 negative golden vectors. Not compiler correctness.
M38–M41 cover the Coq runtime projection, abstract checker reject,
runtime-step determinism, and failure-path preservation. Not the
OCaml runtime or a native binary.

P0-120 local safety corollaries (Closed, Coq model only, Category B):
`apply_block_accepted_inputs_unique` / `apply_block_rejects_duplicate_input`
and `res_mint_bounded_by_genesis`. Schedule-side aggregate
`res_total_issuance_capped` is Category A. These do **not** close
reorg / common-prefix (OP-R1) or runtime `S_RES` rejection (OP-D1).
See `Kernel/SafetyExtensions.v`.

Open properties (liveness, fairness, finality, reorg, refinement,
crypto, binary equivalence): `release/OpenProperties.md`.

`KERNEL_COMPLETENESS_MATRIX=100%`
`EXTRACTION_SOURCE_IDENTITY_PROVED=TRUE`
`EXTRACTION_FORMAL_PROOF=source_identity_only`
`EXTRACTION_EQUIVALENCE_NOT_CLAIMED=TRUE`
`PRODUCTION_REFINEMENT_NOT_CLAIMED=TRUE`
`COMPILER_CORRECTNESS=false`
`BINARY_SEMANTIC_EQUIVALENCE=false`

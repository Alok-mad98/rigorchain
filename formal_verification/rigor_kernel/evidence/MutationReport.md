# Mutation Report

Recorded run of `./mutation/run_all_mutations.sh` during P0-109
evidence hardening. Healthy cone: `make` passes. Each mutant must
print `EXPECTED_FAILURE` and must **not** print `mutation_passed`.

These results show that an incorrect consensus *model* rule makes the
corresponding Coq proof fail. They do **not** claim C++ equivalence.

## Summary

| Metric | Value |
|---|---|
| Mutation cases | 41 |
| `EXPECTED_FAILURE` | 41 |
| `mutation_passed` | 0 |
| Result | `MUTATION_TEST_COMPLETE=TRUE` |

## Case results

| ID | Incorrect rule | Expected failing theorem | Result |
|---|---|---|---|
| M1 | `W(h)` increased by `+1` | `res_schedule_finite` | EXPECTED_FAILURE |
| M2 | `mint <= W(h)` becomes `mint < W(h)` | `res_issuance_soundness` | EXPECTED_FAILURE |
| M3 | drop `- burn` from conservation | `oru_conservation_extended` | EXPECTED_FAILURE |
| M4 | `fee` defined as `out - in` (creates ORU) | `no_hidden_inflation` | EXPECTED_FAILURE |
| M5 | `Z.quot` replaced by `Z.div` on `exp_q_raw` | `negative_time_error_preserved` | EXPECTED_FAILURE |
| M6 | claim `decode(encode(t)) = t` | `compact_decode_lower_bound` | EXPECTED_FAILURE |
| M7 | witness v0/20 classified as `T_MINT` | `recognizer_pairwise_disjoint` | EXPECTED_FAILURE |
| M8 | invert target comparison `<=` → `>` | `pow_valid_deterministic` | EXPECTED_FAILURE |
| M9 | `apply_body` no longer consumes inputs | `utxo_soundness` | EXPECTED_FAILURE |
| M10 | drop coinbase-only from `res_created` | `res_issuance_soundness` | EXPECTED_FAILURE |
| M11 | drop `nodupb` guard from `apply_block` | `apply_block_accepted_inputs_unique` | EXPECTED_FAILURE |
| M12 | shrink `RES_MAX_SUPPLY` below epoch-sum | `res_total_issuance_capped` | EXPECTED_FAILURE |
| M13 | drop `transition_inputs_unique` guard | `block_transition_utxo_soundness` | EXPECTED_FAILURE |
| M14 | mint marker classified as converge | `recognizer_trace_disjoint` | EXPECTED_FAILURE |
| M15 | reverse `meets_target` comparison | `target_monotonic` | EXPECTED_FAILURE |
| M16 | `Z.quot` → `Z.div` on `exp_q_boundary` | `time_error_direction_preserved` | EXPECTED_FAILURE |
| M17 | inflate `epoch_minted` | `epoch_supply_transition_sound` | EXPECTED_FAILURE |
| M18 | allow output value > input value | `transaction_value_conservation` | EXPECTED_FAILURE |
| M19 | remove PoW acceptance conjunct | `block_acceptance_sound` | EXPECTED_FAILURE |
| M20 | fees mint coins | `economic_no_hidden_creation` | EXPECTED_FAILURE |
| M21 | nondeterministic opcode transition | `script_step_deterministic` | EXPECTED_FAILURE |
| M22 | invert pipeline comparison | `pow_pipeline_sound` | EXPECTED_FAILURE |
| M23 | `Z.quot` → `Z.div` on `matrix_quot` | `asert_boundary_complete` | EXPECTED_FAILURE |
| M24 | break `observe_ok` / `refines` | `refinement_preservation` | EXPECTED_FAILURE |
| M25 | nondeterministic machine transition | `machine_step_deterministic` | EXPECTED_FAILURE |
| M26 | allow fee creation | `oru_domain_conservation` | EXPECTED_FAILURE |
| M27 | ORU transfer increases supply | `res_creation_separate_from_oru_transfer` | EXPECTED_FAILURE |
| M28 | break economic refinement | `economic_refinement_preserves_conservation` | EXPECTED_FAILURE |
| M29 | allow two different next states | `kernel_transition_deterministic` | EXPECTED_FAILURE |
| M30 | create value without input/subsidy | `transaction_value_flow_preserves_economic_state` | EXPECTED_FAILURE |
| M31 | break `kernel_projection` | `concrete_state_preserves_invariant` | EXPECTED_FAILURE |
| M32 | concrete step bypasses abstract step | `concrete_step_refines_abstract_step` | EXPECTED_FAILURE |
| M33 | break `execution_projection` | `execution_projection_preserves_invariants` | EXPECTED_FAILURE |
| M34 | serialize flag as 2 | `decode_encode_identity` | EXPECTED_FAILURE |
| M35 | nondeterministic executable step | `executable_transition_deterministic` | EXPECTED_FAILURE |
| M36 | force-accept invalid vector | `golden_negative_vectors_rejected` | EXPECTED_FAILURE |
| M37 | change extraction mapping | `extraction_interface_bijective` | EXPECTED_FAILURE |
| M38 | break `runtime_projection` | `runtime_projection_preserves_kernel_domain` | EXPECTED_FAILURE |
| M39 | accept invalid kernel state | `checker_rejects_invalid_kernel_state` | EXPECTED_FAILURE |
| M40 | nondeterministic runtime step | `runtime_step_deterministic` | EXPECTED_FAILURE |
| M41 | remove failure path | `runtime_failure_preserved` | EXPECTED_FAILURE |

M7, M9, and M14 are executed by `mutation/mutate_oru.sh`.
M11 and M13 are executed by `mutation/mutate_utxo.sh`.
M12 and M17 are executed by `mutation/mutate_supply.sh` (schedule-side
only; **not** a runtime `S_RES` reject).
M15 and M16 are executed by `mutation/mutate_pow.sh`.
M16 mutates `exp_q_boundary`, not `exp_q_raw` (M5).

M7 may fail first at `witness_never_falls_to_mint_or_converge` in
`Script/ScriptClassifier.v`. M8 may fail first at
`pow_rejects_above_target` in `Pow/PowModel.v`. M9 may fail first at
`apply_tx_inputs_consumed`. In each case the mutated `.vo` does not
build, which is the required evidence.

## Raw log (P0-109 host)

```
== Mutation M1  expected_fail=res_schedule_finite  file=Supply/SupplySchedule.v ==
EXPECTED_FAILURE id=M1 theorem=res_schedule_finite
== Mutation M3  expected_fail=oru_conservation_extended  file=proofs/ORU_Conservation/ORUConservation.v ==
EXPECTED_FAILURE id=M3 theorem=oru_conservation_extended
== Mutation M4  expected_fail=no_hidden_inflation  file=proofs/ORU_Conservation/ORUConservation.v ==
EXPECTED_FAILURE id=M4 theorem=no_hidden_inflation
== Mutation M7  expected_fail=recognizer_pairwise_disjoint  file=Script/ScriptClassifier.v ==
EXPECTED_FAILURE id=M7 theorem=recognizer_pairwise_disjoint
== Mutation M9  expected_fail=utxo_soundness  file=proofs/UTXO_Soundness/UTXOSoundness.v ==
EXPECTED_FAILURE id=M9 theorem=utxo_soundness
== Mutation M2  expected_fail=res_issuance_soundness  file=proofs/RES_Issuance/RESIssuance.v ==
EXPECTED_FAILURE id=M2 theorem=res_issuance_soundness
== Mutation M10  expected_fail=res_issuance_soundness  file=proofs/RES_Issuance/RESIssuance.v ==
EXPECTED_FAILURE id=M10 theorem=res_issuance_soundness
== Mutation M5  expected_fail=negative_time_error_preserved  file=Pow/ASERTModel.v ==
EXPECTED_FAILURE id=M5 theorem=negative_time_error_preserved
== Mutation M6  expected_fail=compact_decode_lower_bound  file=Pow/CompactBits.v ==
EXPECTED_FAILURE id=M6 theorem=compact_decode_lower_bound
== Mutation M8  expected_fail=pow_valid_deterministic  file=Pow/AbstractHash.v ==
EXPECTED_FAILURE id=M8 theorem=pow_valid_deterministic
---- mutation summary ----
EXPECTED_FAILURES=10
MUTATION_PASSES=0
MUTATION_TEST_COMPLETE=TRUE
EXPECTED_FAILURES_CONFIRMED=TRUE
```

Later increments added M11 (`mutate_utxo.sh`) and M12
(`mutate_supply.sh`, schedule-side `RES_MAX_SUPPLY` shrink), then
M13–M17 (UTXO transition, script-trace disjointness, target
ordering, ASERT `Z.quot` boundary, epoch-prefix supply). The live
runner expected 17 `EXPECTED_FAILURE` lines after deepening. P0-126~130
adds M18–M23 (`mutate_completeness.sh`), then M24–M25
(`mutate_refinement.sh`), then M26–M28 (`mutate_oru_domain.sh`),
then M29–M30 (`mutate_kernel_transition.sh`),
then M31–M32 (`mutate_concrete_refinement.sh`),
then M33–M35 (`mutate_execution_boundary.sh`),
then M36–M37 (`mutate_extraction_hardening.sh`),
then M38–M41 (`mutate_runtime_boundary.sh`).
The live runner now expects 41. The P0-109 log above is the
historical ten-case run.

## Restore

Each mutation restores the original `.v` and rebuilds the corresponding
`.vo` before the next case. The runner finishes with a clean `make` of
the kernel cone.

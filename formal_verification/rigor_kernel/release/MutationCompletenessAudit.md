# Mutation Completeness Audit

Audit only. **No new mutation** in P0-130.

Frontier 1.1 and 1.4 added M51-M76 (M66 was withdrawn as redundant),
frontier 2.1 added M77-M89 for the chain-level layer, frontier 2.4
added M90-M94 for the ASERT fixed-point width and wrap bounds, RFC-019
added M95-M99 for unit-price monotonicity, 128-bit products, block mint
sum, derived-cap identity and reorg replay, and the
harness was rebuilt so a mutant is only credited with a kill it can be
shown to make. The per-mutation table below is the P0-130 snapshot and
stops at M41; the current, generated declaration table is
`mutation/expected_failures.md`.

Two harness corrections came out of running the suite rather than
reading it. 2.1's rebuild stopped a mutant from claiming a kill it did
not make. 2.4 found the complement: `mutation/admit_proof.py` matched
`Qed.` only at the start of a line, so on the one-line
`Proof. ... Qed.` form it ran past the proof and deleted the *next*
lemma, and the mutant was then reported as having broken elaboration
rather than as having killed anything. Both forms are handled now.

Healthy cone: `./mutation/run_all_mutations.sh` prints 127
`EXPECTED_FAILURE` lines and `MUTATION_PASSES=0`.
Frontier 2.5 added M108–M128 (block proof, multi-tip, maturity,
script spend).

`MUTATION_COUNT=127`
`MUTATION_TEST_COMPLETE=TRUE`
`EXPECTED_FAILURES_CONFIRMED=TRUE`

## M1–M41

| ID | Target theorem | Broken rule | Expected failure | Executed | Observed |
|---|---|---|---|---|---|
| M1 | `res_schedule_finite` | `W(h)` increased by `+1` | compile/Qed fail | `mutate_supply.sh` | EXPECTED_FAILURE |
| M2 | `res_issuance_soundness` | `mint <= W(h)` becomes `<` | compile/Qed fail | `mutate_res.sh` | EXPECTED_FAILURE |
| M3 | `oru_conservation_extended` | drop `- burn` | compile/Qed fail | `mutate_oru.sh` | EXPECTED_FAILURE |
| M4 | `no_hidden_inflation` | fee = `out - in` | compile/Qed fail | `mutate_oru.sh` | EXPECTED_FAILURE |
| M5 | `negative_time_error_preserved` | `Z.quot` → `Z.div` | compile/Qed fail | `mutate_pow.sh` | EXPECTED_FAILURE |
| M6 | `compact_decode_lower_bound` | claim lossless compact codec | compile/Qed fail | `mutate_pow.sh` | EXPECTED_FAILURE |
| M7 | `recognizer_pairwise_disjoint` | witness classified as `T_MINT` | compile/Qed fail | `mutate_oru.sh` | EXPECTED_FAILURE |
| M8 | `pow_valid_deterministic` | invert target comparison | compile/Qed fail | `mutate_pow.sh` | EXPECTED_FAILURE |
| M9 | `utxo_soundness` | `apply_body` does not consume | compile/Qed fail | `mutate_oru.sh` | EXPECTED_FAILURE |
| M10 | `res_issuance_soundness` | drop coinbase-only | compile/Qed fail | `mutate_res.sh` | EXPECTED_FAILURE |
| M11 | `apply_block_accepted_inputs_unique` | drop `nodupb` | compile/Qed fail | `mutate_utxo.sh` | EXPECTED_FAILURE |
| M12 | `res_total_issuance_capped` | shrink `RES_MAX_SUPPLY` | compile/Qed fail | `mutate_supply.sh` | EXPECTED_FAILURE |
| M13 | `block_transition_utxo_soundness` | drop uniqueness guard | compile/Qed fail | `mutate_utxo.sh` | EXPECTED_FAILURE |
| M14 | `recognizer_trace_disjoint` | mint classified as converge | compile/Qed fail | `mutate_oru.sh` | EXPECTED_FAILURE |
| M15 | `target_monotonic` | reverse `meets_target` | compile/Qed fail | `mutate_pow.sh` | EXPECTED_FAILURE |
| M16 | `time_error_direction_preserved` | `Z.quot` → `Z.div` on boundary | compile/Qed fail | `mutate_pow.sh` | EXPECTED_FAILURE |
| M17 | `epoch_supply_transition_sound` | inflate `epoch_minted` | compile/Qed fail | `mutate_supply.sh` | EXPECTED_FAILURE |
| M18 | `transaction_value_conservation` | output > input | compile/Qed fail | `mutate_completeness.sh` | EXPECTED_FAILURE |
| M19 | `block_acceptance_sound` | drop PoW conjunct | compile/Qed fail | `mutate_completeness.sh` | EXPECTED_FAILURE |
| M20 | `economic_no_hidden_creation` | fees mint | compile/Qed fail | `mutate_completeness.sh` | EXPECTED_FAILURE |
| M21 | `script_step_deterministic` | nondeterministic opcode | compile/Qed fail | `mutate_completeness.sh` | EXPECTED_FAILURE |
| M22 | `pow_pipeline_sound` | invert pipeline comparison | compile/Qed fail | `mutate_completeness.sh` | EXPECTED_FAILURE |
| M23 | `asert_boundary_complete` | `Z.quot` → `Z.div` on matrix | compile/Qed fail | `mutate_completeness.sh` | EXPECTED_FAILURE |
| M24 | `refinement_preservation` | break `observe_ok` / `refines` | compile/Qed fail | `mutate_refinement.sh` | EXPECTED_FAILURE |
| M25 | `machine_step_deterministic` | nondeterministic machine | compile/Qed fail | `mutate_refinement.sh` | EXPECTED_FAILURE |
| M26 | `oru_domain_conservation` | allow fee creation | compile/Qed fail | `mutate_oru_domain.sh` | EXPECTED_FAILURE |
| M27 | `res_creation_separate_from_oru_transfer` | ORU transfer inflates | compile/Qed fail | `mutate_oru_domain.sh` | EXPECTED_FAILURE |
| M28 | `economic_refinement_preserves_conservation` | break `economic_refines` | compile/Qed fail | `mutate_oru_domain.sh` | EXPECTED_FAILURE |
| M29 | `kernel_transition_deterministic` | two next states | compile/Qed fail | `mutate_kernel_transition.sh` | EXPECTED_FAILURE |
| M30 | `transaction_value_flow_preserves_economic_state` | create value without input/subsidy | compile/Qed fail | `mutate_kernel_transition.sh` | EXPECTED_FAILURE |
| M31 | `concrete_state_preserves_invariant` | break `kernel_projection` | compile/Qed fail | `mutate_concrete_refinement.sh` | EXPECTED_FAILURE |
| M32 | `concrete_step_refines_abstract_step` | concrete bypasses abstract | compile/Qed fail | `mutate_concrete_refinement.sh` | EXPECTED_FAILURE |
| M33 | `execution_projection_preserves_invariants` | break `execution_projection` | compile/Qed fail | `mutate_execution_boundary.sh` | EXPECTED_FAILURE |
| M34 | `decode_encode_identity` | `encode_flag` 1→2 | compile/Qed fail | `mutate_execution_boundary.sh` | EXPECTED_FAILURE |
| M35 | `executable_transition_deterministic` | two executable next states | compile/Qed fail | `mutate_execution_boundary.sh` | EXPECTED_FAILURE |
| M36 | `golden_negative_vectors_rejected` | force-accept invalid | compile/Qed fail | `mutate_extraction_hardening.sh` | EXPECTED_FAILURE |
| M37 | `extraction_interface_bijective` | CK_UTXO maps to EK_ORU | compile/Qed fail | `mutate_extraction_hardening.sh` | EXPECTED_FAILURE |
| M38 | `runtime_projection_preserves_kernel_domain` | flip projected UTXO flag | compile/Qed fail | `mutate_runtime_boundary.sh` | EXPECTED_FAILURE |
| M39 | `checker_rejects_invalid_kernel_state` | `force_checker_accept` | compile/Qed fail | `mutate_runtime_boundary.sh` | EXPECTED_FAILURE |
| M40 | `runtime_step_deterministic` | `runtime_step_unique = false` | compile/Qed fail | `mutate_runtime_boundary.sh` | EXPECTED_FAILURE |
| M41 | `runtime_failure_preserved` | remove failure path | compile/Qed fail | `mutate_runtime_boundary.sh` | EXPECTED_FAILURE |

Runner: `mutation/run_all_mutations.sh` pins the count of the whole
suite, not of this snapshot. The pin is 93; the 41 above is where the
P0-130 table stops.

## Mutation classes (required)

| Class | IDs | Covered? |
|---|---|---|
| UTXO | M9, M11, M13 | yes |
| Economic | M20, M27, M28, M30 | yes |
| RES | M2, M10 | yes |
| ORU | M3, M4, M26 | yes |
| Script | M7, M14, M21 | yes |
| PoW | M8, M15, M22 | yes |
| ASERT | M5, M16, M23, M90-M94 | yes |
| Runtime | M38, M40, M41 | yes |
| Extraction | M36, M37 | yes |

**No required class is uncovered.**

Additional classes present (not requested, still executed): Supply
(M1, M12, M17), Transaction (M18), Block (M19), Refinement (M24,
M31, M32), Execution (M25, M33, M35), Serialization (M34), Checker
(M39).

## Honest gaps (not missing classes)

Category A theorems with **no dedicated mutant** include
`kernel_sound`, `integer_range_safe`, `runtime_domain_closed`,
`operation_trace_preserved`, `checker_accepts_valid_kernel_state`,
several economic siblings (`fees_only_transfer`, …), and some
refinement composition lemmas. That is incomplete *per-theorem*
coverage, not an uncovered *class*. P0-130 does not add mutants.

M8’s listed target is a Category B name; the mutant is still a
real PoW-comparison break.

Binary harness B1–B3 are **not** Coq mutations and are not counted
in 41.

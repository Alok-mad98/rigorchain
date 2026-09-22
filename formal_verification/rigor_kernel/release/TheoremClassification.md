# Theorem Classification

How to read the kernel theorem list. Nothing is deleted; weaker
statements are kept and **not inflated**.

**Public claim:** Formally verified critical consensus invariants
inside a Coq consensus kernel model, with explicit extraction and
runtime trust boundaries.

## Categories

| Category | Meaning | Public weight |
|---|---|---|
| A | Core verified consensus invariant | May be cited in the public claim |
| B | Supporting mathematical lemma | Do not cite as the domain's soundness result |
| C | Boundary / documentation lemma | Documentation only; not invariant evidence |

## Category A — core verified consensus invariant

| Theorem | File | Domain |
|---|---|---|
| `asert_boundary_complete` | `Pow/ASERTMatrix.v` | ASERT |
| `asert_poly_fits_uint64` | `proofs/Pow_Correctness/ASERTCorrectness.v` | The same polynomial stays under 2^64, by under one part in 10^5 at the top of the domain. Bounds the width the evaluation needs. M90 |
| `asert_poly_pos` | `proofs/Pow_Correctness/ASERTCorrectness.v` | The ASERT cubic is strictly positive on its whole domain, so the unsigned cast of `poly >> 48` in the C++ cannot wrap. A negative polynomial would not yield a small target but an enormous one, and nothing downstream checks it. M91 |
| `block_acceptance_sound` | `Kernel/Block/Acceptance.v` | Block |
| `block_proof_eq_div` | `Pow/BlockProof.v` | GetBlockProof: `(~T/(T+1))+1 = 2^256/(T+1)` for `0 < T < 2^256`. Not arith_uint256 refinement, not OP-R1. M108 |
| `block_proof_positive_on_valid` | `Pow/BlockProof.v` | A representable positive target has strictly positive work. M111 |
| `block_proof_zero_on_bad_target` | `Pow/BlockProof.v` | `T <= 0` or `T >= 2^256` contributes proof 0. M109 |
| `block_proof_zero_on_compact_fail` | `Pow/BlockProof.v` | SetCompact negative / overflow / decoded 0 / negative nBits yields proof 0. M110 |
| `block_transition_utxo_soundness` | `proofs/UTXO_Soundness/UTXOTransitionSoundness.v` | UTXO |
| `chain_no_hidden_creation` | `Kernel/Chain/ChainTransition.v` | Over a run, the ORU stock moves by subsidy plus transfers in, minus transfers out and burn, and by nothing else. M81 |
| `chain_oru_conservation` | `Kernel/Chain/ChainTransition.v` | Tip stock equals start stock plus the summed per-block flow. The chain-level form of conservation; a single step cannot state it. M80 |
| `chain_res_cap_fixed` | `Kernel/Chain/ChainTransition.v` | The RES cap a run is measured against is the cap it started with. Rules out a chain that raises its own ceiling block by block, which no single-step theorem can see. M82 |
| `chain_res_monotone` | `Kernel/Chain/ChainTransition.v` | RES issuance never runs backwards along an acceptable run. M83 |
| `chain_work_app` | `Kernel/Chain/ForkChoice.v` | Accumulated chainwork is the sum of per-block GetBlockProof increments. First-only or last-only is not a chainwork. Not OP-R1. M102 |
| `chain_work_uses_block_proof` | `Kernel/Chain/ForkChoice.v` | Suffix work is `GetBlockProof` on each block's nBits. A free increment is a different function. M112 |
| `check_econ_reflects` | `Kernel/EconomicInvariant/Economic.v` | Economic |
| `check_executable_kernel_reflects` | `CheckerContract/CheckerSpecification.v` | Checker contract |
| `check_value_flow_reflects` | `Kernel/Transaction/Validity.v` | Transaction |
| `checker_accepts_valid_kernel_state` | `CheckerContract/CheckerSpecification.v` | Checker |
| `checker_decides_kernel_validity` | `CheckerContract/CheckerSpecification.v` | Checker contract |
| `checker_rejects_invalid_kernel_state` | `CheckerContract/CheckerSpecification.v` | Checker |
| `checker_rejects_unrepresentable_state` | `CheckerContract/CheckerSpecification.v` | Checker contract |
| `classify_range_exact` | `Script/ScriptClassifier.v` | Script |
| `classify_total` | `Script/ScriptClassifier.v` | Script |
| `classify_type_total` | `proofs/Script_Correctness/ScriptCorrectness.v` | Script |
| `classifier_then_spend` | `Script/Semantics/Spend.v` | Spend uses `classify_script`, not a second recognizer ladder. Not a VM. M128 |
| `compact_decode_lower_bound` | `Pow/CompactBits.v` | PoW / compact |
| `concrete_chain_commute` | `Kernel/Chain/ChainRefinement.v` | The concrete layer's run projects to the abstract run. M84 |
| `converge_not_null_data` | `Script/ScriptClassifier.v` | Script |
| `converge_unspendable` | `Script/Semantics/Spend.v` | A converge lock cannot be spent. `bad-txns-spend-convergence`. Not a VM. M123 |
| `decode_encode_identity` | `SerializationBoundary/KernelSerialization.v` | Serialization |
| `econ_chain_no_supply_loss` | `Kernel/EconomicInvariant/Economic.v` | Economic |
| `economic_net_issuance_is_subsidy` | `Kernel/EconomicInvariant/Economic.v` | Economic |
| `economic_no_hidden_creation` | `Kernel/EconomicInvariant/Economic.v` | Economic |
| `economic_refinement_preserves_conservation` | `Refinement/EconomicRefinement.v` | Refinement |
| `epoch_supply_transition_sound` | `Supply/SupplyTransition.v` | Supply |
| `equal_work_keeps_active` | `Kernel/Chain/ForkChoice.v` | Equal chainwork keeps the already-active suffix: no reorg for nothing. Not k-deep, not OP-R1. M104 |
| `executable_chain_commute` | `Kernel/Chain/ChainRefinement.v` | The executable layer's run projects to the concrete run. M85 |
| `executable_chain_preserves_invariants` | `Kernel/Chain/ChainRefinement.v` | Executable invariants survive a whole run, with acceptability transported through the projection at each step. M86 |
| `executable_step_preserves_invariants` | `ExecutionBoundary/ExecutableInvariance.v` | Execution boundary |
| `exp_q_clamped` | `proofs/Pow_Correctness/ASERTCorrectness.v` | ASERT |
| `extraction_interface_bijective` | `ExtractionBoundary/ExtractionModel.v` | Extraction |
| `extraction_interface_injective` | `ExtractionBoundary/ExtractionModel.v` | Extraction boundary |
| `extraction_interface_surjective` | `ExtractionBoundary/ExtractionModel.v` | Extraction boundary |
| `factor_range` | `proofs/Pow_Correctness/ASERTCorrectness.v` | ASERT |
| `factor_wrap_continuous` | `proofs/Pow_Correctness/ASERTCorrectness.v` | `factor 65536 = 2 * factor 0` exactly. The exponent is split into a shift count and a remainder, and this is the statement that the two pieces agree where they meet, so the piecewise target has no jump as the remainder wraps. The most coefficient-sensitive result here. M90, M92, M93, M94 |
| `golden_negative_vectors_rejected` | `ExtractionBoundary/GoldenNegatives.v` | Extraction |
| `immature_coinbase_rejected` | `Kernel/Tx/Maturity.v` | Coinbase spends below depth 100 are rejected. Not ConnectBlock. M117 |
| `input_over_max_rejected` | `Kernel/Tx/Maturity.v` | An input above MAX_MONEY is rejected. Overflow sanity bound, not an ORU supply cap. M121 |
| `input_ok_of_checks` | `Kernel/KernelInputBridge.v` | Restated over `computed_ok_b`: if the six Path A checks and the three state bools pass, Path B `input_ok` holds on `kernel_input_of w`. One way only. Not a production-authority claim. M129 |
| `input_ok_reflect` | `Kernel/UnifiedTransition.v` | Path B `input_ok` is decidable: `input_ok s i ↔ input_ok_b s i = true` on any record. Reflection of the existing predicate, not a new rule. Not a production checker. M130 |
| `integer_range_safe` | `RuntimeModel/IntegerBoundary.v` | Runtime |
| `invalid_kernel_input_rejected` | `Runtime/RuntimeFailure.v` | Runtime |
| `kernel_apply_of_checks` | `Kernel/KernelInputBridge.v` | Deferral to `input_ok_of_checks` and `kapp_fun`: a computed input that passes the checks is related by `kernel_apply` to the step-function result. Coverage is the parent mutant. Not a production-authority claim |
| `kernel_chain_is_graph` | `Kernel/Chain/ChainTransition.v` | The chain relation is exactly the chain fold on acceptable runs. M77 mis-evaluates acceptability at the initial state and this stops proving |
| `kernel_chain_preserves_invariants` | `Kernel/Chain/ChainTransition.v` | The six kernel invariants survive a run of arbitrary length, by induction on the run rather than by iterating the single-step result at the call site. M78 |
| `kernel_chain_states_invariant` | `Kernel/Chain/ChainTransition.v` | Every state the run passes through satisfies the invariants, not only the tip. Strictly stronger than the preceding row. M79 |
| `kernel_sound` | `Kernel/KernelSoundness.v` | Bridge |
| `kernel_step_preserves_invariants` | `proofs/KernelTransitionSoundness.v` | Unified Kernel |
| `kernel_step_refinement_composes` | `Refinement/KernelComposition.v` | Refinement |
| `machine_chain_refines_executable_chain` | `Kernel/Chain/ChainRefinement.v` | A run the bounded-word machine completes decodes to the run the model computes. M87 |
| `machine_invariants_imply_representable` | `Runtime/MachineState.v` | Runtime boundary |
| `machine_state_preserves_invariant` | `Execution/KernelMachine.v` | Execution |
| `machine_step_failure_is_overflow` | `Runtime/MachineState.v` | Runtime boundary |
| `machine_step_refines_executable_step` | `Runtime/MachineState.v` | Runtime refinement |
| `mature_coinbase_at_100` | `Kernel/Tx/Maturity.v` | In-range coinbase becomes spendable at depth 100, not 101. M118 |
| `mint_not_converge` | `Script/ScriptClassifier.v` | Script |
| `mint_not_null_data` | `Script/ScriptClassifier.v` | Script |
| `mint_script_of_is_mint` | `Script/ScriptClassifier.v` | Script |
| `more_work_wins` | `Kernel/Chain/ForkChoice.v` | The strictly heavier suffix is selected, regardless of length. Longest-chain selection is a different (wrong) rule. Not k-deep, not OP-R1. M103 |
| `negative_time_error_preserved` | `Pow/ASERTModel.v` | ASERT |
| `no_hidden_inflation` | `proofs/ORU_Conservation/ORUConservation.v` | ORU |
| `nonstandard_unspendable` | `Script/Semantics/Spend.v` | A nonstandard lock cannot be spent. Not a VM. M125 |
| `nulldata_unspendable` | `Script/Semantics/Spend.v` | OP_RETURN / null-data cannot be spent. Not a VM. M124 |
| `operation_trace_preserved` | `ExtractionBoundary/ExtractionTrace.v` | Extraction |
| `oru_conservation_extended` | `Supply/FeeDomain.v` | ORU |
| `oru_reorg_rewind_eq` | `Kernel/Chain/ReorgReplay.v` | Dual-supply ORU rewind: disconnect orphans then apply the new suffix equals never applying the orphans. Pair of `res_reorg_rewind_eq`. Not OP-R1. M105 |
| `oru_reorg_surviving_pool` | `Kernel/Chain/ReorgReplay.v` | Surviving ORU pool is start + prefix + suffix; orphans are not a summand. M106 |
| `oru_subsidy_fold_closed` | `Supply/SupplySchedule.v` | RFC-020: subsidy fold from genesis through height n equals n · 128. A halving schedule would have been an epoch loop. M107 |
| `plain_transfer_no_maturity` | `Kernel/Tx/Maturity.v` | A non-coinbase, non-mint transfer has no depth gate. M120 |
| `pool_add_overflow_none` | `Kernel/Tx/Maturity.v` | A pool add that would exceed MAX_MONEY is None, checked before the addition. M122 |
| `pow_acceptance_monotonic` | `Pow/TargetRelation.v` | PoW |
| `pow_pipeline_sound` | `Pow/Acceptance.v` | PoW |
| `pow_valid_rejects_above_target` | `proofs/Pow_Correctness/PowCorrectness.v` | PoW |
| `reachable_preserves_invariants` | `Kernel/Chain/ChainTransition.v` | Nothing an acceptable run can reach violates the invariants. Deferral to `kernel_chain_is_graph` and `kernel_chain_preserves_invariants`; the coverage gate checks the components |
| `recognizer_pairwise_disjoint` | `proofs/Script_Correctness/ScriptCorrectness.v` | Script |
| `recognizer_trace_disjoint` | `Script/Semantics/ScriptTrace.v` | Script |
| `refinement_preservation` | `Refinement/TransitionRefinement.v` | Refinement |
| `res_block_mint_sum` | `proofs/RES_Issuance/RESIssuance.v` | RFC-019: folding body mints adds their amounts and nothing else. M97 |
| `res_creation_separate_from_oru_transfer` | `Economic/DomainSeparation.v` | Economic |
| `res_derived_cap_identity` | `Supply/SupplySchedule.v` | RFC-019: epoch-unfolded derived issuance equals the cap pin. M98 |
| `res_height_monotonic` | `Supply/SupplySchedule.v` | RFC-019: RATE is non-decreasing on the authorized ray. M95 |
| `res_issuance_soundness` | `proofs/RES_Issuance/RESIssuance.v` | RES |
| `res_mint_products_fit_uint128` | `Supply/SupplySchedule.v` | RFC-019: a legal mint's cross-products fit unsigned 128-bit. M96 |
| `res_mint_uses_same_maturity` | `Kernel/Tx/Maturity.v` | Body RES mint uses the same 100-block spend gate as coinbase. M119 |
| `res_reorg_replay_eq` | `proofs/RES_Issuance/RESIssuance.v` | RFC-020: same-height rewind of orphans then kept++suffix. Forwards to `res_reorg_rewind_eq`. M99 |
| `res_reorg_rewind_eq` | `Kernel/Chain/ReorgReplay.v` | RFC-020: disconnect orphans then apply the new suffix equals never applying the orphans. Not fork-choice (OP-R1). M100 |
| `res_reorg_surviving_pool` | `Kernel/Chain/ReorgReplay.v` | RFC-020: surviving RES pool is start + prefix + suffix; orphans are not a summand. M101 |
| `res_schedule_finite` | `Supply/SupplySchedule.v` | Supply |
| `res_total_issuance_capped` | `Supply/SupplySchedule.v` | Supply |
| `runtime_accepts_kernel_valid_input` | `Runtime/RuntimeFailure.v` | Runtime failure |
| `runtime_domain_closed` | `RuntimeModel/IntegerBoundary.v` | Runtime |
| `runtime_failure_preserved` | `Runtime/RuntimeFailure.v` | Runtime |
| `runtime_projection_preserves_kernel_domain` | `Runtime/RuntimeState.v` | Runtime |
| `runtime_rejects_iff_some_flag_down` | `Runtime/RuntimeFailure.v` | Runtime failure |
| `select_tip_binary` | `Kernel/Chain/ForkChoice.v` | Finite-tip selection specialises to the two-tip rule. M115 |
| `select_tip_keeps_active` | `Kernel/Chain/ForkChoice.v` | If the active tip already has the pool max, it stays selected. Equal work is not a reorg. M114 |
| `select_tip_maximal` | `Kernel/Chain/ForkChoice.v` | The selected tip's work is at least every candidate's work. Not ActivateBestChain, not OP-R1. M113 |
| `select_tip_total` | `Kernel/Chain/ForkChoice.v` | The selected tip is one of the candidates, including the active suffix. M116 |
| `subsidy_is_only_creation_source` | `Economic/ORU/ORUSoundness.v` | ORU |
| `subsidy_schedule_bound` | `Kernel/EconomicInvariant/Economic.v` | Economic |
| `target_monotonic` | `Pow/TargetRelation.v` | PoW |
| `time_error_direction_preserved` | `Pow/ASERTBoundary.v` | ASERT |
| `transaction_fee_bounded_by_inputs` | `Kernel/Transaction/Validity.v` | Transaction |
| `transaction_fee_separation` | `Kernel/Transaction/Validity.v` | Transaction |
| `transaction_no_negative_value` | `Kernel/Transaction/Validity.v` | Transaction |
| `transaction_single_output_bounded` | `Kernel/Transaction/Validity.v` | Transaction |
| `transaction_value_conservation` | `Kernel/Transaction/Validity.v` | Transaction |
| `transaction_value_flow_preserves_economic_state` | `Economic/UTXOEconomicBridge.v` | Unified Kernel |
| `utxo_soundness` | `proofs/UTXO_Soundness/UTXOSoundness.v` | UTXO |
| `witness_requires_sig_ok` | `Script/Semantics/Spend.v` | Witness v0 / v1 and mint require abstract `sig_ok`. Not Ed25519 unforgeability. M126 |
| `witness_sig_ok_spends` | `Script/Semantics/Spend.v` | An auth-class lock with `sig_ok` succeeds. Not “scripts are verified”. M127 |

A same-named `oru_conservation_extended` also exists in
`proofs/ORU_Conservation/ORUConservation.v` (kernel_tx packaging).
Category A cites the FeeDomain identity.

A same-named `decode_encode_identity` also exists in
`Kernel/SerializationBoundary.v` (ORU list; Category B).
Category A cites the executable-kernel-state encode/decode.

## Category B — supporting mathematical lemma

Kept. Not the public soundness result for the domain.

| Theorem | File | Honest description |
|---|---|---|
| `apply_block_accepted_inputs_unique` | `Kernel/SafetyExtensions.v` | Converse of the `nodupb` guard: accepted block ⇒ unique inputs. Definitional; Category-A UTXO content remains `utxo_soundness` |
| `apply_block_rejects_duplicate_input` | `Kernel/SafetyExtensions.v` | Definitional unfolding of the `nodupb` guard in `apply_block`; the real Category-A UTXO content remains `utxo_soundness` |
| `asert_extract_reflects_quot` | `Kernel/ExtractionBoundary.v` | Checker reflects the Coq predicate |
| `asert_poly_fits_uint64_on_exponent` | `proofs/Pow_Correctness/ASERTCorrectness.v` | `asert_poly_fits_uint64` with the domain hypothesis discharged by `exp_rem_range`, hence holding for every exponent including a hostile one. Forwards; the killable content is in the parent |
| `asert_poly_pos_on_exponent` | `proofs/Pow_Correctness/ASERTCorrectness.v` | `asert_poly_pos` with the domain hypothesis discharged by `exp_rem_range`. Forwards |
| `bridge_is_inhabited` | `Kernel/KernelInputBridge.v` | Non-vacuity: the hypotheses of `input_ok_of_checks` are jointly satisfiable, so the bridge is not an empty implication |
| `burn_is_monotonic_supply_decrease` | `Economic/ORU/ORUSoundness.v` | ORU |
| `burn_only_removal` | `Kernel/EconomicInvariant/Economic.v` | Economic |
| `chain_ok_accepts_funded_spend` | `Kernel/Chain/ChainTransition.v` | Non-vacuity: the spend rejected above is accepted once an earlier block funds it. The pair is what shows acceptability reads the running state and not the initial one. M88 covers the rejection half |
| `chain_ok_inhabited` | `Kernel/Chain/ChainTransition.v` | Non-vacuity: acceptable runs of every length exist, so the chain theorems are not empty past the first block |
| `chain_ok_inhabited_with_issuance` | `Kernel/Chain/ChainTransition.v` | Non-vacuity: runs that actually issue exist, so the previous row is not an artefact of the all-zero input |
| `chain_ok_rejects_overspend` | `Kernel/Chain/ChainTransition.v` | Non-vacuity, other direction: the acceptability condition rejects a block spending more than the running stock. M88 |
| `chain_res_capped` | `Kernel/Chain/ChainTransition.v` | The cap holds at the tip, measured against the starting cap. Forwards to `chain_res_cap_fixed` and `kernel_chain_preserves_invariants` |
| `check_econ_false_iff` | `Kernel/EconomicInvariant/Economic.v` | Economic |
| `check_econ_is_not_constant` | `Kernel/EconomicInvariant/Economic.v` | Economic |
| `check_executable_kernel_false_iff` | `CheckerContract/CheckerSpecification.v` | Checker contract |
| `check_res_mint_bounded_by_genesis` | `Kernel/SafetyExtensions.v` | Executable form of the per-mint genesis cap (includes `0 <`) |
| `check_value_flow_false_iff` | `Kernel/Transaction/Validity.v` | Transaction |
| `check_value_flow_is_not_constant` | `Kernel/Transaction/Validity.v` | Transaction |
| `checker_is_not_constant` | `CheckerContract/CheckerSpecification.v` | Checker contract |
| `checker_survives_machine_step` | `CheckerContract/CheckerSpecification.v` | Checker contract |
| `classify_authority_free` | `proofs/Script_Correctness/ScriptCorrectness.v` | Payload independence at the asset level. Forwards to `mint_shape_classifies`; the killable content is `mint_script_of_is_mint` |
| `classify_payload_independent` | `Script/ScriptClassifier.v` | Payload independence at the script-type level. Same forwarding, same source of content |
| `collapsing_boundary_is_not_injective` | `ExtractionBoundary/ExtractionModel.v` | Extraction boundary |
| `collapsing_boundary_is_not_surjective` | `ExtractionBoundary/ExtractionModel.v` | Extraction boundary |
| `computed_ok_reflect` | `Kernel/KernelInputBridge.v` | Boolean packaging of the six Path A checks plus three state checks reflects the Prop `computed_ok`. Not a reflection of `input_ok`. Not a production-authority claim |
| `concrete_state_preserves_invariant` | `Refinement/ConcreteKernel/ConcreteKernelMapping.v` | Concrete Refinement |
| `decode_encode_identity` | `Kernel/SerializationBoundary.v` | ORU list encode/decode identity |
| `empty_transaction_has_no_fee` | `Kernel/Transaction/Validity.v` | Transaction |
| `encode_oru_injective` | `Kernel/SerializationBoundary.v` | No two distinct ORU states share a wire encoding. Proved from the encoded list, not through `decode_encode_identity`, so a collapsed field is caught here and not only one level below. M54 |
| `executable_apply_is_graph` | `ExecutionBoundary/ExecutableTransition.v` | Execution Boundary |
| `executable_chain_refines_kernel_chain` | `Kernel/Chain/ChainRefinement.v` | Executable run projects to the abstract run. Composes `executable_chain_commute` and `concrete_chain_commute`; the killable content is in those two |
| `executable_preservation_hypotheses_are_inhabited` | `ExecutionBoundary/ExecutableInvariance.v` | Execution boundary |
| `executable_transition_deterministic` | `ExecutionBoundary/ExecutableTransition.v` | Execution Boundary |
| `extraction_boundary_aligned` | `ExtractionBoundary/ExtractionModel.v` | Extraction boundary |
| `factor_range_on_exponent` | `proofs/Pow_Correctness/ASERTCorrectness.v` | `factor_range` with the domain hypothesis discharged by `exp_rem_range`. Forwards |
| `factor_wrap_step_is_one` | `proofs/Pow_Correctness/ASERTCorrectness.v` | Quantifies `factor_wrap_continuous`: the step taken at the wrap is one unit, the same as an ordinary interior step, so the seam is not merely absent but invisible |
| `fees_only_transfer` | `Kernel/EconomicInvariant/Economic.v` | Economic |
| `hash_less_target_equivalent` | `Pow/TargetRelation.v` | Definitional unfolding of `meets_target` vs `hash_le_target`. Abstract `H` only |
| `kernel_apply_is_graph` | `Kernel/UnifiedTransition.v` | Unified Kernel |
| `kernel_chain_app` | `Kernel/Chain/ChainTransition.v` | Running two chains in sequence is running their concatenation. Forwards to `kernel_chain_is_graph` |
| `kernel_chain_deterministic` | `Kernel/Chain/ChainTransition.v` | A run has one endpoint. Forwards to `kernel_chain_is_graph`, as `kernel_transition_deterministic` does one layer down |
| `kernel_transition_deterministic` | `proofs/KernelTransitionSoundness.v` | Unified Kernel |
| `ladder_order_immaterial` | `Script/ScriptClassifier.v` | Reordering the mint/converge/null-data ladder cannot change a class, because the recognizers are disjoint |
| `machine_chain_completes_on_idle` | `Kernel/Chain/ChainRefinement.v` | Non-vacuity: idle runs of every length complete on the machine, so the guard on `machine_chain_sound` is satisfiable at every chain length and not only for the empty chain |
| `machine_chain_refinement_is_partial` | `Kernel/Chain/ChainRefinement.v` | Non-vacuity, other direction: an acceptable run the machine cannot finish. The failure is representation, not a rule violation. M89 |
| `machine_chain_sound` | `Kernel/Chain/ChainRefinement.v` | End to end: a completed machine run decodes, projects onto the abstract run, and satisfies the invariants there. A bundle of the four Category A results it composes; no mutation can make its own proof fail |
| `machine_decode_is_partial` | `Runtime/MachineState.v` | Runtime boundary |
| `machine_refinement_is_partial` | `Runtime/MachineState.v` | Runtime refinement |
| `machine_step_is_graph` | `Execution/KernelMachine.v` | Execution |
| `mint_shape_classifies` | `Script/ScriptClassifier.v` | Every 20-byte mint payload lands in `T_MINT` |
| `no_shape_is_nonstandard` | `Script/ScriptClassifier.v` | A script matching any recognizer is never `T_NONSTANDARD` |
| `oru_domain_conservation` | `Economic/ORU/ORUSoundness.v` | ORU |
| `oru_extract_reflects_conservation` | `Kernel/ExtractionBoundary.v` | Checker reflects the Coq predicate |
| `oru_fees_are_transfer_only` | `Economic/ORU/ORUSoundness.v` | ORU |
| `oru_flow_condition_is_necessary` | `ExecutionBoundary/ExecutableInvariance.v` | Execution boundary |
| `pow_extract_reflects_target` | `Kernel/ExtractionBoundary.v` | Checker reflects the Coq predicate |
| `prefix_work_cancels` | `Kernel/Chain/ForkChoice.v` | Common-prefix work cannot flip the two-tip verdict, because work is a sum. Forwards to `chain_work_app` |
| `representable_does_not_imply_valid` | `Runtime/MachineState.v` | Runtime boundary |
| `res_extract_reflects_issuance` | `Kernel/ExtractionBoundary.v` | Checker reflects the Coq predicate |
| `res_mint_bounded_by_genesis` | `Kernel/SafetyExtensions.v` | Transitive corollary of `res_created` and Category-A `res_schedule_finite` |
| `runtime_apply_is_graph` | `Runtime/RuntimeTransition.v` | Runtime |
| `runtime_rejection_is_not_constant` | `Runtime/RuntimeFailure.v` | Runtime failure |
| `runtime_state_can_be_unrepresentable` | `Runtime/RuntimeState.v` | Runtime boundary |
| `runtime_step_deterministic` | `Runtime/RuntimeTransition.v` | Runtime |
| `sample_computed_ok` | `Kernel/KernelInputBridge.v` | Non-vacuity: `computed_ok_b` holds of the sample state and witness, so T2 is not an empty implication. Not a production-authority claim |
| `sample_witness_passes_every_check` | `Kernel/KernelInputBridge.v` | Non-vacuity: one witness passes all six Path A checks at once |
| `script_correctness` | `proofs/Script_Correctness/ScriptCorrectness.v` | Bundles the classifier range, surjectivity, payload independence and recognizer disjointness. The Class-A content is in those four |
| `script_execution_preserves_class` | `Script/Semantics/Execution.v` | Script |
| `script_extract_reflects_classify` | `Kernel/ExtractionBoundary.v` | Checker reflects the Coq predicate |
| `script_invalid_state_rejected` | `Script/Semantics/Execution.v` | Reject state stays rejected. Kernel subset only; not a Script VM |
| `script_trace_deterministic` | `Script/Semantics/ScriptTrace.v` | Kernel-subset classifier step is a function. Not a Script VM |
| `select_by_total_eq_suffix` | `Kernel/Chain/ForkChoice.v` | Comparing full-chain work equals comparing suffix work. Forwards to `prefix_work_cancels` |
| `select_suffix_is_a_or_b` | `Kernel/Chain/ForkChoice.v` | Selection returns one of the two suffixes. Totality of the two-tip rule |
| `selected_oru_follows_winner` | `Kernel/Chain/ForkChoice.v` | ORU issued on the live suffix is the winner's schedule. Forwards to `more_work_wins` |
| `shape_hits_le_one` | `Script/ScriptClassifier.v` | At most one shape matches. Counts the three Class-A disjointness results; adds no constraint they do not already carry |
| `surface_check_asert_eq` | `Kernel/SurfaceCorrespondence.v` | Surface [check_asert] agrees with the cone checker after [of_cone]. Pointwise [bool]; boxed [Z]. Not CertiCoq |
| `surface_check_oru_eq` | `Kernel/SurfaceCorrespondence.v` | Surface [check_oru] agrees with the cone checker after [of_cone]. Pointwise [bool]. Not CertiCoq |
| `surface_check_pow_eq` | `Kernel/SurfaceCorrespondence.v` | Surface [check_pow] agrees with the cone checker after [of_cone]. Pointwise [bool]. Not CertiCoq |
| `surface_check_res_eq` | `Kernel/SurfaceCorrespondence.v` | Surface [check_res] agrees with the cone checker after [of_cone]. RFC-020 body mint; no [draw_ok]. Not CertiCoq |
| `surface_check_script_eq` | `Kernel/SurfaceCorrespondence.v` | Surface [check_script] agrees with the cone checker after [of_cone]. Pointwise [bool]. Not CertiCoq |
| `surface_check_utxo_eq` | `Kernel/SurfaceCorrespondence.v` | Surface [check_utxo] agrees with the cone checker after [of_cone]. Pointwise [bool]. Not CertiCoq |
| `surface_validate_kernel_eq` | `Kernel/SurfaceCorrespondence.v` | Surface [validate_kernel] dispatches to the same six surface checkers. Not CertiCoq |
| `target_domain_restriction_is_necessary` | `RuntimeModel/IntegerBoundary.v` | Runtime integer domain |
| `time_error_domain_restriction_is_necessary` | `RuntimeModel/IntegerBoundary.v` | Runtime integer domain |
| `transaction_totals_nonneg` | `Kernel/Transaction/Validity.v` | Transaction |
| `u256_shr_le` | `Pow/Uint256Model.v` | A right shift does not increase a non-negative value. Killed by M151, which implements the shift in the other direction |
| `u256_add_wrap_ok` | `Pow/Uint256Model.v` | Wrap-add lands in `[0, 2^256)`. The only theorem in the file that says anything about the 256-bit range rather than about `Z`. Killed by M152, which drops the reduction |
| `utxo_extract_reflects_valid_tx` | `Kernel/ExtractionBoundary.v` | Checker reflects the Coq predicate |
| `w_add_negative_result_rejected` | `Runtime/MachineWord.v` | Runtime integer domain |
| `w_add_overflow_reachable` | `Runtime/MachineWord.v` | Runtime integer domain |
| `word_domain_strictly_smaller_than_Z` | `Runtime/MachineWord.v` | Runtime integer domain |

## Category C — boundary / documentation lemma

| Theorem | File | Honest description |
|---|---|---|
| `u256_add_assoc` | `Pow/Uint256Model.v` | Commutativity and associativity of `Z.add` under a `u256` name. In this model `u256` is `Z` and `u256_add` is `Z.add`, so the statement is a fact about the integers and says nothing about 256-bit arithmetic; no mutation of a kernel rule breaks it without redefining addition to be something other than addition. The range is carried by `u256_add_wrap_ok`, which is Category B and killed by M152 |
| `u256_add_comm` | `Pow/Uint256Model.v` | Commutativity and associativity of `Z.add` under a `u256` name. In this model `u256` is `Z` and `u256_add` is `Z.add`, so the statement is a fact about the integers and says nothing about 256-bit arithmetic; no mutation of a kernel rule breaks it without redefining addition to be something other than addition. The range is carried by `u256_add_wrap_ok`, which is Category B and killed by M152 |
| `check_pow_deterministic` | `Kernel/PowValidator.v` | Holds of any function, so it is not evidence about the one it names: `check_pow h = check_pow h`. `PowHeader` carries exactly the three fields `check_pow` reads, so there is no stronger depends-only-on statement available either. Kept as a boundary marker: the determinism that is in question is the C++ implementation's, which Coq cannot see |
| `encoding_deterministic` | `Kernel/SerializationBoundary.v` | Holds of any function, so it is not evidence about the one it names: `encode_oru s = encode_oru s`. The direction with content is `encode_oru_injective`, which is Category B and killed by M54 |
| `machine_step_deterministic_real` | `Runtime/MachineState.v` | Holds of any function, so it is not evidence about the one it names: it states that `machine_step_of` is a function, which is what `Definition` already means. The runtime nondeterminism it gestures at is outside the model; `machine_refinement_is_partial` is the statement that carries that boundary |
| `pow_asert_correctness` | `proofs/Pow_Correctness/ASERTCorrectness.v` | Holds of any function, so it is not evidence about the one it names: `deterministic_asert` unfolds to `anchor = parent -> asert_next_target anchor = asert_next_target parent`. Function-ness packaging only, as the inventory prose already said; Class A content is `negative_time_error_preserved` |
| `pow_hash_deterministic` | `proofs/Pow_Correctness/PowCorrectness.v` | Holds of any function, so it is not evidence about the one it names: congruence for the uninterpreted digest `H`. Over an abstract function the free facts are the only facts; `kernel_does_not_assume_sha3_security` is the honest marker |
| `pow_valid_deterministic` | `proofs/Pow_Correctness/PowCorrectness.v` | Holds of any function, so it is not evidence about the one it names: `pow_valid h 0 = pow_valid h 0`, inside the same abstract section |
| `same_state_same_encoding` | `Kernel/SerializationBoundary.v` | Holds of any function, so it is not evidence about the one it names: `s1 = s2 -> encode_oru s1 = encode_oru s2` is `f_equal`. Content is in `encode_oru_injective` |
| `asert_extraction_source_eq` | `Kernel/ExtractionBoundary.v` | Alias identity, proved by `reflexivity`: `extracted_check_asert` is *defined as* `ASERTValidator.check_asert`, so the statement is that the alias is the alias. It records that the extraction entry point is the kernel function rather than a re-implementation; it is not evidence about either, and a mutant cannot break it without breaking the definition it names |
| `extraction_source_equivalence` | `Kernel/ExtractionBoundary.v` | Conjunction of the six alias identities above, proved by `reflexivity`. Records that all six extraction entry points are the kernel functions. **Not** compiler equivalence, and no semantic content of its own |
| `oru_extraction_source_eq` | `Kernel/ExtractionBoundary.v` | Alias identity, proved by `reflexivity`: `extracted_check_oru` is *defined as* `ORUValidator.check_oru`, so the statement is that the alias is the alias. It records that the extraction entry point is the kernel function rather than a re-implementation; it is not evidence about either, and a mutant cannot break it without breaking the definition it names |
| `pow_extraction_source_eq` | `Kernel/ExtractionBoundary.v` | Alias identity, proved by `reflexivity`: `extracted_check_pow` is *defined as* `PowValidator.check_pow`, so the statement is that the alias is the alias. It records that the extraction entry point is the kernel function rather than a re-implementation; it is not evidence about either, and a mutant cannot break it without breaking the definition it names |
| `res_extraction_source_eq` | `Kernel/ExtractionBoundary.v` | Alias identity, proved by `reflexivity`: `extracted_check_res` is *defined as* `RESValidator.check_res`, so the statement is that the alias is the alias. It records that the extraction entry point is the kernel function rather than a re-implementation; it is not evidence about either, and a mutant cannot break it without breaking the definition it names |
| `script_extraction_source_eq` | `Kernel/ExtractionBoundary.v` | Alias identity, proved by `reflexivity`: `extracted_check_script` is *defined as* `ScriptValidator.check_script`, so the statement is that the alias is the alias. It records that the extraction entry point is the kernel function rather than a re-implementation; it is not evidence about either, and a mutant cannot break it without breaking the definition it names |
| `utxo_extraction_source_eq` | `Kernel/ExtractionBoundary.v` | Alias identity, proved by `reflexivity`: `extracted_check_utxo` is *defined as* `UTXOValidator.check_utxo`, so the statement is that the alias is the alias. It records that the extraction entry point is the kernel function rather than a re-implementation; it is not evidence about either, and a mutant cannot break it without breaking the definition it names |
| `asert_child_time_irrelevant` | `proofs/Pow_Correctness/ASERTCorrectness.v` | documentation-only, no semantic content (`asert_next_target i = asert_next_target i`) |
| `asert_poly_exceeds_int64` | `proofs/Pow_Correctness/ASERTCorrectness.v` | Implementation constraint, not a kernel invariant: at the top of the domain the polynomial exceeds 2^63, so the `__int128` promotion in `core/src/pow.cpp` is load-bearing and an `int64_t` evaluation would be signed overflow. Recorded so the requirement cannot be optimised away silently. M93 |
| `check_econ_accepts_something` | `Kernel/EconomicInvariant/Economic.v` | Economic |
| `check_econ_rejects_negative_subsidy` | `Kernel/EconomicInvariant/Economic.v` | Economic |
| `check_value_flow_accepts_something` | `Kernel/Transaction/Validity.v` | Transaction |
| `check_value_flow_rejects_overspend` | `Kernel/Transaction/Validity.v` | Transaction |
| `checker_accepts_something` | `CheckerContract/CheckerSpecification.v` | Checker contract |
| `checker_rejects_something` | `CheckerContract/CheckerSpecification.v` | Checker contract |
| `concrete_step_refines_abstract_step` | `Refinement/ConcreteKernel/ConcreteKernelTransition.v` | Concrete Refinement |
| `crypto_assumptions_are_not_security_proofs` | `Kernel/CryptoAssumptions.v` | Central inventory: hash / keccak / signature / PoW digest are assumptions, not proofs |
| `crypto_keccak_not_verified` | `Kernel/CryptoAssumptions.v` | keccak implementation not verified |
| `encoding_contract_proved_crypto_not_proved` | `Kernel/SerializationBoundary.v` | Encoding contract proved; crypto and full extraction refinement not proved |
| `executable_layer_in_scope` | `Kernel/RefinementBoundary.v` | Same marker, Coq-validator field |
| `executable_step_refines_concrete_step` | `ExecutionBoundary/ExecutableTransition.v` | Execution Boundary |
| `execution_projection_preserves_invariants` | `ExecutionBoundary/ConcreteExecutionState.v` | Execution Boundary |
| `extraction_boundary_declared` | `Kernel/ExtractionBoundary.v` | Compiler/runtime remain outside the proof scope |
| `kernel_boundary_closed` | `Kernel/RefinementBoundary.v` | Documentation marker (`True`). Not extraction or compiler proof |
| `longer_need_not_win` | `Kernel/Chain/ForkChoice.v` | Witness: one block of work 3 beats two of work 1. Why `more_work_wins` is not longest-chain. Not OP-R1 |
| `machine_step_deterministic` | `Execution/KernelMachine.v` | Execution |
| `negative_vectors_rejected_in_coq` | `Kernel/VerifiedCompilerBoundary.v` | INVALID-001..005 are `false` in Coq (`vm_compute`). Not a binary proof |
| `no_cpp_equivalence_claim` | `Audit/KernelNonClaims.v` | Machine-checked non-claim (`= false`) |
| `res_reorg_orphans_change_pool` | `Kernel/Chain/ReorgReplay.v` | Non-vacuity of RES rewind: a positive orphan mint is visible if it is not disconnected. Why `res_reorg_rewind_eq` is not `x = x`. Not OP-R1 |
| `runtime_step_refines_executable_step` | `Runtime/RuntimeTransition.v` | Runtime |
| `sample_input_is_accepted` | `ExecutionBoundary/ExecutableInvariance.v` | Execution boundary |
| `sample_state_satisfies_invariants` | `ExecutionBoundary/ExecutableInvariance.v` | Execution boundary |
| `script_step_deterministic` | `Script/Semantics/Execution.v` | Script |
| `specification_layer_in_scope` | `Kernel/RefinementBoundary.v` | Same marker, specification field |
| `we_do_not_claim_binary_semantic_equivalence` | `Audit/KernelNonClaims.v` | Machine-checked non-claim (`= false`) |
| `we_do_not_claim_compiler_correctness` | `Audit/KernelNonClaims.v` | Machine-checked non-claim (`= false`) |
| `we_do_not_claim_cpp_equivalence` | `Audit/KernelNonClaims.v` | Machine-checked non-claim (`= false`) |
| `we_do_not_claim_extraction_equivalence` | `Audit/KernelNonClaims.v` | Machine-checked non-claim (`= false`) |
| `we_do_not_claim_extraction_full_equivalence` | `Kernel/ExtractionBoundary.v` | Full OCaml refinement is `false` |
| `we_do_not_claim_verified_compiler_in_tcb` | `Kernel/VerifiedCompilerBoundary.v` | CompCert / CakeML not in the current TCB; compiler correctness remains `false` |

## Reading rule

Do not delete a theorem. Do not inflate theorem importance.
A Closed class-B or class-C lemma is still a real Coq lemma. It is
not permission to strengthen the public claim.

Phase 7 Category C bools live in `certicoq/proofs/`
(`CompiledCheckerSoundness.v`). The cone classification walk
skips `certicoq` (`SKIP_PARTS`). They are **not** rows in this
table: they are not cone theorems and must not force inventory
/ Print Assumptions over the sidecar.

CI: `scripts/check_theorem_classification.py` greps every
`(theorem, file)` pair, requires Category A name-sets of the
canonical table and the secondary view to be identical, scans
`Kernel/` and `proofs/` for unclassified `Theorem`/`Corollary`
names (must be in `release/UnclassifiedAllowlist.md`), and emits
the Print Assumptions driver.

Secondary view: `../evidence/TheoremClassification.md`.
Category A is authoritative and identical across the two files;
B/C sets intentionally differ.

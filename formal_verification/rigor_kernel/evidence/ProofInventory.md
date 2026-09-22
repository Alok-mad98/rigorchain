# Proof Inventory

All listed theorems compile with `Qed` and `Print Assumptions` reports
**Closed under the global context**. No `Admitted`, `Axiom`, or extra
`Parameter` is used in the kernel cone.

Scope: consensus kernel only. C++ equivalence and full-chain validity
are non-goals for every row.

## Top-level theorems

| theorem | file | kernel area | assumptions |
|---|---|---|---|
| utxo_soundness | UTXOSoundness.v | UTXO | Closed |
| no_hidden_inflation | ORUConservation.v | ORU | Closed |
| oru_conservation_extended | ORUConservation.v | ORU | Closed |
| res_issuance_soundness | RESIssuance.v | RES | Closed |
| recognizer_pairwise_disjoint | ScriptCorrectness.v | Script (class A) | Closed |
| script_correctness | ScriptCorrectness.v | Script (class B supporting lemma) | Closed |
| pow_valid_rejects_above_target | PowCorrectness.v | PoW (class A) | Closed |
| pow_valid_deterministic | PowCorrectness.v | PoW (class B; abstract relation only) | Closed |
| pow_asert_correctness | ASERTCorrectness.v | ASERT | Closed |
| block_transition_utxo_soundness | UTXOTransitionSoundness.v | UTXO (class A) | Closed |
| recognizer_trace_disjoint | Script/Semantics/ScriptTrace.v | Script (class A) | Closed |
| target_monotonic | Pow/TargetRelation.v | PoW (class A) | Closed |
| pow_acceptance_monotonic | Pow/TargetRelation.v | PoW (class A) | Closed |
| time_error_direction_preserved | Pow/ASERTBoundary.v | ASERT (class A) | Closed |
| epoch_supply_transition_sound | Supply/SupplyTransition.v | Supply (class A) | Closed |
| transaction_value_conservation | Kernel/Transaction/Validity.v | Transaction (class A) | Closed |
| block_acceptance_sound | Kernel/Block/Acceptance.v | Block composition (class A) | Closed |
| economic_no_hidden_creation | Kernel/EconomicInvariant/Economic.v | Economic (class A) | Closed |
| script_step_deterministic | Script/Semantics/Execution.v | Script subset (class A) | Closed |
| pow_pipeline_sound | Pow/Acceptance.v | PoW pipeline (class A) | Closed |
| asert_boundary_complete | Pow/ASERTMatrix.v | ASERT matrix (class A) | Closed |
| refinement_preservation | Refinement/TransitionRefinement.v | Refinement skeleton (class A) | Closed |
| machine_step_deterministic | Execution/KernelMachine.v | Machine subset (class A) | Closed |
| oru_domain_conservation | Economic/ORU/ORUSoundness.v | ORU domain (class A) | Closed |
| res_creation_separate_from_oru_transfer | Economic/DomainSeparation.v | Domain split (class A) | Closed |
| economic_refinement_preserves_conservation | Refinement/EconomicRefinement.v | Economic refine (class A) | Closed |
| kernel_step_refinement_composes | Refinement/KernelComposition.v | Kernel compose (class A) | Closed |
| kernel_step_preserves_invariants | proofs/KernelTransitionSoundness.v | Unified Kernel (class A) | Closed |
| oru_reorg_rewind_eq | Kernel/Chain/ReorgReplay.v | Dual-supply ORU rewind (class A) | Closed |
| oru_reorg_surviving_pool | Kernel/Chain/ReorgReplay.v | Dual-supply ORU rewind (class A) | Closed |
| oru_subsidy_fold_closed | Supply/SupplySchedule.v | RFC-020 ORU fold n·128 (class A) | Closed |
| chain_work_app | Kernel/Chain/ForkChoice.v | Most-work selection (class A) | Closed |
| more_work_wins | Kernel/Chain/ForkChoice.v | Most-work selection (class A) | Closed |
| equal_work_keeps_active | Kernel/Chain/ForkChoice.v | Most-work selection (class A) | Closed |
| res_reorg_rewind_eq | Kernel/Chain/ReorgReplay.v | RFC-020 RES rewind (class A) | Closed |
| res_reorg_surviving_pool | Kernel/Chain/ReorgReplay.v | RFC-020 RES rewind (class A) | Closed |
| res_reorg_replay_eq | proofs/RES_Issuance/RESIssuance.v | RFC-020 RES rewind, same-height (class A) | Closed |
| kernel_chain_preserves_invariants | Kernel/Chain/ChainTransition.v | Chain-level kernel run (class A) | Closed |
| kernel_chain_states_invariant | Kernel/Chain/ChainTransition.v | Chain-level kernel run (class A) | Closed |
| reachable_preserves_invariants | Kernel/Chain/ChainTransition.v | Chain-level kernel run (class A) | Closed |
| kernel_chain_is_graph | Kernel/Chain/ChainTransition.v | Chain-level kernel run (class A) | Closed |
| chain_oru_conservation | Kernel/Chain/ChainTransition.v | Chain-level kernel run (class A) | Closed |
| chain_no_hidden_creation | Kernel/Chain/ChainTransition.v | Chain-level kernel run (class A) | Closed |
| chain_res_cap_fixed | Kernel/Chain/ChainTransition.v | Chain-level kernel run (class A) | Closed |
| chain_res_monotone | Kernel/Chain/ChainTransition.v | Chain-level kernel run (class A) | Closed |
| concrete_chain_commute | Kernel/Chain/ChainRefinement.v | Chain-level kernel run (class A) | Closed |
| executable_chain_commute | Kernel/Chain/ChainRefinement.v | Chain-level kernel run (class A) | Closed |
| executable_chain_preserves_invariants | Kernel/Chain/ChainRefinement.v | Chain-level kernel run (class A) | Closed |
| machine_chain_refines_executable_chain | Kernel/Chain/ChainRefinement.v | Chain-level kernel run (class A) | Closed |
| kernel_transition_deterministic | proofs/KernelTransitionSoundness.v | Unified Kernel (class A) | Closed |
| transaction_value_flow_preserves_economic_state | Economic/UTXOEconomicBridge.v | UTXO–economic bridge (class A) | Closed |
| concrete_step_refines_abstract_step | Refinement/ConcreteKernel/ConcreteKernelTransition.v | Concrete refine (class A) | Closed |
| concrete_state_preserves_invariant | Refinement/ConcreteKernel/ConcreteKernelMapping.v | Concrete lift (class A) | Closed |
| execution_projection_preserves_invariants | ExecutionBoundary/ConcreteExecutionState.v | Execution proj (class A) | Closed |
| executable_step_refines_concrete_step | ExecutionBoundary/ExecutableTransition.v | Execution refine (class A) | Closed |
| executable_transition_deterministic | ExecutionBoundary/ExecutableTransition.v | Execution determ (class A) | Closed |
| decode_encode_identity | SerializationBoundary/KernelSerialization.v | Kernel-state serialize (class A) | Closed |
| extraction_interface_bijective | ExtractionBoundary/ExtractionModel.v | Extraction iface (class A) | Closed |
| operation_trace_preserved | ExtractionBoundary/ExtractionTrace.v | Extraction trace (class A) | Closed |
| integer_range_safe | RuntimeModel/IntegerBoundary.v | Kernel int bound (class A) | Closed |
| golden_negative_vectors_rejected | ExtractionBoundary/GoldenNegatives.v | V1–V6 reject (class A) | Closed |
| runtime_projection_preserves_kernel_domain | Runtime/RuntimeState.v | Runtime proj (class A) | Closed |
| runtime_step_refines_executable_step | Runtime/RuntimeTransition.v | Runtime refine (class A) | Closed |
| runtime_step_deterministic | Runtime/RuntimeTransition.v | Runtime determ (class A) | Closed |
| invalid_kernel_input_rejected | Runtime/RuntimeFailure.v | Invalid input (class A) | Closed |
| runtime_failure_preserved | Runtime/RuntimeFailure.v | Failure path (class A) | Closed |
| runtime_domain_closed | RuntimeModel/IntegerBoundary.v | Kernel domain (class A) | Closed |
| checker_accepts_valid_kernel_state | CheckerContract/CheckerSpecification.v | Checker accept (class A) | Closed |
| checker_rejects_invalid_kernel_state | CheckerContract/CheckerSpecification.v | Checker reject (class A) | Closed |

Paths are under `formal_verification/rigor_kernel/proofs/`.

---

### utxo_soundness

**Statement.** If `valid_tx tx` and `utxo_wf u`, then there exists `u'`
such that `apply_tx u tx = Some u'`, `utxo_wf u'`, `inputs_consumed tx u u'`,
and `outputs_created tx u u'`.

**Assumptions.** Structural validity of `tx` (non-coinbase, non-empty
in/out, `NoDup` inputs, money-range outputs, inputs do not collide with
new outpoints) and `NoDup` keys on the association-list UTXO set.

**Non-goals.** C++ `ConnectBlock` equivalence; signature checks; script
execution; mempool policy; cross-block chain validity.

---

### no_hidden_inflation

**Statement.** If `valid_tx tx` then `oru_input tx = oru_output tx + fee tx`.

**Assumptions.** `valid_tx` is `valid_oru_tx` on the embedded ORU legs
(`oru_out <= oru_in`). `fee` is defined as `oru_in - oru_out`.

**Non-goals.** Reconstructing circulating supply from a live UTXO set;
C++ `EvaluateSupply` equivalence; hidden inflation via a non-modelled
path.

---

### oru_conservation_extended

**Statement.** If `valid_tx tx` then
`new_supply tx = old_supply tx + subsidy tx + fees tx - burn tx`.

**Assumptions.** `new_supply` is `ORU_supply_transition`, which is the
same identity as `oru_claimable = (fees - burn) + subsidy` when the
coinbase claims exactly the claimable amount.

**Non-goals.** Runtime fee-burn policy outside the RES-transfer domain
model; C++ supply-counter equivalence.

---

### res_issuance_soundness

**Statement.** If `res_created tx`, `draw_eligible tx`, and
`h = rt_height tx`, then the tx is coinbase, `valid_res_mint`,
`mint_amount tx <= W h`, and `draw_eligible tx`.

**Assumptions.** `res_created` already packages coinbase, mint shape,
positivity, and the `W(h)` bound. **Draw eligibility is a hypothesis**
(C-4). The bound is the schedule `W`, not a running `S_RES` scalar.

**Non-goals.** `draw_won → owner`; ConnectBlock rejection on observational
`S_RES`; C++ mint-script equivalence.

---

### script_correctness

**Class B** supporting lemma (kept; not the public script invariant).
It used to bundle three statements of the form `x = x` behind an
existential witnessed by `classify s`. The Class-A script content is
`classify_total`, `classify_range_exact`, `classify_type_total` and the
three recognizer-disjointness results.

**Statement.** The asset range is exactly the four classes, every one
of them is produced by some script, no 20-byte payload changes a
class, and at most one shape recognizer fires on any script.

**Assumptions.** `classify` is `classify_asset` (a total function of
script bytes). No height, key, or chain state is an argument.

**Non-goals.** Agreement with `rigor_classify.cpp`; a script interpreter;
signature verification.

---

### pow_valid_deterministic

**Class B** supporting lemma (kept; abstract PoW relation only).
**NOT** a digest implementation proof. Class-A PoW content is
`pow_valid_rejects_above_target`.

**Statement.** For every header `h`, `pow_valid h 0 = pow_valid h 0`
(function-ness). The same module also proves same header ⇒ same digest,
and digest-above-target rejection, all over an abstract `H`.

**Assumptions.** `H : list byte -> list byte` is a Coq function
(uninterpreted). No theorem unfolds SHA3-256 / Keccak (C-1).

**Non-goals.** Collision resistance; concrete `"RGRPOW01"` digest values
as a refinement; C++ `CheckProofOfWork` equivalence.

---

### pow_asert_correctness

**Statement.** For all `anchor parent`, `deterministic_asert anchor parent`
(`anchor = parent → asert_next_target anchor = asert_next_target parent`).

**Assumptions.** The ASERT integer model (`Z.quot` for `exp_q`, `Z.div`
for `num_shifts`). Child `nTime` is not an input.

**Non-goals.** C++ `__int128` / `arith_uint256` equivalence; full
target-monotonicity under remainder wrap (stated only as exponent
monotonicity).

---

## Supporting theorems cited by coverage

| theorem | file | kernel area | assumptions |
|---|---|---|---|
| apply_tx_preserves_wf | UTXOSoundness.v | UTXO K-U-3 | Closed |
| apply_tx_inputs_consumed | UTXOSoundness.v | UTXO K-U-6 | Closed |
| apply_tx_outputs_created | UTXOSoundness.v | UTXO K-U-7 | Closed |
| apply_tx_frame | UTXOSoundness.v | UTXO K-U-8 | Closed |
| apply_block_no_double_spend | UTXOSoundness.v | UTXO K-U-12 | Closed |
| oru_conservation_tx | ORUConservation.v | ORU K-O-2 | Closed |
| fee_split_kernel | ORUConservation.v | ORU K-O-4 | Closed |
| oru_conservation_with_burn | ORUConservation.v | ORU K-O-5 | Closed |
| no_unauthorized_oru_creation_kernel | ORUConservation.v | ORU K-O-6 | Closed |
| oru_accounting_deterministic_kernel | ORUConservation.v | ORU K-O-11 | Closed |
| non_coinbase_cannot_create_res | RESIssuance.v | RES K-R-2 | Closed |
| res_mint_requires_coinbase | RESIssuance.v | RES K-R-5 | Closed |
| res_mint_amount_bounded | RESIssuance.v | RES K-R-7 | Closed |
| invalid_mint_rejected | RESIssuance.v | RES K-R-8 | Closed |
| pow_creates_no_res | RESIssuance.v | RES K-R-9 | Closed |
| res_cumulative_cap | RESIssuance.v | RES K-R-10 | Closed |
| classify_total | ScriptClassifier.v | Script K-S-1 | Closed |
| classify_range_exact | ScriptClassifier.v | Script K-S-1 | Closed |
| classify_type_total | ScriptCorrectness.v | Script K-S-1 | Closed |
| classify_extensional | ScriptClassifier.v | Script K-S-2 | Closed |
| recognizer_pairwise_disjoint | ScriptCorrectness.v | Script K-S-4 | Closed |
| witness_ladder_terminal_kernel | ScriptCorrectness.v | Script K-S-5 | Closed |
| classify_no_interpreter | ScriptCorrectness.v | Script K-S-7 | Closed |
| classify_authority_free | ScriptCorrectness.v | Script K-S-8 | Closed |
| pow_preimage_length | PowCorrectness.v | PoW K-P-2 | Closed |
| pow_preimage_tag_prefix | PowCorrectness.v | PoW K-P-3 | Closed |
| serialize_header80_injective | PowCorrectness.v | PoW K-P-4 | Closed |
| pow_hash_deterministic | PowCorrectness.v | PoW K-P-5 | Closed |
| pow_valid_rejects_above_target | PowCorrectness.v | PoW K-P-7 | Closed |
| pow_golden_vectors | PowCorrectness.v | PoW K-P-10 | Closed |
| factor_range | ASERTCorrectness.v | ASERT K-A-4 | Closed |
| clamp_to_prev_bounds | ASERTCorrectness.v | ASERT K-A-6 | Closed |
| asert_next_bits_deterministic | ASERTCorrectness.v | ASERT K-A-7 | Closed |
| asert_child_time_irrelevant | ASERTCorrectness.v | ASERT K-A-9 | Closed |
| asert_golden_vectors | ASERTCorrectness.v | ASERT K-A-11 | Closed |
| res_schedule_finite | SupplySchedule.v | Supply | Closed |
| compact_decode_lower_bound | CompactBits.v | ASERT | Closed |
| negative_time_error_preserved | ASERTModel.v | ASERT | Closed |

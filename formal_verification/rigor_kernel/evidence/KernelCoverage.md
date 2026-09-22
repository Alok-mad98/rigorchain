# Kernel Coverage Matrix

Maps P0-106-R3 requirement IDs to the Coq theorem that discharges
them and the proof file that contains the theorem.

```
P0-106 requirement
        |
        v
Coq theorem
        |
        v
Proof file
```

Coverage is **kernel-model coverage**, not C++ line coverage.

## UTXO

| P0-106 ID | Coq theorem | Proof file |
|---|---|---|
| K-U-3 | apply_tx_preserves_wf | proofs/UTXO_Soundness/UTXOSoundness.v |
| K-U-6 | apply_tx_inputs_consumed | proofs/UTXO_Soundness/UTXOSoundness.v |
| K-U-7 | apply_tx_outputs_created | proofs/UTXO_Soundness/UTXOSoundness.v |
| K-U-8 | apply_tx_frame | proofs/UTXO_Soundness/UTXOSoundness.v |
| K-U-12 | apply_block_no_double_spend | proofs/UTXO_Soundness/UTXOSoundness.v |
| K-U-13 | utxo_soundness | proofs/UTXO_Soundness/UTXOSoundness.v |
| K-U-13+ | block_transition_utxo_soundness | proofs/UTXO_Soundness/UTXOTransitionSoundness.v |

Top-level: `utxo_soundness` packages K-U-3 / K-U-6 / K-U-7.
`block_transition_utxo_soundness` is the block-transition extension
(accepted transition ⇒ no double-spend, `utxo_wf`, output range).

## ORU

| P0-106 ID | Coq theorem | Proof file |
|---|---|---|
| K-O-2 | oru_conservation_tx | proofs/ORU_Conservation/ORUConservation.v |
| K-O-4 | fee_split_kernel | proofs/ORU_Conservation/ORUConservation.v |
| K-O-5 | oru_conservation_with_burn | proofs/ORU_Conservation/ORUConservation.v |
| K-O-6 | no_unauthorized_oru_creation_kernel | proofs/ORU_Conservation/ORUConservation.v |
| K-O-10 | no_hidden_inflation | proofs/ORU_Conservation/ORUConservation.v |
| K-O-11 | oru_accounting_deterministic_kernel | proofs/ORU_Conservation/ORUConservation.v |

`oru_conservation_extended` restates the supply identity used by K-O-5 / K-O-10.

## RES

| P0-106 ID | Coq theorem | Proof file |
|---|---|---|
| K-R-2 | non_coinbase_cannot_create_res | proofs/RES_Issuance/RESIssuance.v |
| K-R-5 | res_mint_requires_coinbase | proofs/RES_Issuance/RESIssuance.v |
| K-R-7 | res_mint_amount_bounded | proofs/RES_Issuance/RESIssuance.v |
| K-R-8 | invalid_mint_rejected | proofs/RES_Issuance/RESIssuance.v |
| K-R-9 | pow_creates_no_res | proofs/RES_Issuance/RESIssuance.v |
| K-R-10 | res_cumulative_cap | proofs/RES_Issuance/RESIssuance.v |
| K-R-11 | res_issuance_soundness | proofs/RES_Issuance/RESIssuance.v |

`W(h)` is defined in `Supply/SupplySchedule.v`. No theorem claims a
runtime `S_RES` ConnectBlock reject.

## Script

| P0-106 ID | Coq theorem | Proof file |
|---|---|---|
| K-S-1 | classify_total / classify_range_exact | Script/ScriptClassifier.v |
| K-S-1 | classify_type_total | proofs/Script_Correctness/ScriptCorrectness.v |
| K-S-2 | classify_extensional | Script/ScriptClassifier.v |
| K-S-4 | recognizer_pairwise_disjoint / classify_type_disjoint | proofs/Script_Correctness/ScriptCorrectness.v |
| K-S-5 | witness_ladder_terminal_kernel | proofs/Script_Correctness/ScriptCorrectness.v |
| K-S-7 | classify_no_interpreter | proofs/Script_Correctness/ScriptCorrectness.v |
| K-S-8 | classify_authority_free | proofs/Script_Correctness/ScriptCorrectness.v |
| K-S-11 | script_correctness | proofs/Script_Correctness/ScriptCorrectness.v |

Kernel-subset traces (`ScriptState` / `ScriptStep` / `ScriptTrace`):
`script_trace_deterministic` (B), `recognizer_trace_disjoint` (A).
Not a Bitcoin Script VM.

Classifier refinement against `rigor_classify.cpp` is **not** covered.

## PoW

| P0-106 ID | Coq theorem | Proof file |
|---|---|---|
| K-P-2 | pow_preimage_length | proofs/Pow_Correctness/PowCorrectness.v |
| K-P-3 | pow_preimage_tag_prefix | proofs/Pow_Correctness/PowCorrectness.v |
| K-P-4 | serialize_header80_injective | proofs/Pow_Correctness/PowCorrectness.v |
| K-P-5 | pow_hash_deterministic | proofs/Pow_Correctness/PowCorrectness.v |
| K-P-7 | pow_valid_rejects_above_target | proofs/Pow_Correctness/PowCorrectness.v |
| K-P-10 | pow_golden_vectors | proofs/Pow_Correctness/PowCorrectness.v |

`pow_valid_rejects_above_target` is the class-A PoW invariant.
`pow_valid_deterministic` is a class-B supporting lemma (abstract
PoW relation only; **not** a digest implementation proof).
Target ordering (`Pow/TargetRelation.v`): `target_monotonic` and
`pow_acceptance_monotonic` (A); `hash_less_target_equivalent` (B).
Digests are abstract `H` (C-1). SHA3 / hash implementation is **not**
claimed. K-P-10 is `Example consistency_check` only — not a refinement.

## ASERT

| P0-106 ID | Coq theorem | Proof file |
|---|---|---|
| K-A-4 | factor_range | proofs/Pow_Correctness/ASERTCorrectness.v |
| K-A-6 | clamp_to_prev_bounds | proofs/Pow_Correctness/ASERTCorrectness.v |
| K-A-7 | asert_next_bits_deterministic | proofs/Pow_Correctness/ASERTCorrectness.v |
| K-A-9 | asert_child_time_irrelevant | proofs/Pow_Correctness/ASERTCorrectness.v |
| K-A-11 | asert_golden_vectors | proofs/Pow_Correctness/ASERTCorrectness.v |
| K-A-12 | pow_asert_correctness | proofs/Pow_Correctness/ASERTCorrectness.v |

Operator split lives in `Pow/ASERTModel.v` (`Z.quot` vs `Z.div`).
Boundary direction (`Pow/ASERTBoundary.v`): `time_error_direction_preserved`
uses `Z.quot` on negative / zero / positive time-error.
Compact lower bound is `compact_decode_lower_bound` in `Pow/CompactBits.v`.

`W(h)` epoch prefixes: `epoch_supply_transition_sound` in
`Supply/SupplyTransition.v` (A). Schedule identity only; **not** a
runtime `S_RES` reject.

## Category counts (P0-120~125)

| Class | Before deepening | After deepening |
|---|---|---|
| A | 13 | 19 |
| B | 29 | 31 |

Kernel invariant coverage strengthened.
Formal kernel scope expanded.

See `release/KernelDeepeningReport.md`.

## Completeness hardening (P0-126~130)

| Area | Coq theorem | Proof file |
|---|---|---|
| Transaction | transaction_value_conservation / transaction_no_negative_value / transaction_fee_separation | Kernel/Transaction/Validity.v |
| Block composition | block_acceptance_sound | Kernel/Block/Acceptance.v |
| Economic | economic_no_hidden_creation / fees_only_transfer / burn_only_removal / subsidy_schedule_bound | Kernel/EconomicInvariant/Economic.v |
| Script subset | script_step_deterministic / script_execution_preserves_class | Script/Semantics/Execution.v |
| PoW pipeline | pow_pipeline_sound | Pow/Acceptance.v |
| ASERT matrix | asert_boundary_complete | Pow/ASERTMatrix.v |

Block acceptance is model composition (UTXO + PoW + supply + script).
Not a node.

## Category counts (P0-126~130)

| Class | After deepening | After completeness |
|---|---|---|
| A | 19 | 31 |
| B | 31 | 32 |

Kernel safety coverage expanded.
Consensus kernel model completeness strengthened.

See `release/KernelCompletenessReport.md`.

## Refinement foundation (P0-131~135)

| Area | Coq theorem | Proof file |
|---|---|---|
| Refinement | refinement_preservation | Refinement/TransitionRefinement.v |
| Machine | machine_step_deterministic / machine_state_preserves_invariant | Execution/KernelMachine.v |

Concrete steps are Coq models. Not production code. Not CompCert.

## Category counts (P0-131~135)

| Class | After completeness | After refinement foundation |
|---|---|---|
| A | 31 | 34 |
| B | 32 | 32 |

Formal refinement framework established.

See `release/FormalTrustRoadmap.md`.

## ORU first-class domain (P0-136~140)

| Area | Coq theorem | Proof file |
|---|---|---|
| ORU domain | oru_domain_conservation / oru_fees_are_transfer_only / burn_is_monotonic_supply_decrease / subsidy_is_only_creation_source | Economic/ORU/ORUSoundness.v |
| Domain split | res_creation_separate_from_oru_transfer | Economic/DomainSeparation.v |
| Econ refine | economic_refinement_preserves_conservation | Refinement/EconomicRefinement.v |
| Kernel compose | kernel_step_refinement_composes | Refinement/KernelComposition.v |

ORU conservation and RES issuance are separately modeled and verified.
ORU Domain = Independent Verified Domain.

## Category counts (P0-136~140)

| Class | After refinement foundation | After ORU domain |
|---|---|---|
| A | 34 | 41 |
| B | 32 | 32 |

## Unified Kernel Transition (P0-141~145)

| Area | Coq theorem | Proof file |
|---|---|---|
| Invariant bundle | kernel_step_preserves_invariants | proofs/KernelTransitionSoundness.v |
| Chain-level run | kernel_chain_preserves_invariants | Kernel/Chain/ChainTransition.v |
| Chain-level run | kernel_chain_states_invariant | Kernel/Chain/ChainTransition.v |
| Chain-level run | reachable_preserves_invariants | Kernel/Chain/ChainTransition.v |
| Chain-level run | kernel_chain_is_graph | Kernel/Chain/ChainTransition.v |
| Chain-level run | chain_oru_conservation | Kernel/Chain/ChainTransition.v |
| Chain-level run | chain_no_hidden_creation | Kernel/Chain/ChainTransition.v |
| Chain-level run | chain_res_cap_fixed | Kernel/Chain/ChainTransition.v |
| Chain-level run | chain_res_monotone | Kernel/Chain/ChainTransition.v |
| Chain-level run | concrete_chain_commute | Kernel/Chain/ChainRefinement.v |
| Chain-level run | executable_chain_commute | Kernel/Chain/ChainRefinement.v |
| Chain-level run | executable_chain_preserves_invariants | Kernel/Chain/ChainRefinement.v |
| Chain-level run | machine_chain_refines_executable_chain | Kernel/Chain/ChainRefinement.v |
| Determinism | kernel_transition_deterministic | proofs/KernelTransitionSoundness.v |
| Value flow | transaction_value_flow_preserves_economic_state | Economic/UTXOEconomicBridge.v |

Unified consensus kernel transition invariants are formally verified.
Coq model only. Not a node; not C++ refinement.

## Category counts (P0-141~145)

| Class | After ORU domain | After unified transition |
|---|---|---|
| A | 41 | 44 |
| B | 32 | 32 |

## Concrete refinement (P0-120)

| Area | Coq theorem | Proof file |
|---|---|---|
| Step refine | concrete_step_refines_abstract_step | Refinement/ConcreteKernel/ConcreteKernelTransition.v |
| Invariant lift | concrete_state_preserves_invariant | Refinement/ConcreteKernel/ConcreteKernelMapping.v |

Coq abstract → Coq concrete only. OP-R2 (implementation) remains open.

## Category counts (concrete refinement)

| Class | After unified transition | After concrete refinement |
|---|---|---|
| A | 44 | 46 |
| B | 32 | 32 |

## Execution boundary (P0-121)

| Area | Coq theorem | Proof file |
|---|---|---|
| Exec projection | execution_projection_preserves_invariants | ExecutionBoundary/ConcreteExecutionState.v |
| Exec refine | executable_step_refines_concrete_step | ExecutionBoundary/ExecutableTransition.v |
| Exec determ | executable_transition_deterministic | ExecutionBoundary/ExecutableTransition.v |
| Serialize | decode_encode_identity | SerializationBoundary/KernelSerialization.v |

Level 4.5 Formal Kernel Execution Boundary. Coq model only.

## Category counts (P0-121)

| Class | After concrete refinement | After execution boundary |
|---|---|---|
| A | 46 | 50 |
| B | 32 | 32 |

## Extraction hardening (P0-124~126)

| Area | Coq theorem | Proof file |
|---|---|---|
| Interface | extraction_interface_bijective | ExtractionBoundary/ExtractionModel.v |
| Trace | operation_trace_preserved | ExtractionBoundary/ExtractionTrace.v |
| Integer | integer_range_safe | RuntimeModel/IntegerBoundary.v |
| Negatives | golden_negative_vectors_rejected | ExtractionBoundary/GoldenNegatives.v |

Level 4.8 Extraction Boundary Hardened Kernel. Compiler remains OPEN.

## Category counts (P0-124~126)

| Class | After execution boundary | After extraction hardening |
|---|---|---|
| A | 50 | 54 |
| B | 32 | 32 |

## Runtime trust boundary (P0-127~129)

| Area | Coq theorem | Proof file |
|---|---|---|
| Projection | runtime_projection_preserves_kernel_domain | Runtime/RuntimeState.v |
| Refine | runtime_step_refines_executable_step | Runtime/RuntimeTransition.v |
| Determ | runtime_step_deterministic | Runtime/RuntimeTransition.v |
| Invalid | invalid_kernel_input_rejected | Runtime/RuntimeFailure.v |
| Failure | runtime_failure_preserved | Runtime/RuntimeFailure.v |
| Domain | runtime_domain_closed | RuntimeModel/IntegerBoundary.v |
| Accept | checker_accepts_valid_kernel_state | CheckerContract/CheckerSpecification.v |
| Reject | checker_rejects_invalid_kernel_state | CheckerContract/CheckerSpecification.v |

Level 4.9 Runtime Trust Boundary Hardened Kernel. Compiler remains OPEN.

## Category counts (P0-127~129)

| Class | After extraction hardening | After runtime boundary |
|---|---|---|
| A | 54 | 62 |
| B | 32 | 32 |

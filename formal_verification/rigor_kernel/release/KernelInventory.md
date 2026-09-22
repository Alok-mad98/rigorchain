# Kernel Inventory

Map of the Coq consensus **kernel** cone as it exists now.

This file does **not** add a consensus proof and does **not**
strengthen the public claim.

**Public claim (unchanged):** Formally verified critical consensus
invariants inside a Coq consensus kernel model, with explicit
extraction and runtime trust boundaries.

`EXTRACTION_FORMAL_PROOF=source_identity_only`
`EXTRACTION_EQUIVALENCE_NOT_CLAIMED=TRUE`
`PRODUCTION_REFINEMENT_NOT_CLAIMED=TRUE`
`COMPILER_CORRECTNESS=false`
`BINARY_SEMANTIC_EQUIVALENCE=false`
`VERIFIED_COMPILER_FORCED_SWITCH=FALSE`

`KERNEL_INVENTORY=TRUE`

Status key: **PROVED** = Category-A Closed invariant covers the
*declared Coq-model* item. **PARTIAL** = a real fragment is Closed;
a named neighbouring piece is not. **OPEN** = not formalized in this
cone.

---

## 1. Specification Layer

### Supply model

| | |
|---|---|
| Status | **PROVED** (ORU subsidy + RES `W(h)` schedule and epoch-sum cap) |
| Coq file | `Supply/SupplySchedule.v`, `Supply/ORUModel.v`, `Supply/SupplyTransition.v` |
| Theorem | `res_schedule_finite` (A), `res_total_issuance_capped` (A), `epoch_supply_transition_sound` (A) |
| Evidence | M1, M12, M17; `release/TheoremClassification.md` |

`res_total_issuance_capped` is a schedule-side identity. It is **not**
a runtime `S_RES` reject (OP-D1 remains open by design).

### ORU accounting model

| | |
|---|---|
| Status | **PROVED** (first-class domain, peer of RES) |
| Coq file | `Economic/ORU/ORUSoundness.v`, `Supply/FeeDomain.v`, `proofs/ORU_Conservation/ORUConservation.v` |
| Theorem | `oru_domain_conservation` (A); `oru_conservation_extended` (A; FeeDomain), `no_hidden_inflation` (A) |
| Evidence | M3, M4, M26 |

**ORU Domain = Independent Verified Domain**

ORU conservation and RES issuance are separately modeled and verified.

### RES schedule model

| | |
|---|---|
| Status | **PROVED** (mint ≤ `W(h)` + per-mint genesis corollary + epoch-sum cap) |
| Coq file | `proofs/RES_Issuance/RESIssuance.v`, `Kernel/Chain/ReorgReplay.v`, `Kernel/RESValidator.v`, `Kernel/SafetyExtensions.v` |
| Theorem | `res_issuance_soundness` (A); `res_reorg_rewind_eq` (A); `res_reorg_surviving_pool` (A); `res_mint_bounded_by_genesis` (B) |
| Evidence | M2, M10, M99–M101; INVALID-003 |

Running `S_RES` ConnectBlock rejection is **OPEN** (OP-D1), not part
of this specification.

### UTXO model

| | |
|---|---|
| Status | **PROVED** (single-tx soundness; block-transition uniqueness is Category A) |
| Coq file | `proofs/UTXO_Soundness/UTXOSoundness.v`, `proofs/UTXO_Soundness/UTXOTransitionSoundness.v`, `Kernel/SafetyExtensions.v` |
| Theorem | `utxo_soundness` (A); `block_transition_utxo_soundness` (A); `apply_block_accepted_inputs_unique` (B) |
| Evidence | M9, M11, M13; INVALID-001 |

Most-work two-tip selection is proved (`more_work_wins`,
`chain_work_app`, `equal_work_keeps_active`). That is not a k-deep
bound. OP-R1 stays **OPEN**.

### Script classifier model

| | |
|---|---|
| Status | **PARTIAL** (classifier disjointness Closed; no VM / sighash / Ed25519) |
| Coq file | `Script/ScriptClassifier.v`, `proofs/Script_Correctness/ScriptCorrectness.v`, `Script/Semantics/ScriptTrace.v` |
| Theorem | `recognizer_pairwise_disjoint` (A); `recognizer_trace_disjoint` (A); `script_correctness` is Category B packaging |
| Evidence | M7, M14 |

### PoW model

| | |
|---|---|
| Status | **PARTIAL** (abstract digest `H`, target compare, compact lower bound) |
| Coq file | `Pow/AbstractHash.v`, `Pow/CompactBits.v`, `Pow/TargetRelation.v`, `proofs/Pow_Correctness/PowCorrectness.v` |
| Theorem | `pow_valid_rejects_above_target` (A), `compact_decode_lower_bound` (A), `target_monotonic` (A), `pow_acceptance_monotonic` (A) |
| Evidence | M6, M8, M15; `evidence/PowScope.md` |

Hash implementation is an assumption (`H`). keccak is not proved.

### ASERT model

| | |
|---|---|
| Status | **PARTIAL** (integer `exp_q` / clamp / factor range; not production nBits) |
| Coq file | `Pow/ASERTModel.v`, `Pow/ASERTBoundary.v`, `proofs/Pow_Correctness/ASERTCorrectness.v` |
| Theorem | `negative_time_error_preserved` (A), `exp_q_clamped` (A), `factor_range` (A), `time_error_direction_preserved` (A). `pow_asert_correctness` is Category C |
| Evidence | M5, M16; `evidence/ASERTScope.md` |

### Unified Kernel Transition

| | |
|---|---|
| Status | **PROVED** (model-level composition of the six domains) |
| Coq file | `Kernel/UnifiedState.v`, `Kernel/UnifiedTransition.v`, `Kernel/KernelInputBridge.v`, `Kernel/InvariantBundle.v`, `proofs/KernelTransitionSoundness.v`, `Economic/UTXOEconomicBridge.v` |
| Theorem | `kernel_step_preserves_invariants` (A), `kernel_transition_deterministic` (A), `transaction_value_flow_preserves_economic_state` (A), `input_ok_of_checks` (A), `kernel_apply_of_checks` (A), `input_ok_reflect` (A) |
| Evidence | M29, M30, M129, M130 |

Unified consensus kernel transition invariants are formally verified.

This is a Coq-model `kernel_step` over one `KernelState`. It is **not**
a C++ refinement, a full node, or binary semantic equivalence.
`KernelInputBridge.v` constructs a `KernelInput` from Path A checkers.
That construction is not a production-authority claim.

Path B `input_ok` is decidable: `input_ok s i ↔ input_ok_b s i = true`
(`input_ok_reflect`). The Bridge restates the one-way lemma as
`computed_ok_b s w = true → input_ok s (kernel_input_of w)`. The naive
biconditional `input_ok (kernel_input_of w) ↔ computed_ok` is **not**
proved (left-to-right is false). Free Path B `KernelInput` records are
banned in the gated cone (G-1 / `check_forbidden_tokens.py`); examples
go through `kernel_input_of`. Unique-entry (D-16 item 1) is not done.

ND-01 / ND-02 closed on 2026-09-18 (see `ENDGAME_DECISION_LOG` D-15/D-16).
Follow-up engineering has not started, except the Path A inductive
rename: `KernelValidationRequest` (formerly `KernelInput` on Path A),
per D-15, the `input_ok` decidability fragment, and the free-record
ban above. The Path B record `KernelInput` is unchanged.

### Chain-Level Kernel Run

| | |
|---|---|
| Status | **PROVED** (invariants and conservation over a run of arbitrary length) |
| Coq file | `Kernel/Chain/ChainTransition.v`, `Kernel/Chain/ChainRefinement.v` |
| Theorem | `kernel_chain_preserves_invariants` (A), `kernel_chain_states_invariant` (A), `reachable_preserves_invariants` (A), `kernel_chain_is_graph` (A), `chain_oru_conservation` (A), `chain_no_hidden_creation` (A), `chain_res_cap_fixed` (A), `chain_res_monotone` (A), `concrete_chain_commute` (A), `executable_chain_commute` (A), `executable_chain_preserves_invariants` (A), `machine_chain_refines_executable_chain` (A) |
| Evidence | M77–M89 |

Every result above `Kernel/Chain/` governs a single step. These govern
a run of arbitrary length, by induction over the run rather than by
iterating the single-step result at the call site.

Acceptability is judged at the running state, not at the state the run
started from, because `input_ok` reads the ORU stock and the RES cap
out of the state. `chain_ok_rejects_overspend` and
`chain_ok_accepts_funded_spend` pin that down: the identical block is
refused in one and accepted in the other, and the only difference is
the block before it.

`machine_chain_sound` is the end-to-end statement — a run the bounded
word machine completes decodes, projects onto the abstract run, and
satisfies the consensus invariants there. It does not claim the
machine completes every run; `machine_chain_refinement_is_partial`
exhibits an acceptable run it cannot finish, and
`machine_chain_completes_on_idle` shows the guard is nonetheless
satisfiable at every chain length.

This is a Coq-model chain over `KernelState`. It is **not**
k-deep finality, block relay, or mempool policy. RES rewind and
most-work selection live in `ReorgReplay.v` / `ForkChoice.v`; they
do not close OP-R1.

### Concrete kernel refinement (Coq → Coq)

| | |
|---|---|
| Status | **PROVED** (Coq abstract → Coq concrete) |
| Coq file | `Refinement/ConcreteKernel/` |
| Theorem | `concrete_step_refines_abstract_step` (A), `concrete_state_preserves_invariant` (A) |
| Evidence | M31, M32; `release/ConcreteRefinementBoundary.md` |

Coq abstract → Coq concrete refinement is Closed. Implementation
refinement remains OP-R2.

### Executable kernel boundary (Level 4.5)

| | |
|---|---|
| Status | **PROVED** (Coq concrete → executable model + Coq-model serialization) |
| Coq file | `ExecutionBoundary/`, `SerializationBoundary/KernelSerialization.v` |
| Theorem | `execution_projection_preserves_invariants` (A), `executable_step_refines_concrete_step` (A), `executable_transition_deterministic` (A), `decode_encode_identity` (A; kernel state) |
| Evidence | M33, M34, M35; `release/KernelExecutionBoundaryReport.md` |

Not C++, not a node, not compiler correctness.

### Extraction interface (Level 4.8)

| | |
|---|---|
| Status | **PROVED** (interface totality + operation trace + kernel integer bound) |
| Coq file | `ExtractionBoundary/`, `RuntimeModel/IntegerBoundary.v` |
| Theorem | `extraction_interface_bijective` (A), `operation_trace_preserved` (A), `integer_range_safe` (A), `golden_negative_vectors_rejected` (A) |
| Evidence | M36, M37; V1–V6; `release/ExtractionBoundaryHardeningReport.md` |

Coq executable model → extraction interface is Closed. Compiler
correctness remains OPEN.

### Runtime trust boundary (Level 4.9)

| | |
|---|---|
| Status | **PROVED** (Coq runtime abstraction + abstract checker contract) |
| Coq file | `Runtime/`, `CheckerContract/CheckerSpecification.v` |
| Theorem | `runtime_projection_preserves_kernel_domain` (A), `runtime_step_refines_executable_step` (A), `runtime_step_deterministic` (A), `invalid_kernel_input_rejected` (A), `runtime_failure_preserved` (A), `runtime_domain_closed` (A), `checker_accepts_valid_kernel_state` (A), `checker_rejects_invalid_kernel_state` (A) |
| Evidence | M38–M41; V7–V10; `release/RuntimeBoundaryClosureReport.md` |

Coq runtime semantic model is Closed. OCaml runtime, ABI, CPU, and
native binary remain OPEN.

---

## 2. Proof Layer Inventory

Every `.v` in `_CoqProject.kernel-critical`. Print Assumptions for
Category A/B is CI-driven and **Closed**. Unclassified theorems are
not in the public-claim set.

Claim level: **public** = may be cited in the public claim (A only);
**supporting** = do not cite as domain soundness; **boundary** =
documentation / non-claim; **none** = definitions only.

| File | Top theorem | Category | Print Assumptions | Mutation | Claim level |
|---|---|---|---|---|---|
| `Core/Bytes.v` | `le32_bytes_length` | unclassified | not in A/B driver | none | supporting |
| `Core/Amount.v` | `money_range_zero` | unclassified | not in A/B driver | none | supporting |
| `Core/Height.v` | `genesis_height_zero` | unclassified | not in A/B driver | none | supporting |
| `Script/ScriptTypes.v` | `script_type_eqb_refl` | unclassified | not in A/B driver | none | supporting |
| `Script/ScriptClassifier.v` | `classify_extensional` | unclassified | not in A/B driver | none | supporting |
| `Script/ScriptClassifier.v` | `classify_total` | A | Closed | M70 | core |
| `Script/ScriptClassifier.v` | `classify_range_exact` | A | Closed | M71 | core |
| `Script/ScriptClassifier.v` | `mint_not_converge` | A | Closed | M73 | core |
| `Script/ScriptClassifier.v` | `mint_not_null_data` | A | Closed | M74 | core |
| `Script/ScriptClassifier.v` | `converge_not_null_data` | A | Closed | M75 | core |
| `Script/ScriptClassifier.v` | `mint_script_of_is_mint` | A | Closed | M76 | core |
| `Script/Semantics/ScriptState.v` | *(definitions)* | — | — | none | none |
| `Script/Semantics/ScriptStep.v` | *(definitions)* | — | — | none | none |
| `Script/Semantics/ScriptTrace.v` | `recognizer_trace_disjoint` | A | Closed | M14 | public |
| `Supply/ORUModel.v` | *(definitions)* | — | — | none | none |
| `Supply/ORUConservation.v` | `oru_conservation` | unclassified | not in A/B driver | none | supporting |
| `Supply/SupplySchedule.v` | `res_schedule_finite` / `res_total_issuance_capped` | A | Closed | M1, M12 | public |
| `Supply/FeeDomain.v` | `oru_conservation_extended` | A | Closed | M3 (packaging file) | public |
| `Supply/SupplyTransition.v` | `epoch_supply_transition_sound` | A | Closed | M17 | public |
| `Pow/AbstractHash.v` | `hash_deterministic` | unclassified | not in A/B driver | M8 | supporting |
| `Pow/PowModel.v` | `pow_same_header_same_result` | unclassified | not in A/B driver | none | supporting |
| `Pow/Uint256Model.v` | `u256_add_comm` | B | Closed | none | supporting |
| `Pow/CompactBits.v` | `compact_decode_lower_bound` | A | Closed | M6 | public |
| `Pow/ASERTModel.v` | `negative_time_error_preserved` | A | Closed | M5 | public |
| `Pow/TargetRelation.v` | `target_monotonic` / `pow_acceptance_monotonic` | A | Closed | M15 | public |
| `Pow/ASERTBoundary.v` | `time_error_direction_preserved` | A | Closed | M16 | public |
| `proofs/UTXO_Soundness/UTXOSoundness.v` | `utxo_soundness` | A | Closed | M9 | public |
| `proofs/UTXO_Soundness/UTXOTransitionSoundness.v` | `block_transition_utxo_soundness` | A | Closed | M13 | public |
| `proofs/ORU_Conservation/ORUConservation.v` | `no_hidden_inflation` | A | Closed | M4 | public |
| `proofs/RES_Issuance/RESIssuance.v` | `res_issuance_soundness` / `res_reorg_replay_eq` | A | Closed | M2, M10, M99 | public |
| `Kernel/Chain/ReorgReplay.v` | `res_reorg_rewind_eq` / `oru_reorg_rewind_eq` | A | Closed | M100, M101, M105, M106 | public |
| `Supply/SupplySchedule.v` | `oru_subsidy_fold_closed` | A | Closed | M107 | public |
| `Pow/BlockProof.v` | `block_proof_eq_div` / compact-fail zeros / positive work | A | Closed | M108–M111 | public |
| `Kernel/Chain/ForkChoice.v` | `chain_work_app` / `more_work_wins` / `equal_work_keeps_active` / `chain_work_uses_block_proof` / `select_tip_*` | A | Closed | M102–M104, M112–M116 | public |
| `Kernel/Tx/Maturity.v` | `immature_coinbase_rejected` / `mature_coinbase_at_100` / `res_mint_uses_same_maturity` / `plain_transfer_no_maturity` / `input_over_max_rejected` / `pool_add_overflow_none` | A | Closed | M117–M122 | public |
| `Script/Semantics/Spend.v` | `converge_unspendable` / `nulldata_unspendable` / `nonstandard_unspendable` / `witness_requires_sig_ok` / `witness_sig_ok_spends` / `classifier_then_spend` | A | Closed | M123–M128 | public |
| `proofs/Script_Correctness/ScriptCorrectness.v` | `recognizer_pairwise_disjoint` | A | Closed | M7 | public |
| `proofs/Pow_Correctness/PowCorrectness.v` | `pow_valid_rejects_above_target` | A | Closed | M8 (via compare) | public |
| `proofs/Pow_Correctness/ASERTCorrectness.v` | `exp_q_clamped` / `factor_range` | A | Closed | none beyond M5 | public |
| `Kernel/UTXOValidator.v` | `check_utxo_valid` | unclassified | not in A/B driver | none | supporting |
| `Kernel/ORUValidator.v` | `check_oru_valid` | unclassified | not in A/B driver | none | supporting |
| `Kernel/RESValidator.v` | `check_res_sound` | unclassified | not in A/B driver | none | supporting |
| `Kernel/ScriptValidator.v` | `check_script_sound` | unclassified | not in A/B driver | none | supporting |
| `Kernel/PowValidator.v` | `check_pow_sound` | unclassified | not in A/B driver | none | supporting |
| `Kernel/ASERTValidator.v` | `check_asert_sound` | unclassified | not in A/B driver | none | supporting |
| `Kernel/KernelInterface.v` | *(definitions)* | — | — | none | none |
| `Kernel/KernelSoundness.v` | `kernel_sound` | A | Closed | none | public |

`Kernel/KernelSoundness.v` now names the Path A inductive
`KernelValidationRequest` (renamed 2026-09-18 per D-15; formerly
`KernelInput` on Path A). Constructors `KI_*` are unchanged. The Path B
record `UnifiedTransition.KernelInput` is unchanged.
| `Kernel/RefinementBoundary.v` | `kernel_boundary_closed` | C | not required | none | boundary |
| `Kernel/ExtractionBoundary.v` | `extraction_source_equivalence` | B | Closed | none | supporting |
| `Kernel/SerializationBoundary.v` | `encoding_deterministic` | B | Closed | none | supporting |
| `Audit/KernelNonClaims.v` | `we_do_not_claim_cpp_equivalence` | C | not required | none | boundary |
| `Kernel/CryptoAssumptions.v` | `crypto_keccak_not_verified` | C | not required | none | boundary |
| `Kernel/VerifiedCompilerBoundary.v` | `we_do_not_claim_verified_compiler_in_tcb` | C | not required | none | boundary |
| `Kernel/SafetyExtensions.v` | `apply_block_accepted_inputs_unique` | B | Closed | M11 | supporting |
| `Kernel/Transaction/Types.v` | *(definitions)* | — | — | none | none |
| `Kernel/Transaction/Validity.v` | `transaction_value_conservation` | A | Closed | M18 | public |
| `Kernel/EconomicInvariant/Economic.v` | `economic_no_hidden_creation` | A | Closed | M20 | public |
| `Kernel/Block/Types.v` | *(definitions)* | — | — | none | none |
| `Kernel/Block/Acceptance.v` | `block_acceptance_sound` | A | Closed | M19 | public |
| `Script/Semantics/Execution.v` | `script_step_deterministic` | A | Closed | M21 | public |
| `Pow/Acceptance.v` | `pow_pipeline_sound` | A | Closed | M22 | public |
| `Pow/ASERTMatrix.v` | `asert_boundary_complete` | A | Closed | M23 | public |
| `Audit/KernelAssumptions.v` | `abstract_digest_is_a_function` | unclassified | not in A/B driver | none | supporting |
| `Refinement/AbstractKernel.v` | *(definitions)* | — | — | none | none |
| `Refinement/ConcreteKernel.v` | *(definitions)* | — | — | none | none |
| `Refinement/RefinementRelation.v` | *(definitions)* | — | — | M24 | none |
| `Refinement/TransitionRefinement.v` | `refinement_preservation` | A | Closed | M24 | public |
| `Execution/KernelMachine.v` | `machine_step_deterministic` | A | Closed | M25 | public |
| `Economic/ORU/ORUDomain.v` | *(definitions)* | — | — | none | none |
| `Economic/ORU/ORUTransition.v` | *(definitions)* | — | — | M26, M27 | none |
| `Economic/ORU/ORUSoundness.v` | `oru_domain_conservation` | A | Closed | M26 | public |
| `Economic/DomainSeparation.v` | `res_creation_separate_from_oru_transfer` | A | Closed | M27 | public |
| `Refinement/EconomicState.v` | *(definitions)* | — | — | M28 | none |
| `Refinement/EconomicRefinement.v` | `economic_refinement_preserves_conservation` | A | Closed | M28 | public |
| `Refinement/KernelComposition.v` | `kernel_step_refinement_composes` | A | Closed | none | public |
| `Kernel/UnifiedState.v` | *(definitions)* | — | — | none | none |
| `Kernel/UnifiedTransition.v` | `input_ok_reflect` | A | Closed | M29, M30, M130 | public |
| `Kernel/KernelInputBridge.v` | `input_ok_of_checks` | A | Closed | M129 | public |
| `Kernel/InvariantBundle.v` | *(definitions)* | — | — | none | none |
| `proofs/KernelTransitionSoundness.v` | `kernel_step_preserves_invariants` | A | Closed | M29 | public |
| `Economic/UTXOEconomicBridge.v` | `transaction_value_flow_preserves_economic_state` | A | Closed | M30 | public |
| `Refinement/ConcreteKernel/ConcreteKernelState.v` | *(definitions)* | — | — | none | none |
| `Refinement/ConcreteKernel/ConcreteKernelMapping.v` | `concrete_state_preserves_invariant` | A | Closed | M31 | public |
| `Refinement/ConcreteKernel/ConcreteKernelTransition.v` | `concrete_step_refines_abstract_step` | A | Closed | M32 | public |
| `ExecutionBoundary/ConcreteExecutionState.v` | `execution_projection_preserves_invariants` | A | Closed | M33 | public |
| `ExecutionBoundary/ExecutableTransition.v` | `executable_step_refines_concrete_step` | A | Closed | M35 | public |
| `SerializationBoundary/KernelSerialization.v` | `decode_encode_identity` | A | Closed | M34 | public |
| `ExtractionBoundary/ExtractionInterface.v` | *(definitions)* | — | — | M37 | none |
| `ExtractionBoundary/ExtractionModel.v` | `extraction_interface_bijective` | A | Closed | M37 | public |
| `ExtractionBoundary/ExtractionTrace.v` | `operation_trace_preserved` | A | Closed | M37 | public |
| `ExtractionBoundary/GoldenNegatives.v` | `golden_negative_vectors_rejected` | A | Closed | M36 | public |
| `RuntimeModel/IntegerBoundary.v` | `integer_range_safe` | A | Closed | none | public |
| `Runtime/RuntimeState.v` | `runtime_projection_preserves_kernel_domain` | A | Closed | M38 | public |
| `Runtime/RuntimeTransition.v` | `runtime_step_refines_executable_step` | A | Closed | M40 | public |
| `Runtime/RuntimeSemantics.v` | *(definitions)* | — | — | none | none |
| `Runtime/RuntimeFailure.v` | `runtime_failure_preserved` | A | Closed | M41 | public |
| `CheckerContract/CheckerSpecification.v` | `checker_rejects_invalid_kernel_state` | A | Closed | M39 | public |

Canonical A/B/C names: `release/TheoremClassification.md`.
Unclassified `Kernel/` + `proofs/` names: `release/UnclassifiedAllowlist.md`.

---

## 3. Extraction Layer Inventory

```
Coq validators (check_*)
        ↓  extraction_source_equivalence  (definitional; Category B)
Extracted OCaml names
        ↓  untrusted ocamlopt
Native binary (test artifact)
```

| Step | Formally proved? |
|---|---|
| Coq `check_*` ⇔ kernel predicates | Yes (reflection lemmas; supporting) |
| Extracted names ⇔ Coq validators | Yes, **source identity only** |
| Coq extraction correctness (Gallina → OCaml semantics) | **No** |
| OCaml compiler correctness | **No** |
| Binary semantic equivalence | **No** |
| C++ / ConnectBlock refinement | **No** |

`EXTRACTION_FORMAL_PROOF=source_identity_only`
`COMPILER_CORRECTNESS=false`
`BINARY_SEMANTIC_EQUIVALENCE=false`

Faithful path: `extracted/Extract.v` (inductive `Z`). Legacy
`extraction/ExtractKernel.v` is **not** evidence (P0-113 F-01).

---

## Routes

| Kind | Items |
|---|---|
| Already proved (declared kernel scope) | Category A table; see §1–§2 |
| Not proved | OpenProperties.md (liveness, reorg, refinement, crypto, compiler) |
| TCB (trusted, not proved) | `release/TCBInventory.md` |
| High-cost future | CompCert/CertiCoq path; extraction refinement; production ConnectBlock; network/liveness cone |
| Not worth continuing *as this cone's next P0* | Full Bitcoin-node refinement; claiming theorem→binary; inflating Category A with packaging lemmas |

`P0-119_INVENTORY_COMPLETE=TRUE`

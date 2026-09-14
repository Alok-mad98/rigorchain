# Theorem Index

Kernel theorems for external audit. Every listed theorem compiles
with `Qed`. `Print Assumptions` reports **Closed under the global
context**.

Classification: `../evidence/TheoremClassification.md`.
Full statements: `../evidence/ProofInventory.md`.

**Public claim (only):** Formally verified critical consensus invariants
(Coq consensus kernel only).

Binary rows below are **test traces**, not refinement proofs.

---

## Runtime Trust Boundary (Level 4.9)

Coq runtime semantic model and abstract checker contract. Not the
OCaml runtime; not compiler correctness; not native binary.

---

Theorem:

runtime_projection_preserves_kernel_domain

Class: A

Location:

Runtime/RuntimeState.v

Statement:

If `runtime_invariants r`, then
`executable_invariants (runtime_projection r)`.

Assumptions:

Closed under global context

---

Theorem:

runtime_step_refines_executable_step

Class: A

Location:

Runtime/RuntimeTransition.v

Statement:

If `runtime_step r r'`, then
`executable_kernel_step (runtime_projection r) (runtime_projection r')`.

Assumptions:

Closed under global context

---

Theorem:

runtime_step_deterministic

Class: A

Location:

Runtime/RuntimeTransition.v

Statement:

If `runtime_apply r i r1` and `runtime_apply r i r2`, then `r1 = r2`.

Assumptions:

Closed under global context

---

Theorem:

invalid_kernel_input_rejected

Class: A

Location:

Runtime/RuntimeFailure.v

Statement:

If `InvalidKernelInput i`, then `~ input_ok s i`.

Assumptions:

Closed under global context

---

Theorem:

runtime_failure_preserved

Class: A

Location:

Runtime/RuntimeFailure.v

Statement:

If `InvalidRuntimeInput i`, then `runtime_rejects_invalid i = true`.

Assumptions:

Closed under global context

---

Theorem:

runtime_domain_closed

Class: A

Location:

RuntimeModel/IntegerBoundary.v

Statement:

Kernel-domain money, height, target, and time-error ranges stay
inside the documented signed bound. Not all integers.

Assumptions:

Closed under global context

---

Theorem:

checker_accepts_valid_kernel_state

Class: A

Location:

CheckerContract/CheckerSpecification.v

Statement:

`checker_accept_contract true` (abstract interface).

Assumptions:

Closed under global context

---

Theorem:

checker_rejects_invalid_kernel_state

Class: A

Location:

CheckerContract/CheckerSpecification.v

Statement:

`checker_reject_contract false` (abstract interface).

Assumptions:

Closed under global context

---

## Extraction Boundary (Level 4.8)

Coq executable model → extracted interface. Not compiler
correctness; not native binary equivalence.

---

Theorem:

extraction_interface_bijective

Class: A

Location:

ExtractionBoundary/ExtractionModel.v

Statement:

Every `CoqKernelFunction` has an `ExtractedKernelInterface` entry
and `boundary_aligned`.

Assumptions:

Closed under global context

---

Theorem:

operation_trace_preserved

Class: A

Location:

ExtractionBoundary/ExtractionTrace.v

Statement:

`extract_trace ops = expected_trace ops` (operation sequence
preserved at the extraction boundary).

Assumptions:

Closed under global context

---

Theorem:

integer_range_safe

Class: A

Location:

RuntimeModel/IntegerBoundary.v

Statement:

If `money_range z` then `0 <= z <= 2^62 - 1`. Kernel-domain bound
only.

Assumptions:

Closed under global context

---

## Execution Boundary (Level 4.5)

Executable kernel model and Coq-model serialization. Not C++, not a
node, not compiler correctness.

---

Theorem:

execution_projection_preserves_invariants

Class: A

Location:

ExecutionBoundary/ConcreteExecutionState.v

Statement:

If `executable_invariants e`, then
`concrete_invariants (execution_projection e)`.

Assumptions:

Closed under global context

---

Theorem:

executable_step_refines_concrete_step

Class: A

Location:

ExecutionBoundary/ExecutableTransition.v

Statement:

If `executable_kernel_step s s'`, then
`concrete_kernel_step (execution_projection s) (execution_projection s')`.

Assumptions:

Closed under global context

---

Theorem:

executable_transition_deterministic

Class: A

Location:

ExecutionBoundary/ExecutableTransition.v

Statement:

If `executable_kernel_apply s i s1` and `executable_kernel_apply s i s2`,
then `s1 = s2`.

Assumptions:

Closed under global context

---

Theorem:

decode_encode_identity

Class: A

Location:

SerializationBoundary/KernelSerialization.v

Statement:

`decode_kernel_state (encode_kernel_state e) = Some e`.
Coq-model serialization only.

Assumptions:

Closed under global context

---

## Concrete Refinement (Coq → Coq)

Coq abstract → Coq concrete refinement. Not C++ implementation
refinement; not a node; not compiler correctness.

---

Theorem:

concrete_step_refines_abstract_step

Class: A

Location:

Refinement/ConcreteKernel/ConcreteKernelTransition.v

Statement:

If `concrete_kernel_step c c'`, then
`kernel_step (kernel_projection c) (kernel_projection c')`.

Assumptions:

Closed under global context

---

Theorem:

concrete_state_preserves_invariant

Class: A

Location:

Refinement/ConcreteKernel/ConcreteKernelMapping.v

Statement:

If `kernel_invariants a` and `kernel_projection c = a`, then
`concrete_invariants c`.

Assumptions:

Closed under global context

---

## Chain-Level Kernel Run

Kernel invariants and value conservation are formally verified over a
run of arbitrary length, not only over one step. Coq model only. Not
reorganisation, fork choice, or block relay.

---

Theorem:

kernel_chain_preserves_invariants

Class: A

Location:

Kernel/Chain/ChainTransition.v

Statement:

If `kernel_invariants s` and `chain_ok s is`, then
`kernel_invariants (chain_step_of s is)`, for a run `is` of any length.

Assumptions:

Closed under global context

---

Theorem:

kernel_chain_states_invariant

Class: A

Location:

Kernel/Chain/ChainTransition.v

Statement:

Under the same hypotheses, every state the run passes through --- not
only the endpoint --- satisfies `kernel_invariants`.

Assumptions:

Closed under global context

---

Theorem:

reachable_preserves_invariants

Class: A

Location:

Kernel/Chain/ChainTransition.v

Statement:

If `kernel_invariants s` and `s'` is reachable from `s` by some
acceptable run, then `kernel_invariants s'`.

Assumptions:

Closed under global context

---

Theorem:

kernel_chain_is_graph

Class: A

Location:

Kernel/Chain/ChainTransition.v

Statement:

`kernel_chain s is s'` holds exactly when `chain_ok s is` and
`s' = chain_step_of s is`. Acceptability is evaluated at the running
state, so each block is judged against the state its predecessors
produced.

Assumptions:

Closed under global context

---

Theorem:

chain_oru_conservation

Class: A

Location:

Kernel/Chain/ChainTransition.v

Statement:

`oru_stock` at the end of a run equals `oru_stock` at the start plus
the summed per-block flow.

Assumptions:

Closed under global context

---

Theorem:

chain_no_hidden_creation

Class: A

Location:

Kernel/Chain/ChainTransition.v

Statement:

The net ORU change over a run equals subsidy plus transfers in, minus
transfers out and burn, and nothing else.

Assumptions:

Closed under global context

---

Theorem:

chain_res_cap_fixed

Class: A

Location:

Kernel/Chain/ChainTransition.v

Statement:

`res_cap` at the end of a run equals `res_cap` at the start: a chain
cannot raise the ceiling it is measured against.

Assumptions:

Closed under global context

---

Theorem:

chain_res_monotone

Class: A

Location:

Kernel/Chain/ChainTransition.v

Statement:

`res_stock` never decreases along an acceptable run.

Assumptions:

Closed under global context

---

Theorem:

concrete_chain_commute

Class: A

Location:

Kernel/Chain/ChainRefinement.v

Statement:

`kernel_projection` of the concrete layer's run equals the abstract
run of the projection.

Assumptions:

Closed under global context

---

Theorem:

executable_chain_commute

Class: A

Location:

Kernel/Chain/ChainRefinement.v

Statement:

`execution_projection` of the executable layer's run equals the
concrete run of the projection.

Assumptions:

Closed under global context

---

Theorem:

executable_chain_preserves_invariants

Class: A

Location:

Kernel/Chain/ChainRefinement.v

Statement:

`executable_invariants` survive a whole run whose projection is
acceptable at every step.

Assumptions:

Closed under global context

---

Theorem:

machine_chain_refines_executable_chain

Class: A

Location:

Kernel/Chain/ChainRefinement.v

Statement:

If the start state decodes and the machine completes the run, the
result decodes to the run the model computes. The machine is not
claimed to complete every run.

Assumptions:

Closed under global context

---

## Unified Kernel Transition

Unified consensus kernel transition invariants are formally verified
inside the Coq model. Not a node; not a C++ refinement.

---

Theorem:

kernel_step_preserves_invariants

Class: A

Location:

proofs/KernelTransitionSoundness.v

Statement:

If `kernel_invariants old_state` and `kernel_step old_state new_state`,
then `kernel_invariants new_state`.

Assumptions:

Closed under global context

---

Theorem:

kernel_transition_deterministic

Class: A

Location:

proofs/KernelTransitionSoundness.v

Statement:

If `kernel_apply s i s1` and `kernel_apply s i s2`, then `s1 = s2`.

Assumptions:

Closed under global context

---

Theorem:

transaction_value_flow_preserves_economic_state

Class: A

Location:

Economic/UTXOEconomicBridge.v

Statement:

A kernel apply copies fees / burn / subsidy and updates ORU stock by
subsidy + incoming − outgoing − burn. No value is created except
subsidy (plus incoming transfer).

Assumptions:

Closed under global context

---

## Class A — core verified invariants

---

Theorem:

utxo_soundness

Class: A

Location:

proofs/UTXO_Soundness/

Statement:

If `valid_tx tx` and `utxo_wf u`, then there exists `u'` such that
`apply_tx u tx = Some u'`, `utxo_wf u'`, `inputs_consumed tx u u'`,
and `outputs_created tx u u'`.

Assumptions:

Closed under global context

Test artifact (not a proof):

`kernel_binary/test_vectors/utxo_valid.json` / `utxo_invalid.json`

---

Theorem:

no_hidden_inflation

Class: A

Location:

proofs/ORU_Conservation/

Statement:

If `valid_tx tx` then `oru_input tx = oru_output tx + fee tx`.

Assumptions:

Closed under global context

---

Theorem:

oru_conservation_extended

Class: A

Location:

proofs/ORU_Conservation/

Statement:

If `valid_tx tx` then
`new_supply tx = old_supply tx + subsidy tx + fees tx - burn tx`.

Assumptions:

Closed under global context

---

Theorem:

res_issuance_soundness

Class: A

Location:

proofs/RES_Issuance/

Statement:

If `res_created tx`, `draw_eligible tx`, and `h = rt_height tx`,
then the tx is coinbase, `valid_res_mint`, `mint_amount tx <= W h`,
and `draw_eligible tx`. Draw eligibility remains a hypothesis (C-4).

Assumptions:

Closed under global context

---

Theorem:

recognizer_pairwise_disjoint

Class: A

Location:

proofs/Script_Correctness/

Statement:

Recognizer branches are exclusive: a witness program never classifies
as mint/converge/null-data, and later recognizers fire only after
earlier ones fail.

Assumptions:

Closed under global context

---

Theorem:

pow_valid_rejects_above_target

Class: A

Location:

proofs/Pow_Correctness/

Statement:

If `digest_as_Z (pow_hash h) > target` then `~ pow_valid h target`.
Digest `H` is uninterpreted (C-1).

Assumptions:

Closed under global context

---

Theorem:

pow_asert_correctness

Class: A

Location:

proofs/Pow_Correctness/

Statement:

For all `anchor parent`, `deterministic_asert anchor parent`
(`anchor = parent → asert_next_target anchor = asert_next_target parent`).

Assumptions:

Closed under global context

---

Theorem:

kernel_sound

Class: A

Location:

Kernel/KernelSoundness.v

Statement:

`validate_kernel input = true → consensus_invariant input`.

Assumptions:

Closed under global context

---

## Class B — supporting lemmas (kept, lowered public weight)

---

Theorem:

pow_valid_deterministic

Class: B (supporting lemma)

Location:

proofs/Pow_Correctness/

Statement:

For every header `h`, `pow_valid h 0 = pow_valid h 0` (function-ness).

Honest description:

Abstract PoW relation only. **NOT** a digest implementation proof.
`H` is uninterpreted. The class-A PoW content is
`pow_valid_rejects_above_target`.

Assumptions:

Closed under global context

---

Theorem:

script_correctness

Class: B (supporting lemma)

Location:

proofs/Script_Correctness/

Statement:

The asset range is exactly `[A_ORU; A_RES; A_NONE; A_INVALID]`, every
one of the four is produced by some script, no 20-byte payload moves a
script between classes, and at most one shape recognizer fires on any
script.

Honest description:

A bundle of four results that each carry content. Until frontier 1.4
this said `classify s = a` for `a := classify s`, plus two further
self-equalities, and held of any function whatsoever. The class-A
content is in `classify_total`, `classify_range_exact`,
`classify_type_total` and the recognizer-disjointness results.

Assumptions:

Closed under global context

---

## Class C — boundary / documentation lemmas

---

Theorem:

kernel_boundary_closed

Class: C (documentation lemma)

Location:

Kernel/RefinementBoundary.v

Statement:

`refinement_scope_closed` (the documentation record field is `True`).

Honest description:

Boundary marker only. It does not prove extraction, compiler, or
binary correctness. The proof stop is Coq validators.

Assumptions:

Closed under global context

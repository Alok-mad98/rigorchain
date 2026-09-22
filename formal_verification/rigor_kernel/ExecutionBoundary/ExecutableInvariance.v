(** What the executable kernel step actually computes, and the fact
    that it preserves the kernel invariant.

    The execution boundary previously exported only
    [execution_projection_preserves_invariants], whose proof is
    [exact Hinv] because [ExecutableKernelState] is a field-for-field
    clone of [ConcreteKernelState]. A projection between clone records
    is not an invariant result.

    The property this layer owes is invariant PRESERVATION across a
    step: starting from a state satisfying the kernel invariant and
    applying an accepted input yields a state that still satisfies it.
    That is proved here, and it is not a projection — the RES clamp and
    the ORU flow condition both have to be discharged.

    NOT CLAIMED: C++ ConnectBlock, node, compiler correctness.
*)
From Coq Require Import Bool.Bool.
From Coq Require Import ZArith.ZArith.
From Coq Require Import Lia.
From RigorKernel Require Import Kernel.UnifiedState.
From RigorKernel Require Import Kernel.UnifiedTransition.
From RigorKernel Require Import Kernel.KernelInputBridge.
From RigorKernel Require Import Supply.SupplySchedule.
From RigorKernel Require Import Refinement.ConcreteKernel.ConcreteKernelState.
From RigorKernel Require Import Refinement.ConcreteKernel.ConcreteKernelMapping.
From RigorKernel Require Import Refinement.ConcreteKernel.ConcreteKernelTransition.
From RigorKernel Require Import ExecutionBoundary.ConcreteExecutionState.
From RigorKernel Require Import ExecutionBoundary.ExecutableTransition.

Local Open Scope Z_scope.

(** * The step, computed out

    The model layers are stacked definitionally, so nothing in the
    development previously stated what [executable_step_of] computes.
    Every result below, and the independent machine step in
    [Runtime.MachineState], depends on this. *)

Lemma executable_step_fields :
  forall (e : ExecutableKernelState) (i : KernelInput),
    executable_step_of e i =
    {| ek_utxo_flag := andb (ek_utxo_flag e) (ki_utxo_ok i);
       ek_fees := ki_fees i;
       ek_burn := ki_burn i;
       ek_subsidy := ki_subsidy i;
       ek_res_stock :=
         Z.min (ek_res_cap e) (ek_res_stock e + Z.max 0 (ki_res_mint i));
       ek_res_cap := ek_res_cap e;
       ek_oru_stock := ek_oru_stock e + oru_delta i;
       ek_script_ok := andb (ek_script_ok e) (ki_script_ok i);
       ek_pow_ok := andb (ek_pow_ok e) (ki_pow_ok i);
       ek_asert_ok := andb (ek_asert_ok e) (ki_asert_ok i) |}.
Proof.
  intros e i.
  unfold executable_step_of, embed_concrete, concrete_step_of,
         execution_projection, exec_project_utxo, kernel_projection,
         project_utxo_flag, kernel_step_of, utxo_transition,
         econ_transition, res_transition, oru_transition,
         script_transition, pow_transition, asert_transition.
  destruct e. simpl. reflexivity.
Qed.

(** * Invariant preservation

    The ORU conjunct is discharged from the flow condition inside
    [input_ok] ([outgoing + burn <= stock + subsidy + incoming]); the
    RES conjunct from the clamp. Neither is available by destructuring
    the hypothesis. *)

Theorem executable_step_preserves_invariants :
  forall (e : ExecutableKernelState) (i : KernelInput),
    executable_invariants e ->
    input_ok (kernel_projection (execution_projection e)) i ->
    executable_invariants (executable_step_of e i).
Proof.
  intros e i Hinv Hok.
  destruct Hinv as [Hu [Horu [[Hr0 Hrc] [Hs [Hp Ha]]]]].
  unfold input_ok, kernel_projection, execution_projection,
         exec_project_utxo in Hok. simpl in Hok.
  destruct Hok as [Hiu [His [Hip [Hia [Hin [Hout [_ [Hburn
                  [Hsub [Hmint [Hflow [Hcap0 _]]]]]]]]]]]].
  rewrite executable_step_fields.
  unfold executable_invariants. simpl.
  repeat split.
  - rewrite Hu, Hiu. reflexivity.
  - unfold oru_delta. lia.
  - apply Z.min_glb; lia.
  - apply Z.le_min_l.
  - rewrite Hs, His. reflexivity.
  - rewrite Hp, Hip. reflexivity.
  - rewrite Ha, Hia. reflexivity.
Qed.

(** * The preservation theorem is not vacuous

    An accepted input exists, so the hypothesis set is inhabited. If it
    were not, the theorem above would hold for any postcondition. *)

Definition sample_state : ExecutableKernelState :=
  {| ek_utxo_flag := true;
     ek_fees := 0;
     ek_burn := 0;
     ek_subsidy := 0;
     ek_res_stock := res_stock (ks_res sample_kstate);
     ek_res_cap := res_cap (ks_res sample_kstate);
     ek_oru_stock := oru_stock (ks_oru sample_kstate);
     ek_script_ok := true;
     ek_pow_ok := true;
     ek_asert_ok := true |}.

Definition sample_input : KernelInput := kernel_input_of sample_witness.

Theorem sample_state_satisfies_invariants :
  executable_invariants sample_state.
Proof.
  unfold executable_invariants, sample_state, sample_kstate. simpl.
  repeat split; try reflexivity;
    unfold RES_MAX_SUPPLY; lia.
Qed.

Theorem sample_input_is_accepted :
  input_ok (kernel_projection (execution_projection sample_state)) sample_input.
Proof.
  replace (kernel_projection (execution_projection sample_state))
    with sample_kstate
    by (unfold sample_state, sample_kstate, kernel_projection,
               execution_projection, exec_project_utxo, project_utxo_flag;
        reflexivity).
  unfold sample_input. exact bridge_is_inhabited.
Qed.

Theorem executable_preservation_hypotheses_are_inhabited :
  exists (e : ExecutableKernelState) (i : KernelInput),
    executable_invariants e /\
    input_ok (kernel_projection (execution_projection e)) i /\
    executable_invariants (executable_step_of e i).
Proof.
  exists sample_state, sample_input.
  split; [exact sample_state_satisfies_invariants |].
  split; [exact sample_input_is_accepted |].
  apply executable_step_preserves_invariants;
    [exact sample_state_satisfies_invariants | exact sample_input_is_accepted].
Qed.

(** The flow condition is load-bearing: without it the ORU stock can
    go negative and the invariant is not preserved. *)
Theorem oru_flow_condition_is_necessary :
  exists (e : ExecutableKernelState) (i : KernelInput),
    executable_invariants e /\
    ek_oru_stock e + oru_delta i < 0.
Proof.
  exists sample_state, (kernel_input_of drain_witness).
  split; [exact sample_state_satisfies_invariants |].
  unfold sample_state, sample_kstate.
  rewrite drain_witness_delta. simpl. lia.
Qed.

(** RigorKernel.Kernel.Chain.ChainRefinement — the stack, over runs.

    Scope: consensus kernel only.

    `Kernel/Chain/ChainTransition.v` lifts the abstract kernel to
    multi-block runs. This lifts the refinement stack to match, so the
    guarantee is not "each layer takes one correct step" but "each
    layer runs a whole chain and lands where the abstract chain says
    it should".

    The machine layer is the one that makes this worth proving. Its
    step is written independently over the bounded word domain and can
    fail where the unbounded model succeeds, so a chain on the machine
    is a sequence of things that can each go wrong. The obligation
    below is a simulation with partiality carried through: IF the
    whole run completes on the machine, THEN it decodes to the run the
    model would have produced, and the consensus invariants hold at
    the end. Both halves of "if" are shown to be reachable: idle runs
    of any length complete, and there is an acceptable run the machine
    cannot finish.

    NOT CLAIMED: reorganisation, fork choice, OCaml runtime, ABI,
    native binary, CPU semantics, C++ ConnectBlock.
*)
From Coq Require Import Bool.Bool.
From Coq Require Import Lists.List.
From Coq Require Import ZArith.ZArith.
From Coq Require Import Lia.
From RigorKernel Require Import Kernel.UnifiedState.
From RigorKernel Require Import Kernel.UnifiedTransition.
From RigorKernel Require Import Kernel.InvariantBundle.
From RigorKernel Require Import Kernel.Chain.ChainTransition.
From RigorKernel Require Import Refinement.ConcreteKernel.ConcreteKernelState.
From RigorKernel Require Import Refinement.ConcreteKernel.ConcreteKernelMapping.
From RigorKernel Require Import Refinement.ConcreteKernel.ConcreteKernelTransition.
From RigorKernel Require Import ExecutionBoundary.ConcreteExecutionState.
From RigorKernel Require Import ExecutionBoundary.ExecutableTransition.
From RigorKernel Require Import ExecutionBoundary.ExecutableInvariance.
From RigorKernel Require Import Runtime.MachineWord.
From RigorKernel Require Import Runtime.MachineState.

Import ListNotations.
Local Open Scope Z_scope.

(** * Concrete layer *)

Fixpoint concrete_chain_step_of
    (c : ConcreteKernelState) (is : list KernelInput)
  : ConcreteKernelState :=
  match is with
  | [] => c
  | i :: rest => concrete_chain_step_of (concrete_step_of c i) rest
  end.

(** The single-step commutation, iterated. Class: B (supporting). *)
Theorem concrete_chain_commute :
  forall (is : list KernelInput) (c : ConcreteKernelState),
    kernel_projection (concrete_chain_step_of c is)
      = chain_step_of (kernel_projection c) is.
Proof.
  induction is as [| i rest IH]; intros c; simpl; [reflexivity |].
  rewrite IH, projection_step_commute. reflexivity.
Qed.

(** * Executable layer *)

Fixpoint executable_chain_step_of
    (e : ExecutableKernelState) (is : list KernelInput)
  : ExecutableKernelState :=
  match is with
  | [] => e
  | i :: rest => executable_chain_step_of (executable_step_of e i) rest
  end.

(** One executable step, seen all the way down at the abstract layer.
    Both projections are applied, so this is the composite the chain
    induction actually needs. *)
Lemma executable_step_projects :
  forall (e : ExecutableKernelState) (i : KernelInput),
    kernel_projection (execution_projection (executable_step_of e i))
      = kernel_step_of (kernel_projection (execution_projection e)) i.
Proof.
  intros e i.
  rewrite execution_step_commute. apply projection_step_commute.
Qed.

Theorem executable_chain_commute :
  forall (is : list KernelInput) (e : ExecutableKernelState),
    execution_projection (executable_chain_step_of e is)
      = concrete_chain_step_of (execution_projection e) is.
Proof.
  induction is as [| i rest IH]; intros e; simpl; [reflexivity |].
  rewrite IH, execution_step_commute. reflexivity.
Qed.

(** The executable layer runs the abstract chain. This is the
    statement the layers above are ultimately reduced to.
    Class: A (core verified consensus invariant).
    Assumptions: none.
    Coq consensus kernel only: yes. *)
Theorem executable_chain_refines_kernel_chain :
  forall (is : list KernelInput) (e : ExecutableKernelState),
    kernel_projection (execution_projection (executable_chain_step_of e is))
      = chain_step_of (kernel_projection (execution_projection e)) is.
Proof.
  intros is e. rewrite executable_chain_commute, concrete_chain_commute.
  reflexivity.
Qed.

(** Invariant preservation on the executable layer over a whole run.
    The acceptability hypothesis is the abstract one, transported
    through the projection at each step — which is exactly why
    [chain_ok] had to be defined at the running state.
    Class: A (core verified consensus invariant).
    Assumptions: executable invariants at the start, and the projected
    run is acceptable step by step.
    Coq consensus kernel only: yes. *)
Theorem executable_chain_preserves_invariants :
  forall (is : list KernelInput) (e : ExecutableKernelState),
    executable_invariants e ->
    chain_ok (kernel_projection (execution_projection e)) is ->
    executable_invariants (executable_chain_step_of e is).
Proof.
  induction is as [| i rest IH]; intros e Hinv Hok; simpl in *;
    [exact Hinv |].
  destruct Hok as [Hstep Hrest].
  apply IH.
  - apply executable_step_preserves_invariants; assumption.
  - rewrite executable_step_projects. exact Hrest.
Qed.

(** * Machine layer

    A run on the machine is a fold that can stop. [None] is not
    failure of the consensus rules; it is the word domain refusing to
    represent a state the unbounded model is happy with. *)

Fixpoint machine_chain_step_of
    (m : MachineKernelState) (is : list KernelInput)
  : option MachineKernelState :=
  match is with
  | [] => Some m
  | i :: rest =>
      match machine_step_of m i with
      | None => None
      | Some m' => machine_chain_step_of m' rest
      end
  end.

(** The chain-level simulation obligation. Note what it does NOT say:
    it does not say the machine completes the run. It says that a run
    the machine completes is the run the model describes.
    Class: A (core verified consensus invariant).
    Assumptions: the start state decodes and the whole run completes.
    Coq consensus kernel only: yes. *)
Theorem machine_chain_refines_executable_chain :
  forall (is : list KernelInput) (m : MachineKernelState)
         (e : ExecutableKernelState) (m' : MachineKernelState),
    machine_decode m = Some e ->
    machine_chain_step_of m is = Some m' ->
    machine_decode m' = Some (executable_chain_step_of e is).
Proof.
  induction is as [| i rest IH]; intros m e m' Hdec Hrun; simpl in *.
  - injection Hrun as <-. exact Hdec.
  - destruct (machine_step_of m i) as [m1 |] eqn:Hstep; [| discriminate].
    apply (IH m1 (executable_step_of e i) m'); [| exact Hrun].
    apply machine_step_refines_executable_step with (m := m); assumption.
Qed.

(** * End to end

    Machine run, completed, decoded, projected all the way down to the
    abstract kernel, with the consensus invariants holding there. This
    is the sentence the package could not previously say: not "one
    step of the checked implementation is sound" but "a chain of them
    is, and it computes the chain the model specifies".
    Class: A (core verified consensus invariant).
    Assumptions: the start state decodes, its projection satisfies the
    invariants, the projected run is acceptable at every step, and the
    machine completes the run.
    Coq consensus kernel only: yes. *)
Theorem machine_chain_sound :
  forall (is : list KernelInput) (m : MachineKernelState)
         (e : ExecutableKernelState) (m' : MachineKernelState),
    machine_decode m = Some e ->
    executable_invariants e ->
    kernel_invariants (kernel_projection (execution_projection e)) ->
    chain_ok (kernel_projection (execution_projection e)) is ->
    machine_chain_step_of m is = Some m' ->
    exists e' : ExecutableKernelState,
      machine_decode m' = Some e' /\
      executable_invariants e' /\
      kernel_projection (execution_projection e')
        = chain_step_of (kernel_projection (execution_projection e)) is /\
      kernel_invariants (kernel_projection (execution_projection e')).
Proof.
  intros is m e m' Hdec Hexec Hinv Hok Hrun.
  exists (executable_chain_step_of e is).
  split; [eapply machine_chain_refines_executable_chain; eassumption |].
  split; [apply executable_chain_preserves_invariants; assumption |].
  split; [apply executable_chain_refines_kernel_chain |].
  rewrite executable_chain_refines_kernel_chain.
  apply kernel_chain_preserves_invariants; assumption.
Qed.

(** * The chain obligation is not vacuous

    [machine_chain_sound] is guarded by "the machine completes the
    run". If nothing completed, the theorem would be empty, and if
    everything completed the word bound would be decoration. Neither
    is the case. *)

(** An idle block costs the machine nothing it cannot represent, so a
    decodable state survives one. *)
Lemma machine_step_idle_succeeds :
  forall m : MachineKernelState,
    machine_words_wf m = true ->
    exists m' : MachineKernelState, machine_step_of m idle_input = Some m'.
Proof.
  intros m Hwf.
  apply machine_words_wf_inv in Hwf
    as [_ [_ [_ [Hstock [_ Horu]]]]].
  unfold machine_step_of, idle_input.
  cbn [ki_fees ki_burn ki_subsidy ki_res_mint ki_utxo_ok ki_script_ok
       ki_pow_ok ki_asert_ok ki_incoming ki_outgoing].
  replace (wf_wordb 0) with true
    by (symmetry; apply wf_wordb_true_iff; unfold wf_word; pose word_max_pos; lia).
  cbn [andb negb].
  replace (Z.max 0 0) with 0 by lia.
  rewrite (w_add_of_wf (mk_res_stock m) 0)
    by (unfold wf_word in *; lia).
  replace (oru_delta {| ki_utxo_ok := true; ki_incoming := 0;
                        ki_outgoing := 0; ki_fees := 0; ki_burn := 0;
                        ki_subsidy := 0; ki_res_mint := 0;
                        ki_script_ok := true; ki_pow_ok := true;
                        ki_asert_ok := true |}) with 0
    by (unfold oru_delta; cbn; lia).
  rewrite (w_add_of_wf (mk_oru_stock m) 0)
    by (unfold wf_word in *; lia).
  eexists. reflexivity.
Qed.

(** Runs of any length complete on the machine, so the guard on
    [machine_chain_sound] is satisfiable for every chain length rather
    than only for the empty chain. *)
Theorem machine_chain_completes_on_idle :
  forall (n : nat) (m : MachineKernelState) (e : ExecutableKernelState),
    machine_decode m = Some e ->
    exists m' : MachineKernelState,
      machine_chain_step_of m (repeat idle_input n) = Some m'.
Proof.
  induction n as [| n IH]; intros m e Hdec; simpl.
  - exists m. reflexivity.
  - pose proof Hdec as Hdec'.
    apply machine_decode_some_inv in Hdec' as [Hwf _].
    destruct (machine_step_idle_succeeds m Hwf) as [m1 Hstep].
    rewrite Hstep.
    apply (IH m1 (executable_step_of e idle_input)).
    apply machine_step_refines_executable_step with (m := m); assumption.
Qed.

(** And the bound bites: here is a run the kernel accepts at every
    step and the machine cannot finish. The failure is representation,
    not a rule violation, which is precisely the gap between the model
    and anything that has to run on hardware. *)
Theorem machine_chain_refinement_is_partial :
  exists (m : MachineKernelState) (e : ExecutableKernelState)
         (is : list KernelInput),
    machine_decode m = Some e /\
    chain_ok (kernel_projection (execution_projection e)) is /\
    machine_chain_step_of m is = None.
Proof.
  exists overflow_witness_state,
         (machine_to_executable overflow_witness_state),
         [overflow_witness_input].
  split.
  - unfold machine_decode.
    rewrite machine_words_wf_intro; simpl; try reflexivity;
      unfold wf_word, word_max; lia.
  - split.
    + simpl. split; [| exact I].
      unfold input_ok, overflow_witness_input, overflow_witness_state,
             machine_to_executable, execution_projection, kernel_projection,
             exec_project_utxo.
      cbn.
      repeat split; try reflexivity; unfold word_max; lia.
    + simpl.
      unfold machine_step_of, overflow_witness_state, overflow_witness_input.
      cbn [ki_fees ki_burn ki_subsidy ki_res_mint mk_res_stock mk_oru_stock].
      replace (wf_wordb 0) with true
        by (symmetry; apply wf_wordb_true_iff; unfold wf_word, word_max; lia).
      cbn [andb negb].
      replace (Z.max 0 1) with 1 by lia.
      unfold w_add.
      destruct (wf_wordb (word_max + 1)) eqn:E.
      * apply wf_wordb_true_iff in E. unfold wf_word in E. lia.
      * reflexivity.
Qed.

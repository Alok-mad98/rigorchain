(** Abstract checker contract. Not a binary proof.

    Previously [abstract_checker] took a validity *bit* as an argument
    and returned it, i.e. it was the identity on [bool] and its
    "contract" theorems were [b = b -> b = b]. It could not be wrong,
    and it said nothing about any checker.

    Here the checker DECIDES. It takes a runtime machine state, tries
    to decode it into the executable kernel model, and runs the
    executable decision procedure. The contract is therefore a real
    reflection result — soundness and completeness against
    [executable_invariants] — plus a witness that the checker is not a
    constant function.

    [abstract_checker] is still a Coq interface. It is not
    [rigor-kernel-checker] and not ConnectBlock.

    NOT CLAIMED: native binary equivalence, compiler, C++.
*)
From Coq Require Import Bool.Bool.
From Coq Require Import ZArith.ZArith.
From Coq Require Import Lia.
From RigorKernel Require Import ExecutionBoundary.ConcreteExecutionState.
From RigorKernel Require Import ExtractionBoundary.ExtractionInterface.
From RigorKernel Require Import Runtime.MachineWord.
From RigorKernel Require Import Runtime.MachineState.

Local Open Scope Z_scope.

(** * The executable decision procedure reflects the model invariant

    This is the substance of the contract: a boolean check agrees with
    a propositional invariant in both directions. *)

Theorem check_executable_kernel_reflects :
  forall e : ExecutableKernelState,
    check_executable_kernel e = true <-> executable_invariants e.
Proof.
  intros e. unfold check_executable_kernel, executable_invariants.
  split.
  - intros H.
    apply andb_true_iff in H as [Hutxo H].
    apply andb_true_iff in H as [Horu H].
    apply andb_true_iff in H as [Hres0 H].
    apply andb_true_iff in H as [Hrescap H].
    apply andb_true_iff in H as [Hscript H].
    apply andb_true_iff in H as [Hpow Hasert].
    apply Z.leb_le in Horu. apply Z.leb_le in Hres0.
    apply Z.leb_le in Hrescap.
    repeat split; assumption.
  - intros [Hutxo [Horu [[Hres0 Hrescap] [Hscript [Hpow Hasert]]]]].
    rewrite Hutxo, Hscript, Hpow, Hasert.
    apply Z.leb_le in Horu. apply Z.leb_le in Hres0.
    apply Z.leb_le in Hrescap.
    rewrite Horu, Hres0, Hrescap.
    reflexivity.
Qed.

Corollary check_executable_kernel_false_iff :
  forall e : ExecutableKernelState,
    check_executable_kernel e = false <-> ~ executable_invariants e.
Proof.
  intros e. split.
  - intros Hf Hinv.
    apply check_executable_kernel_reflects in Hinv.
    rewrite Hf in Hinv. discriminate.
  - intros Hn. destruct (check_executable_kernel e) eqn:E; [| reflexivity].
    exfalso. apply Hn, check_executable_kernel_reflects, E.
Qed.

(** * The checker *)

Definition abstract_checker (m : MachineKernelState) : bool :=
  match machine_decode m with
  | None => false
  | Some e => check_executable_kernel e
  end.

(** * Contract

    Accept exactly the machine states that decode into a model state
    satisfying the kernel invariant. Nothing else. *)

Theorem checker_accepts_valid_kernel_state :
  forall (m : MachineKernelState) (e : ExecutableKernelState),
    machine_decode m = Some e ->
    executable_invariants e ->
    abstract_checker m = true.
Proof.
  intros m e Hdec Hinv. unfold abstract_checker.
  rewrite Hdec. apply check_executable_kernel_reflects, Hinv.
Qed.

Theorem checker_rejects_invalid_kernel_state :
  forall (m : MachineKernelState) (e : ExecutableKernelState),
    machine_decode m = Some e ->
    ~ executable_invariants e ->
    abstract_checker m = false.
Proof.
  intros m e Hdec Hinv. unfold abstract_checker.
  rewrite Hdec. apply check_executable_kernel_false_iff, Hinv.
Qed.

(** A state whose words do not fit the runtime domain is rejected, not
    silently truncated. *)
Theorem checker_rejects_unrepresentable_state :
  forall m : MachineKernelState,
    machine_decode m = None ->
    abstract_checker m = false.
Proof.
  intros m H. unfold abstract_checker. rewrite H. reflexivity.
Qed.

Theorem checker_decides_kernel_validity :
  forall m : MachineKernelState,
    abstract_checker m = true <->
    (exists e : ExecutableKernelState,
        machine_decode m = Some e /\ executable_invariants e).
Proof.
  intros m. unfold abstract_checker. split.
  - intros H. destruct (machine_decode m) as [e |] eqn:Hdec; [| discriminate].
    exists e. split; [reflexivity |].
    apply check_executable_kernel_reflects, H.
  - intros [e [Hdec Hinv]]. rewrite Hdec.
    apply check_executable_kernel_reflects, Hinv.
Qed.

(** * The checker is not a constant

    Without these witnesses the contract above would also be satisfied
    by [fun _ => false] (vacuously sound) or, after deleting the
    rejection clauses, by [fun _ => true]. Both directions are
    exhibited. *)

Definition accepting_witness : MachineKernelState :=
  {| mk_entry := EK_UTXO;
     mk_utxo_flag := true;
     mk_fees := 0;
     mk_burn := 0;
     mk_subsidy := 0;
     mk_res_stock := 0;
     mk_res_cap := 0;
     mk_oru_stock := 0;
     mk_script_ok := true;
     mk_pow_ok := true;
     mk_asert_ok := true |}.

(** Same words, but the UTXO domain flag is down. *)
Definition rejecting_witness : MachineKernelState :=
  {| mk_entry := EK_UTXO;
     mk_utxo_flag := false;
     mk_fees := 0;
     mk_burn := 0;
     mk_subsidy := 0;
     mk_res_stock := 0;
     mk_res_cap := 0;
     mk_oru_stock := 0;
     mk_script_ok := true;
     mk_pow_ok := true;
     mk_asert_ok := true |}.

Theorem checker_accepts_something :
  abstract_checker accepting_witness = true.
Proof. reflexivity. Qed.

Theorem checker_rejects_something :
  abstract_checker rejecting_witness = false.
Proof. reflexivity. Qed.

Theorem checker_is_not_constant :
  (exists m, abstract_checker m = true) /\
  (exists m, abstract_checker m = false).
Proof.
  split.
  - exists accepting_witness. exact checker_accepts_something.
  - exists rejecting_witness. exact checker_rejects_something.
Qed.

(** * Checker and step agree

    Accepting a state and then stepping it lands in a state the
    checker can still evaluate: the runtime never steps into a value
    it cannot represent without saying so. *)

Theorem checker_survives_machine_step :
  forall (m : MachineKernelState) (e : ExecutableKernelState)
         (i : UnifiedTransition.KernelInput) (m' : MachineKernelState),
    machine_decode m = Some e ->
    machine_step_of m i = Some m' ->
    abstract_checker m' =
      check_executable_kernel (ExecutableTransition.executable_step_of e i).
Proof.
  intros m e i m' Hdec Hstep.
  unfold abstract_checker.
  rewrite (machine_step_refines_executable_step m e i m' Hdec Hstep).
  reflexivity.
Qed.

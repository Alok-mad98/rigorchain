(** Failure preservation across the Coq runtime-model boundary.

    Invalid kernel input must stay rejected. The runtime model must
    not turn an invalid call into VALID.

    The previous version guarded rejection behind a
    [failure_path_enabled : bool := true] constant, so
    [runtime_rejects_invalid i] reduced to [negb (ki_utxo_ok i)] and
    the theorem reduced to [negb false = true]. The mutation attached
    to it flipped that constant, i.e. it proved that a switch inserted
    to be flipped could be flipped.

    Rejection is now the negation of the input flag conjunction, and
    the contract is stated in both directions plus non-constancy, so
    relaxing any conjunct breaks a proof.

    NOT CLAIMED: OCaml exceptions, binary harness, CPU traps.
*)
From Coq Require Import Bool.Bool.
From Coq Require Import ZArith.ZArith.
From RigorKernel Require Import Kernel.UnifiedState.
From RigorKernel Require Import Kernel.UnifiedTransition.
From RigorKernel Require Import Runtime.RuntimeState.

Local Open Scope Z_scope.

(** A kernel input that fails the UTXO-ok conjunct of [input_ok]. *)
Definition InvalidKernelInput (i : KernelInput) : Prop :=
  ki_utxo_ok i = false.

Definition InvalidRuntimeInput (i : RuntimeKernelInput) : Prop :=
  InvalidKernelInput i.

Theorem invalid_kernel_input_rejected :
  forall (s : KernelState) (i : KernelInput),
    InvalidKernelInput i ->
    ~ input_ok s i.
Proof.
  intros s i Hinv Hok.
  unfold InvalidKernelInput, input_ok in *.
  destruct Hok as [Hu _].
  rewrite Hinv in Hu.
  discriminate.
Qed.

(** The boolean part of [input_ok]. M41 drops a conjunct here. *)
Definition input_flags_ok (i : KernelInput) : bool :=
  andb (ki_utxo_ok i)
    (andb (ki_script_ok i)
      (andb (ki_pow_ok i) (ki_asert_ok i))).

Definition runtime_rejects_invalid (i : RuntimeKernelInput) : bool :=
  negb (input_flags_ok i).

(** Rejection is exactly "some domain flag is down" — both
    directions. Dropping a conjunct from [input_flags_ok] makes the
    right-to-left direction unprovable. *)
Theorem runtime_rejects_iff_some_flag_down :
  forall i : RuntimeKernelInput,
    runtime_rejects_invalid i = true <->
    (ki_utxo_ok i = false \/ ki_script_ok i = false \/
     ki_pow_ok i = false \/ ki_asert_ok i = false).
Proof.
  intros i. unfold runtime_rejects_invalid, input_flags_ok.
  rewrite negb_true_iff.
  split.
  - intros H.
    apply andb_false_iff in H as [Hu | H]; [left; exact Hu |].
    apply andb_false_iff in H as [Hs | H]; [right; left; exact Hs |].
    apply andb_false_iff in H as [Hp | Ha];
      [right; right; left; exact Hp | right; right; right; exact Ha].
  - intros [Hu | [Hs | [Hp | Ha]]]; apply andb_false_iff.
    + left. exact Hu.
    + right. apply andb_false_iff. left. exact Hs.
    + right. apply andb_false_iff. right. apply andb_false_iff.
      left. exact Hp.
    + right. apply andb_false_iff. right. apply andb_false_iff.
      right. exact Ha.
Qed.

Theorem runtime_failure_preserved :
  forall i : RuntimeKernelInput,
    InvalidRuntimeInput i ->
    runtime_rejects_invalid i = true.
Proof.
  intros i Hinv.
  apply runtime_rejects_iff_some_flag_down.
  left. exact Hinv.
Qed.

(** No false rejection: anything the kernel accepts is not rejected by
    the runtime. Without this the rejection function could be
    [fun _ => true] and [runtime_failure_preserved] would still hold. *)
Theorem runtime_accepts_kernel_valid_input :
  forall (s : KernelState) (i : KernelInput),
    input_ok s i ->
    runtime_rejects_invalid i = false.
Proof.
  intros s i Hok.
  unfold input_ok in Hok.
  destruct Hok as [Hu [Hs [Hp [Ha _]]]].
  unfold runtime_rejects_invalid, input_flags_ok.
  rewrite Hu, Hs, Hp, Ha. reflexivity.
Qed.

(** Non-constancy in both directions. *)
Definition all_flags_up : KernelInput :=
  {| ki_utxo_ok := true;
     ki_incoming := 0;
     ki_outgoing := 0;
     ki_fees := 0;
     ki_burn := 0;
     ki_subsidy := 0;
     ki_res_mint := 0;
     ki_script_ok := true;
     ki_pow_ok := true;
     ki_asert_ok := true |}.

Definition asert_flag_down : KernelInput :=
  {| ki_utxo_ok := true;
     ki_incoming := 0;
     ki_outgoing := 0;
     ki_fees := 0;
     ki_burn := 0;
     ki_subsidy := 0;
     ki_res_mint := 0;
     ki_script_ok := true;
     ki_pow_ok := true;
     ki_asert_ok := false |}.

Theorem runtime_rejection_is_not_constant :
  runtime_rejects_invalid all_flags_up = false /\
  runtime_rejects_invalid asert_flag_down = true.
Proof. split; reflexivity. Qed.

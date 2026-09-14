(** Bounded machine-word domain for the runtime layer.

    Models the 63-bit non-negative magnitude an extracted OCaml [int]
    provides. Arithmetic here is PARTIAL: it reports overflow as
    [None] instead of wrapping or silently widening.

    This module exists to make the runtime layer a genuine refinement
    obligation. In an unbounded [Z] model every arithmetic step
    succeeds, so a "runtime refines model" theorem is discharged by
    [exact]. Once the runtime carries a bound the model does not have,
    the obligation can fail, and proving it becomes real work.

    NOT CLAIMED: OCaml [int] semantics, ABI, CPU word width, wrapping
    behaviour of any concrete machine.
*)
From Coq Require Import ZArith.ZArith.
From Coq Require Import Bool.Bool.
From Coq Require Import Lia.

Local Open Scope Z_scope.

(** Same bound as [RuntimeModel.IntegerBoundary.runtime_signed_max]. *)
Definition word_max : Z := 2 ^ 62 - 1.

Definition wf_word (z : Z) : Prop := 0 <= z <= word_max.

Definition wf_wordb (z : Z) : bool := andb (0 <=? z) (z <=? word_max).

Lemma word_max_pos : 0 < word_max.
Proof. unfold word_max. lia. Qed.

Lemma wf_wordb_true_iff : forall z, wf_wordb z = true <-> wf_word z.
Proof.
  intros z. unfold wf_wordb, wf_word.
  rewrite andb_true_iff, !Z.leb_le. reflexivity.
Qed.

Lemma wf_wordb_false_iff : forall z, wf_wordb z = false <-> ~ wf_word z.
Proof.
  intros z. split.
  - intros Hf Hw. apply wf_wordb_true_iff in Hw.
    rewrite Hf in Hw. discriminate.
  - intros Hn. destruct (wf_wordb z) eqn:E; [| reflexivity].
    exfalso. apply Hn, wf_wordb_true_iff, E.
Qed.

(** Checked addition. [None] means "not representable"; it never
    returns a wrapped value. *)
Definition w_add (a b : Z) : option Z :=
  if wf_wordb (a + b) then Some (a + b) else None.

Lemma w_add_some_inv :
  forall a b s, w_add a b = Some s -> s = a + b /\ wf_word s.
Proof.
  intros a b s H. unfold w_add in H.
  destruct (wf_wordb (a + b)) eqn:E; [| discriminate].
  injection H as <-. split; [reflexivity |].
  apply wf_wordb_true_iff, E.
Qed.

Lemma w_add_none_inv :
  forall a b, w_add a b = None -> ~ wf_word (a + b).
Proof.
  intros a b H. unfold w_add in H.
  destruct (wf_wordb (a + b)) eqn:E; [discriminate |].
  apply wf_wordb_false_iff, E.
Qed.

Lemma w_add_of_wf :
  forall a b, wf_word (a + b) -> w_add a b = Some (a + b).
Proof.
  intros a b H. unfold w_add.
  apply wf_wordb_true_iff in H. rewrite H. reflexivity.
Qed.

(** * Non-vacuity witnesses

    These theorems exist so that the bound cannot later be relaxed into
    decoration without a proof failing. They state that the partiality
    introduced above is reachable, i.e. that [w_add] is not a total
    function in disguise. *)

Theorem w_add_overflow_reachable :
  exists a b, wf_word a /\ wf_word b /\ w_add a b = None.
Proof.
  exists word_max, 1.
  split; [unfold wf_word, word_max; lia |].
  split; [unfold wf_word, word_max; lia |].
  unfold w_add.
  destruct (wf_wordb (word_max + 1)) eqn:E; [| reflexivity].
  apply wf_wordb_true_iff in E. unfold wf_word in E. lia.
Qed.

Theorem w_add_negative_result_rejected :
  forall a b, a + b < 0 -> w_add a b = None.
Proof.
  intros a b H. unfold w_add.
  destruct (wf_wordb (a + b)) eqn:E; [| reflexivity].
  apply wf_wordb_true_iff in E. unfold wf_word in E. lia.
Qed.

(** A value the unbounded model accepts and the word domain does not.
    This is the gap the runtime refinement has to discharge. *)
Theorem word_domain_strictly_smaller_than_Z :
  exists z : Z, ~ wf_word z.
Proof.
  exists (word_max + 1). unfold wf_word. lia.
Qed.

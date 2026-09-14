(** RigorKernel.Kernel.ORUValidator — executable ORU check.

    Scope: consensus kernel only.

    [check_oru] is a boolean reflection of [valid_oru_tx].
    No new consensus rule.

    NOT CLAIMED:
      C++ implementation equivalence
      Full chain validity
      Runtime rejection equivalence
*)
From Coq Require Import Bool.Bool.
From Coq Require Import ZArith.ZArith.
From RigorKernel Require Import Supply.ORUModel.
From RigorKernel.proofs.ORU_Conservation Require Import ORUConservation.

Local Open Scope Z_scope.

Definition ORUState := kernel_tx.

Definition valid_oru_tx_b (t : oru_tx) : bool :=
  andb (0 <=? oru_in t)
  (andb (0 <=? oru_out t)
  (andb (0 <=? res_in t)
  (andb (0 <=? res_out t)
  (andb (oru_out t <=? oru_in t)
        (match tx_dom t with
         | DomORU => andb (res_in t =? 0) (res_out t =? 0)
         | DomRES => andb (0 <? oru_in t) (res_out t <=? res_in t)
         end))))).

Lemma valid_oru_tx_b_reflect :
  forall t, valid_oru_tx_b t = true <-> valid_oru_tx t.
Proof.
  intros t. unfold valid_oru_tx_b, valid_oru_tx.
  destruct (tx_dom t); simpl;
    rewrite !andb_true_iff, ?Z.leb_le, ?Z.ltb_lt, ?Z.eqb_eq;
    tauto.
Qed.

Definition check_oru (s : ORUState) : bool := valid_oru_tx_b (to_oru s).

Theorem check_oru_valid :
  forall s, check_oru s = true -> valid_tx s.
Proof.
  intros s H. unfold valid_tx, check_oru in *.
  apply valid_oru_tx_b_reflect. exact H.
Qed.

(** Statement: a passing check implies [no_hidden_inflation].
    Assumptions: none beyond the existing theorem.
    Consensus kernel only: yes. *)
Theorem check_oru_no_hidden_inflation :
  forall s,
    check_oru s = true ->
    oru_input s = oru_output s + fee s.
Proof.
  intros s H. apply no_hidden_inflation. apply check_oru_valid. exact H.
Qed.

(** Statement: a passing check implies [oru_conservation_extended].
    Assumptions: none beyond the existing theorem.
    Consensus kernel only: yes. *)
Theorem check_oru_sound :
  forall s,
    check_oru s = true ->
    new_supply s = old_supply s + subsidy s + fees s - burn s
    /\ oru_input s = oru_output s + fee s.
Proof.
  intros s H.
  pose proof (check_oru_valid s H) as Hv.
  split.
  - apply oru_conservation_extended. exact Hv.
  - apply no_hidden_inflation. exact Hv.
Qed.

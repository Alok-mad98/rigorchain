(** Byte-list lexicographic order lemmas (RFC-000 memcmp-style). *)
From Coq Require Import Arith.Arith.
From Coq Require Import List.
From Coq Require Import Lia.
From Coq Require Import Bool.
Import ListNotations.
Require Import Rigor.RigorBytes.

Lemma bytes_ltb_cons_nil :
  forall (b : byte) (bs : list byte), bytes_ltb (b :: bs) nil = false.
Proof. reflexivity. Qed.

Lemma nat_ltb_false_gt :
  forall n m : nat, (n <? m) = false -> n <> m -> m < n.
Proof.
  intros n m H Hne. apply Nat.ltb_ge in H. lia.
Qed.

Lemma bytes_ltb_irrefl :
  forall (bs : list byte), bytes_ltb bs bs = false.
Proof.
  induction bs as [|x xs IH]; simpl; eauto.
  destruct (Nat.eq_dec x x); [exact IH | congruence].
Qed.

Lemma bytes_ltb_trans :
  forall (a b c : list byte),
    bytes_ltb a b = true ->
    bytes_ltb b c = true ->
    bytes_ltb a c = true.
Proof.
  intros a b c Hab Hbc.
  revert b c Hab Hbc.
  induction a as [|x xs IH]; intros [|y ys] [|z zs] Hab Hbc;
    simpl in *; try reflexivity; try discriminate.
  destruct (Nat.eq_dec x y) as [Exy|Nxy].
  - subst y. destruct (Nat.eq_dec x z) as [Exz|Nxz].
    + subst z. exact (IH ys zs Hab Hbc).
    + simpl in Hbc. exact Hbc.
  - destruct (Nat.eq_dec y z) as [Eyz|Nyz].
    + subst z. simpl. destruct (Nat.eq_dec x y) as [Exy'|_]; [contradiction Nxy| exact Hab].
    + simpl in Hab, Hbc. apply Nat.ltb_lt in Hab. apply Nat.ltb_lt in Hbc.
      simpl. destruct (Nat.eq_dec x z) as [Exz|Nxz]; [subst; lia | apply Nat.ltb_lt; lia].
Qed.

Lemma bytes_ltb_antisym :
  forall (a b : list byte),
    bytes_ltb a b = true ->
    bytes_ltb b a = true ->
    a = b.
Proof.
  induction a as [|xa xa_rest IHa]; intros [|xb xb_rest] Hab Hba.
  - reflexivity.
  - simpl in Hba. discriminate Hba.
  - simpl in Hab. discriminate Hab.
  - simpl in Hab, Hba. destruct (Nat.eq_dec xa xb) as [Exb|Nxb].
    + subst xb. simpl in Hab, Hba.
      destruct (Nat.eq_dec xa xa) eqn:E; [clear E | congruence].
      simpl in Hba.
      assert (Et : xa_rest = xb_rest) by (apply IHa; eauto).
      subst. reflexivity.
    + simpl in Hab, Hba.
      destruct (Nat.eq_dec xa xb) as [Exb'|_]; [contradiction Nxb|].
      destruct (Nat.eq_dec xb xa) as [Ebx|_].
      * exfalso. apply Nxb. symmetry. exact Ebx.
      * apply Nat.ltb_lt in Hab. apply Nat.ltb_lt in Hba. lia.
Qed.

Lemma bytes_ltb_total :
  forall (a b : list byte),
    a = b \/ bytes_ltb a b = true \/ bytes_ltb b a = true.
Proof.
  induction a as [|xa xa_rest IHa]; intros [|xb xb_rest].
  - left. reflexivity.
  - right. left. reflexivity.
  - right. right. reflexivity.
  - destruct (Nat.eq_dec xa xb) as [Exb|Nxb].
    + subst xb. destruct (IHa xb_rest) as [E | [Hab | Hba]].
      * left. f_equal. exact E.
      * right. left. simpl. destruct (Nat.eq_dec xa xa); [exact Hab | congruence].
      * right. right. simpl. destruct (Nat.eq_dec xa xa); [exact Hba | congruence].
    + destruct (xa <? xb)%nat eqn:Hc.
      * right. left. simpl.
        destruct (Nat.eq_dec xa xb) as [Exb'|_]; [exfalso; apply Nxb; exact Exb'| rewrite Hc; reflexivity].
      * right. right. simpl.
        destruct (Nat.eq_dec xa xb) as [Exb'|_]; [exfalso; apply Nxb; exact Exb'|].
        destruct (Nat.eq_dec xb xa) as [Ebx|_]; [exfalso; apply Nxb; symmetry; exact Ebx|].
        apply Nat.ltb_lt. exact (nat_ltb_false_gt xa xb Hc Nxb).
Qed.

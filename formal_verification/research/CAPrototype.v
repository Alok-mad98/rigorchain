(** P0-045 CA research prototype.

    Isolated toy model of a cyclic elementary cellular automaton.
    NOT a RIGOR consensus object. NOT a C++ refinement.
    NOT listed in formal_verification/coq/_CoqProject.

    What this file proves:
      - a fixed rule + fixed state is a function (determinism)
      - step preserves length (boundedness of the model state)
      - Rule 90 cell = left XOR right
      - Rule 90 is F2-linear on equal-length rings

    What this file does NOT prove:
      - any RIGOR ConnectBlock / OpuHash / sighash / classifier property
      - collision resistance, preimage resistance, or PoW security
      - Rule 110 universality (irrelevant to the protocol)
      - that CA should be adopted

    Rule 90 linearity is recorded because it is an argument AGAINST
    using CA as a mixer, commitment, or hash substitute.

    No [Axiom] / [Admitted] / [Parameter]. *)

From Coq Require Import List.
From Coq Require Import Bool.
From Coq Require Import Arith.
From Coq Require Import Lia.
Import ListNotations.

Definition CA_PROTOCOL_USED : bool := false.
Definition FORMALIZATION_VALUE_FOR_PROTOCOL : bool := false.
Definition cpp_refinement_proved : bool := false.

Theorem ca_not_a_protocol_object :
  CA_PROTOCOL_USED = false.
Proof. reflexivity. Qed.

Theorem ca_formalization_does_not_close_rigor :
  FORMALIZATION_VALUE_FOR_PROTOCOL = false.
Proof. reflexivity. Qed.

Theorem ca_cpp_refinement_not_proved :
  cpp_refinement_proved = false.
Proof. reflexivity. Qed.

(** * Model *)

Definition ca_state := list bool.

Definition RULE30 : nat := 30.
Definition RULE90 : nat := 90.
Definition RULE110 : nat := 110.

Definition neigh_idx (l c r : bool) : nat :=
  (if l then 4 else 0) + (if c then 2 else 0) + (if r then 1 else 0).

Definition ca_cell (rule : nat) (l c r : bool) : bool :=
  Nat.testbit rule (neigh_idx l c r).

Definition nth_cyc (s : ca_state) (i : nat) : bool :=
  match length s with
  | 0 => false
  | n => nth (Nat.modulo i n) s false
  end.

Fixpoint mapi_aux (f : nat -> bool -> bool) (i : nat) (xs : ca_state) : ca_state :=
  match xs with
  | [] => []
  | x :: xs' => f i x :: mapi_aux f (S i) xs'
  end.

Definition mapi (f : nat -> bool -> bool) (xs : ca_state) : ca_state :=
  mapi_aux f 0 xs.

Definition ca_step (rule : nat) (s : ca_state) : ca_state :=
  let n := length s in
  mapi (fun i _ =>
    ca_cell rule (nth_cyc s (i + n - 1)) (nth_cyc s i) (nth_cyc s (i + 1))) s.

Definition ca_iter (rule steps : nat) (s : ca_state) : ca_state :=
  Nat.iter steps (ca_step rule) s.

(** * Determinism — definitional. Not a protocol theorem. *)

Theorem ca_deterministic :
  forall rule steps s,
    ca_iter rule steps s = ca_iter rule steps s.
Proof. reflexivity. Qed.

Theorem ca_step_ext :
  forall rule s t,
    s = t -> ca_step rule s = ca_step rule t.
Proof. intros; subst; reflexivity. Qed.

Theorem ca_iter_ext :
  forall rule steps s t,
    s = t -> ca_iter rule steps s = ca_iter rule steps t.
Proof. intros; subst; reflexivity. Qed.

(** * Boundedness — length is the only size; it does not grow. *)

Lemma mapi_aux_length :
  forall f i xs, length (mapi_aux f i xs) = length xs.
Proof.
  intros f i xs. revert i.
  induction xs as [|x xs IH]; intros i; simpl; auto.
Qed.

Lemma mapi_length :
  forall f xs, length (mapi f xs) = length xs.
Proof. intros. apply mapi_aux_length. Qed.

Theorem ca_length_preserved :
  forall rule s, length (ca_step rule s) = length s.
Proof.
  intros rule s.
  unfold ca_step.
  apply mapi_length.
Qed.

Theorem ca_iter_length_preserved :
  forall rule steps s,
    length (ca_iter rule steps s) = length s.
Proof.
  intros rule steps s.
  unfold ca_iter.
  induction steps as [|steps IH]; simpl; auto.
  rewrite ca_length_preserved.
  exact IH.
Qed.

Theorem ca_empty_fixed :
  forall rule, ca_step rule [] = [].
Proof. reflexivity. Qed.

(** * Invariant: all-zero is a fixed point when rule bit 0 is clear.

    Rules 30, 90, 110 all have bit 0 = 0 (neighborhood 000 -> 0). *)

Lemma testbit_rules_bit0 :
  Nat.testbit RULE30 0 = false /\
  Nat.testbit RULE90 0 = false /\
  Nat.testbit RULE110 0 = false.
Proof. vm_compute. repeat split; reflexivity. Qed.

Lemma nth_repeat_false :
  forall n i, nth i (repeat false n) false = false.
Proof.
  induction n as [|n IH]; intros i; simpl.
  - destruct i; reflexivity.
  - destruct i; simpl; auto.
Qed.

Lemma nth_cyc_repeat_false :
  forall n i, nth_cyc (repeat false n) i = false.
Proof.
  intros n i.
  unfold nth_cyc.
  destruct (length (repeat false n)) eqn:Hlen.
  - reflexivity.
  - apply nth_repeat_false.
Qed.

Lemma ca_cell_000_false :
  forall rule,
    Nat.testbit rule 0 = false ->
    ca_cell rule false false false = false.
Proof.
  intros rule H.
  unfold ca_cell, neigh_idx. simpl. exact H.
Qed.

(** Concrete all-zero lock (8 cells). A general [repeat false n] proof
    is possible but not protocol-useful. *)

Definition zero8 : ca_state :=
  [false; false; false; false; false; false; false; false].

Definition single8 : ca_state :=
  [false; false; false; true; false; false; false; false].

Definition alt8 : ca_state :=
  [true; false; true; false; true; false; true; false].

Definition ones8 : ca_state :=
  [true; true; true; true; true; true; true; true].

Theorem ca_invariant_zero8 :
  ca_step RULE30 zero8 = zero8 /\
  ca_step RULE90 zero8 = zero8 /\
  ca_step RULE110 zero8 = zero8.
Proof. vm_compute. repeat split; reflexivity. Qed.

(** * Rule 90 algebra — the load-bearing negative result *)

Lemma rule90_cell :
  forall l c r, ca_cell RULE90 l c r = xorb l r.
Proof.
  intros l c r.
  unfold ca_cell, RULE90, neigh_idx.
  destruct l; destruct c; destruct r; vm_compute; reflexivity.
Qed.

Fixpoint xor_states (a b : ca_state) : ca_state :=
  match a, b with
  | x :: a', y :: b' => xorb x y :: xor_states a' b'
  | _, _ => []
  end.

Lemma xor_states_length :
  forall a b,
    length a = length b ->
    length (xor_states a b) = length a.
Proof.
  induction a as [|x a IH]; intros b Hlen.
  - reflexivity.
  - destruct b as [|y b]; simpl in Hlen; try discriminate.
    simpl. f_equal. apply IH. lia.
Qed.

Lemma nth_xor_states :
  forall a b i,
    length a = length b ->
    nth i (xor_states a b) false = xorb (nth i a false) (nth i b false).
Proof.
  induction a as [|x a IH]; intros b i Hlen.
  - destruct b; [destruct i; reflexivity|discriminate].
  - destruct b as [|y b]; simpl in Hlen; try discriminate.
    destruct i; simpl; auto.
Qed.

Lemma nth_cyc_xor :
  forall a b i,
    length a = length b ->
    nth_cyc (xor_states a b) i = xorb (nth_cyc a i) (nth_cyc b i).
Proof.
  intros a b i Hab.
  unfold nth_cyc.
  rewrite (xor_states_length a b Hab).
  replace (length b) with (length a) by (symmetry; exact Hab).
  case_eq (length a); intros.
  - reflexivity.
  - apply nth_xor_states. exact Hab.
Qed.

Lemma xorb_assoc4 :
  forall la ra lb rb,
    xorb (xorb la lb) (xorb ra rb) = xorb (xorb la ra) (xorb lb rb).
Proof.
  intros.
  destruct la; destruct ra; destruct lb; destruct rb; reflexivity.
Qed.

Lemma nth_mapi_aux :
  forall f i0 xs k,
    k < length xs ->
    nth k (mapi_aux f i0 xs) false = f (i0 + k) (nth k xs false).
Proof.
  intros f i0 xs.
  revert i0.
  induction xs as [|x xs IH]; intros i0 k Hk; simpl in Hk; try lia.
  destruct k as [|k]; simpl.
  - rewrite Nat.add_0_r. reflexivity.
  - rewrite IH by lia.
    f_equal. lia.
Qed.

Lemma nth_mapi :
  forall f xs k,
    k < length xs ->
    nth k (mapi f xs) false = f k (nth k xs false).
Proof.
  intros f xs k Hk.
  unfold mapi.
  rewrite nth_mapi_aux by exact Hk.
  reflexivity.
Qed.

Lemma list_bool_nth_ext :
  forall a b,
    length a = length b ->
    (forall i, nth i a false = nth i b false) ->
    a = b.
Proof.
  induction a as [|x a IH]; intros b Hlen Hnth.
  - destruct b; [reflexivity|discriminate].
  - destruct b as [|y b]; [discriminate|].
    f_equal.
    + specialize (Hnth 0). simpl in Hnth. exact Hnth.
    + apply IH.
      * simpl in Hlen. lia.
      * intros i. specialize (Hnth (S i)). simpl in Hnth. exact Hnth.
Qed.

Lemma ca_step_90_nth :
  forall s k,
    k < length s ->
    nth k (ca_step RULE90 s) false =
      xorb (nth_cyc s (k + length s - 1)) (nth_cyc s (k + 1)).
Proof.
  intros s k Hk.
  unfold ca_step.
  rewrite nth_mapi by exact Hk.
  rewrite rule90_cell.
  reflexivity.
Qed.

Lemma ca_step_90_nonempty_length :
  forall x s, length (ca_step RULE90 (x :: s)) = S (length s).
Proof.
  intros. rewrite ca_length_preserved. reflexivity.
Qed.

Theorem rule90_linear :
  forall a b,
    length a = length b ->
    ca_step RULE90 (xor_states a b) =
      xor_states (ca_step RULE90 a) (ca_step RULE90 b).
Proof.
  intros a b Hlen.
  apply list_bool_nth_ext.
  - rewrite xor_states_length by (rewrite !ca_length_preserved; exact Hlen).
    rewrite !ca_length_preserved.
    apply xor_states_length. exact Hlen.
  - intros i.
    destruct (lt_dec i (length a)) as [Hi|Hge].
    + assert (Hi_xor : i < length (xor_states a b))
        by (rewrite xor_states_length by exact Hlen; exact Hi).
      assert (Hi_sa : i < length (ca_step RULE90 a))
        by (rewrite ca_length_preserved; exact Hi).
      assert (Hi_sb : i < length (ca_step RULE90 b))
        by (rewrite ca_length_preserved; rewrite <- Hlen; exact Hi).
      assert (Hi_sxor : i < length (ca_step RULE90 (xor_states a b)))
        by (rewrite ca_length_preserved; exact Hi_xor).
      rewrite ca_step_90_nth by exact Hi_xor.
      rewrite xor_states_length by exact Hlen.
      rewrite !nth_cyc_xor by exact Hlen.
      rewrite xorb_assoc4.
      rewrite nth_xor_states by (rewrite !ca_length_preserved; exact Hlen).
      rewrite (ca_step_90_nth a) by exact Hi.
      rewrite (ca_step_90_nth b) by (rewrite <- Hlen; exact Hi).
      replace (nth_cyc b (i + length a - 1))
        with (nth_cyc b (i + length b - 1))
        by (rewrite Hlen; reflexivity).
      reflexivity.
    + apply Nat.nlt_ge in Hge.
      rewrite nth_overflow.
      * rewrite nth_overflow; [reflexivity|].
        rewrite xor_states_length by (rewrite !ca_length_preserved; exact Hlen).
        rewrite !ca_length_preserved. exact Hge.
      * rewrite ca_length_preserved.
        rewrite xor_states_length by exact Hlen.
        exact Hge.
Qed.

(** * Golden vectors — same bytes as ca_research.py *)

Example golden_zero30 : ca_step RULE30 zero8 = zero8.
Proof. vm_compute. reflexivity. Qed.
Example golden_zero90 : ca_step RULE90 zero8 = zero8.
Proof. vm_compute. reflexivity. Qed.
Example golden_zero110 : ca_step RULE110 zero8 = zero8.
Proof. vm_compute. reflexivity. Qed.

Example golden_single30 :
  ca_step RULE30 single8 =
    [false; false; true; true; true; false; false; false].
Proof. vm_compute. reflexivity. Qed.

Example golden_single90 :
  ca_step RULE90 single8 =
    [false; false; true; false; true; false; false; false].
Proof. vm_compute. reflexivity. Qed.

Example golden_single110 :
  ca_step RULE110 single8 =
    [false; false; true; true; false; false; false; false].
Proof. vm_compute. reflexivity. Qed.

Example golden_alt30 : ca_step RULE30 alt8 = alt8.
Proof. vm_compute. reflexivity. Qed.

Example golden_alt90 : ca_step RULE90 alt8 = zero8.
Proof. vm_compute. reflexivity. Qed.

Example golden_alt110 : ca_step RULE110 alt8 = ones8.
Proof. vm_compute. reflexivity. Qed.

Example golden_ones30 : ca_step RULE30 ones8 = zero8.
Proof. vm_compute. reflexivity. Qed.

Example golden_ones90 : ca_step RULE90 ones8 = zero8.
Proof. vm_compute. reflexivity. Qed.

Example golden_ones110 : ca_step RULE110 ones8 = zero8.
Proof. vm_compute. reflexivity. Qed.

Example golden_rule90_walk :
  ca_iter RULE90 4 single8 = zero8.
Proof. vm_compute. reflexivity. Qed.

(** Finite Rule 90 linearity lock over the 8-cell byte space is in
    [ca_research.py] (256×256). [rule90_linear] is the forall theorem. *)

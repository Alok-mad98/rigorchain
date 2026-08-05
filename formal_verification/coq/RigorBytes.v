(** Byte-level consensus encodings (RFC-009 / RFC-000). *)
From Coq Require Import Arith.Arith.
From Coq Require Import List.
From Coq Require Import Lia.
From Coq Require Import ZArith.ZArith.
Import ListNotations.

Definition byte := nat.

Definition byte_ok (b : byte) : Prop := (b <=? 255)%nat = true.

Fixpoint all_byte_ok (bs : list byte) : Prop :=
  match bs with
  | nil => True
  | b :: rest => (b <=? 255)%nat = true /\ all_byte_ok rest
  end.

Definition bytes32b := list byte.

Definition is_bytes32 (b : list byte) : bool := Nat.eqb (length b) 32.

Fixpoint repeat_byte (n : nat) (v : byte) : list byte :=
  match n with
  | O => nil
  | S n' => v :: repeat_byte n' v
  end.

Definition bytes32_zero : bytes32b := repeat_byte 32 0.

Fixpoint app_bytes (xs ys : list byte) : list byte :=
  match xs with
  | nil => ys
  | x :: xrest => x :: app_bytes xrest ys
  end.

Definition app_bytes32 (a b c d : list byte) : list byte :=
  app_bytes a (app_bytes b (app_bytes c d)).

Fixpoint concat_txid_lists (l : list (list byte)) : list byte :=
  match l with
  | nil => nil
  | x :: xs => app_bytes x (concat_txid_lists xs)
  end.

Fixpoint bytes_ltb (a b : list byte) : bool :=
  match a, b with
  | nil, nil => false
  | nil, _ => true
  | _, nil => false
  | x :: xs, y :: ys =>
      match Nat.eq_dec x y with
      | left _ => bytes_ltb xs ys
      | right _ => (x <? y)%nat
      end
  end.

Fixpoint insert_sorted_txid (x : list byte) (l : list (list byte)) : list (list byte) :=
  match l with
  | nil => [x]
  | y :: ys =>
      if bytes_ltb x y then x :: y :: ys else y :: insert_sorted_txid x ys
  end.

Fixpoint sort_txid_bytes (l : list (list byte)) : list (list byte) :=
  match l with
  | nil => nil
  | x :: xs => insert_sorted_txid x (sort_txid_bytes xs)
  end.

Definition le32_bytes (z : Z) : list byte :=
  let b0 := Z.to_nat (Z.modulo z 256) in
  let z1 := Z.div z 256 in
  let b1 := Z.to_nat (Z.modulo z1 256) in
  let z2 := Z.div z1 256 in
  let b2 := Z.to_nat (Z.modulo z2 256) in
  let z3 := Z.div z2 256 in
  let b3 := Z.to_nat (Z.modulo z3 256) in
  [b0; b1; b2; b3].

(** Little-endian uint256 interpretation (matches Python / ``arith_uint256``). *)
Fixpoint uint256_of_bytes_go (bs : list byte) (acc : Z) (pow : Z) : Z :=
  match bs with
  | nil => acc
  | b :: rest =>
      uint256_of_bytes_go rest (acc + (Z.of_nat b) * pow) (pow * 256)
  end.

Definition uint256_of_bytes (bs : list byte) : Z :=
  Z.modulo (uint256_of_bytes_go bs 0 1) (Z.shiftl 1 256).

Section UInt256LittleEndianBridge.
Local Open Scope Z_scope.

Definition nat_pow256 (n : nat) : nat := Nat.pow 256 n.

Lemma zadd_match_elim (a b : Z) :
  (a + match b with | 0 => 0 | Z.pos p => Z.pos p | Z.neg p => Z.neg p end) =
  (a + b)%Z.
Proof. destruct b; reflexivity. Qed.

Lemma uint256_go_absorb_pow :
  forall (n : nat) bs (head : Z),
  uint256_of_bytes_go bs head (Z.of_nat (nat_pow256 n)) =
  (head + Z.of_nat (nat_pow256 n) * uint256_of_bytes_go bs 0 1)%Z.
Proof.
  intros n bs head.
  revert n head.
  induction bs as [|b rest IH]; intros n head; simpl.
  - destruct n; simpl nat_pow256; simpl; ring.
  - destruct n as [|n0] eqn:En.
    + simpl nat_pow256. simpl.
      replace (Z.of_nat b * 1) with (Z.of_nat b) by ring.
      replace (head + Z.of_nat b * 1) with (head + Z.of_nat b) by ring.
      assert (Hp : (256%Z) = Z.of_nat (nat_pow256 1)) by (simpl nat_pow256; reflexivity).
      pose proof (@IH 1%nat (head + Z.of_nat b)) as H1.
      pose proof (@IH 1%nat (Z.of_nat b)) as H2.
      pattern 256 in |- *.
      rewrite Hp.
      rewrite H1.
      rewrite <- Z.add_assoc.
      rewrite H2.
      rewrite zadd_match_elim.
      reflexivity.
    + simpl nat_pow256. simpl.
      set (pS := Z.of_nat (nat_pow256 (S n0))).
      assert (Hnat : (nat_pow256 (S (S n0)) = 256 * nat_pow256 (S n0))%nat).
      { simpl nat_pow256. reflexivity. }
      assert (HpSS : Z.of_nat (nat_pow256 (S (S n0))) = (pS * 256)%Z).
      { subst pS.
        pattern (nat_pow256 (S (S n0))).
        rewrite Hnat.
        rewrite Nat2Z.inj_mul.
        ring. }
      pose proof (@IH (S (S n0)) (head + Z.of_nat b * pS)) as Hlhs.
      rewrite HpSS in Hlhs.
      pose proof (@IH 1%nat (Z.of_nat b)) as Hrhs.
      replace (Z.of_nat b * 1) with (Z.of_nat b) by ring.
      assert (H256 : (256%Z) = Z.of_nat (nat_pow256 1)) by (simpl nat_pow256; reflexivity).
      rewrite Hlhs.
      pattern 256 in |- *.
      rewrite H256.
      rewrite Hrhs.
      ring.
Qed.

End UInt256LittleEndianBridge.

Lemma ofnat_mul256 (n : nat) :
  Z.of_nat (Nat.mul 256 n) = Z.mul (Z.of_nat 256) (Z.of_nat n).
Proof. apply Nat2Z.inj_mul. Qed.

Section UInt256LittleEndianSigma.
Local Open Scope Z_scope.

Lemma uint256_go_cons (b : byte) (bs : list byte) :
  uint256_of_bytes_go (b :: bs) 0 1 =
  (Z.of_nat b + 256 * uint256_of_bytes_go bs 0 1)%Z.
Proof.
  simpl.
  replace (Z.of_nat b * 1) with (Z.of_nat b) by ring.
  assert (H256 : (256%Z) = Z.of_nat (nat_pow256 1)) by (simpl nat_pow256; reflexivity).
  pose proof (uint256_go_absorb_pow 1 bs (Z.of_nat b)) as Hgo.
  assert (Heq256 : uint256_of_bytes_go bs (Z.of_nat b) 256 =
                    uint256_of_bytes_go bs (Z.of_nat b) (Z.of_nat (nat_pow256 1))).
  { reflexivity. }
  rewrite Heq256.
  rewrite Hgo.
  reflexivity.
Qed.

End UInt256LittleEndianSigma.

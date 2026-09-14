(** ReplayReorgCore/Hash32Order.v
    RFC-000 §6 byte-wise lexicographic order for 32-byte digests. *)
From Coq Require Import Arith.Arith.
From Coq Require Import List.
Import ListNotations.
Require Import Rigor.RigorBytes.
Require Import Rigor.RigorBytesOrder.

Definition byte := Rigor.RigorBytes.byte.
Definition Hash32 := list byte.

Fixpoint repeat_byte (n : nat) (v : byte) : list byte :=
  match n with
  | O => nil
  | S n' => v :: repeat_byte n' v
  end.

Definition hash32_zero : Hash32 := repeat_byte 32 0.

Definition is_hash32 (h : Hash32) : bool := Nat.eqb (length h) 32.

(** Deterministic tie-break for fork choice (PHASE-4C). *)
Definition hash32_lt (a b : Hash32) : bool := bytes_ltb a b.

(** Qed-sealed on purpose. `Defined` would let `vm_compute` unfold 32-byte
    list equality inside `gather_inputs` / `apply_block` and explode the
    NegativeCore harness. Dual-pool rejects are pinned by
    `valid_dual_pool_on_ins` instead (GOLDEN-VECTOR-DISCREPANCIES.md NEG-014/015). *)
Fixpoint list_eq_dec (l1 l2 : list byte) : {l1 = l2} + {l1 <> l2}.
Proof.
  destruct l1 as [|x1 t1]; destruct l2 as [|x2 t2].
  - left. reflexivity.
  - right. discriminate.
  - right. discriminate.
  - destruct (Nat.eq_dec x1 x2) as [Ex | Nx].
    + destruct (list_eq_dec t1 t2) as [Et | Nt].
      * left. f_equal; auto.
      * right. intro H. inversion H. subst. contradiction.
    + right. intro H. inversion H. subst. contradiction.
Qed.

Definition hash32_eq_dec (a b : Hash32) : {a = b} + {a <> b} :=
  list_eq_dec a b.

Theorem hash32_lt_irrefl :
  forall h : Hash32, hash32_lt h h = false.
Proof.
  intros. exact (bytes_ltb_irrefl h).
Qed.

Theorem hash32_lt_transitive :
  forall a b c : Hash32,
    hash32_lt a b = true ->
    hash32_lt b c = true ->
    hash32_lt a c = true.
Proof.
  intros. exact (bytes_ltb_trans _ _ _ H H0).
Qed.

Theorem hash32_lt_antisymmetric :
  forall a b : Hash32,
    hash32_lt a b = true ->
    hash32_lt b a = true ->
    a = b.
Proof.
  intros. exact (bytes_ltb_antisym _ _ H H0).
Qed.

Theorem hash32_lt_total :
  forall a b : Hash32,
    a = b \/ hash32_lt a b = true \/ hash32_lt b a = true.
Proof.
  intros. exact (bytes_ltb_total a b).
Qed.

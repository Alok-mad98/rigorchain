(** RigorKernel.Pow.Uint256Model — Phase B abstract 256-bit integers.

    Scope: consensus kernel only.

    [u256] is a [Z] together with a range predicate. This is an
    abstraction, not a binding of C++ [uint256] / [arith_uint256].

    NOT CLAIMED:
      C++ implementation equivalence
      Full chain validity
      Hash collision resistance
      Signature security
      Runtime rejection equivalence
*)
From Coq Require Import ZArith.ZArith.
From Coq Require Import Lia.

Local Open Scope Z_scope.

Definition u256 := Z.

Definition u256_mod : Z := 2 ^ 256.

Definition u256_ok (x : Z) : Prop := 0 <= x < u256_mod.

(** * Comparison *)

Definition u256_compare (a b : u256) : comparison := Z.compare a b.

Definition u256_ltb (a b : u256) : bool := a <? b.
Definition u256_leb (a b : u256) : bool := a <=? b.
Definition u256_eqb (a b : u256) : bool := a =? b.

(** * Addition / subtraction (unbounded Z; wrap is a separate combinator) *)

Definition u256_add (a b : u256) : u256 := a + b.
Definition u256_sub (a b : u256) : u256 := a - b.
Definition u256_add_wrap (a b : u256) : u256 := Z.modulo (a + b) u256_mod.

(** * Shifts (Z.shiftl / Z.shiftr: floor semantics on [Z]) *)

Definition u256_shl (a : u256) (n : Z) : u256 := Z.shiftl a n.
Definition u256_shr (a : u256) (n : Z) : u256 := Z.shiftr a n.

(** Statement: comparison is the [Z] order.
    Assumptions: none.
    Consensus kernel only: yes. *)
Theorem u256_compare_eq :
  forall a b : u256,
    u256_compare a b = Eq <-> a = b.
Proof. intros a b. apply Z.compare_eq_iff. Qed.

(** Statement: addition is commutative.
    Assumptions: none.
    Consensus kernel only: yes. *)
Theorem u256_add_comm :
  forall a b : u256, u256_add a b = u256_add b a.
Proof. intros a b. unfold u256_add. lia. Qed.

(** Statement: addition is associative.
    Assumptions: none.
    Consensus kernel only: yes. *)
Theorem u256_add_assoc :
  forall a b c : u256,
    u256_add (u256_add a b) c = u256_add a (u256_add b c).
Proof. intros a b c. unfold u256_add. lia. Qed.

(** Statement: subtraction undoes addition.
    Assumptions: none.
    Consensus kernel only: yes. *)
Theorem u256_sub_add :
  forall a b : u256, u256_sub (u256_add a b) b = a.
Proof. intros a b. unfold u256_add, u256_sub. lia. Qed.

(** Statement: a right shift of a non-negative value does not increase it.
    Assumptions: [0 <= a] and [0 <= n].
    Consensus kernel only: yes. *)
Theorem u256_shr_le :
  forall a n : Z,
    0 <= a ->
    0 <= n ->
    u256_shr a n <= a.
Proof.
  intros a n Ha Hn.
  unfold u256_shr.
  rewrite Z.shiftr_div_pow2 by lia.
  apply Z.div_le_upper_bound.
  - apply Z.pow_pos_nonneg; lia.
  - assert (Hpow : 1 <= 2 ^ n).
    { replace 1 with (2 ^ 0) by reflexivity.
      apply Z.pow_le_mono_r; lia. }
    nia.
Qed.

(** Statement: wrap-add lands in [0, 2^256).
    Assumptions: none ([Z.modulo] on a positive modulus).
    Consensus kernel only: yes. *)
Theorem u256_add_wrap_ok :
  forall a b : u256, u256_ok (u256_add_wrap a b).
Proof.
  intros a b.
  unfold u256_ok, u256_add_wrap, u256_mod.
  apply Z.mod_pos_bound. apply Z.pow_pos_nonneg; lia.
Qed.

(** RIGOR v0.1 — CAmount / uint64 boundary (P0-039).

    Not a proof that "integers never overflow in mathematics".
    This module records the legal consensus range and shows that
    in-range additions used by subsidy + mint − converge − fee_burn
    stay inside signed 64-bit (the C++ CAmount domain).

    cpp_refinement_proved remains false. No [Axiom] / [Admitted]. *)
From Coq Require Import ZArith.ZArith.
From Coq Require Import Lia.

Local Open Scope Z_scope.

Definition cpp_refinement_proved : bool := false.

Theorem amount_boundary_refinement_not_proved :
  cpp_refinement_proved = false.
Proof. reflexivity. Qed.

(** C++ int64_t. *)
Definition CAMOUNT_MIN : Z := -(2 ^ 63).
Definition CAMOUNT_MAX : Z := 2 ^ 63 - 1.

(** C++ MAX_MONEY = 10_000_000_000 * COIN, COIN = 100_000_000. *)
Definition COIN_Z : Z := 100000000.
Definition MAX_MONEY_Z : Z := 10000000000 * COIN_Z.

Definition camount_ok (n : Z) : Prop :=
  CAMOUNT_MIN <= n <= CAMOUNT_MAX.

Definition money_range (n : Z) : Prop :=
  0 <= n <= MAX_MONEY_Z.

Theorem max_money_leaves_add_headroom :
  2 * MAX_MONEY_Z <= CAMOUNT_MAX.
Proof. unfold MAX_MONEY_Z, COIN_Z, CAMOUNT_MAX, CAMOUNT_MIN. lia. Qed.

Theorem money_range_sum_no_wrap :
  forall a b,
    money_range a ->
    money_range b ->
    camount_ok (a + b).
Proof.
  intros a b Ha Hb.
  unfold money_range, camount_ok, CAMOUNT_MIN, CAMOUNT_MAX, MAX_MONEY_Z, COIN_Z in *.
  lia.
Qed.

Theorem circulating_delta_no_wrap :
  forall subsidy mint converge burn,
    money_range subsidy ->
    money_range mint ->
    money_range converge ->
    money_range burn ->
    0 <= subsidy + mint - converge - burn ->
    camount_ok (subsidy + mint - converge - burn).
Proof.
  intros s m c b Hs Hm Hc Hb Hnn.
  unfold money_range, camount_ok, CAMOUNT_MIN, CAMOUNT_MAX, MAX_MONEY_Z, COIN_Z in *.
  lia.
Qed.

Theorem in_range_sum_equals_mod_2_64 :
  forall a b,
    money_range a ->
    money_range b ->
    a + b = (a + b) mod (2 ^ 64).
Proof.
  intros a b Ha Hb.
  unfold money_range, MAX_MONEY_Z, COIN_Z in *.
  apply eq_sym. apply Z.mod_small. lia.
Qed.

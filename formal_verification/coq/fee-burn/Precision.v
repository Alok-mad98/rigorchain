(** fee-burn/Precision.v
    RFC-018 / RFC-017 RES atomic scale.

    Shipping lock: 1 RES = 1_000_000 atoms (six display decimals).
    The pre-RFC-017 scale 1 RES = 100 atoms is not the model.

    Display pins match C++ [FormatResAmount] / RPC [ValueFromAmountAsset]
    and Qt [BitcoinUnits::formatRes]: exactly six fractional digits.

    Not a C++ refinement. *)
From Coq Require Import ZArith.ZArith.
From Coq Require Import Lia.

Local Open Scope Z_scope.

Definition RES_ATOMIC_UNIT : Z := 1.
Definition RES_COIN : Z := 1000000.
Definition RES_DISPLAY_DECIMALS : nat := 6.
Definition RES_MAX_SUPPLY : Z := 21000000 * RES_COIN.

Lemma res_coin_is_1e6 :
  RES_COIN = 1000000.
Proof. reflexivity. Qed.

Lemma res_atomic_unit_is_one :
  RES_ATOMIC_UNIT = 1.
Proof. reflexivity. Qed.

Lemma res_display_decimals_is_six :
  RES_DISPLAY_DECIMALS = 6%nat.
Proof. reflexivity. Qed.

Lemma one_res_is_one_million_atoms :
  1 * RES_COIN = 1000000.
Proof. reflexivity. Qed.

(** The retired two-decimal lock. *)
Lemma res_coin_is_not_100 :
  RES_COIN <> 100.
Proof. discriminate. Qed.

Lemma one_res_is_not_100_atoms :
  RES_COIN <> 100.
Proof. apply res_coin_is_not_100. Qed.

Lemma res_max_supply_uses_six_decimal_coin :
  RES_MAX_SUPPLY = 21000000 * 1000000.
Proof. unfold RES_MAX_SUPPLY. rewrite res_coin_is_1e6. reflexivity. Qed.

Lemma res_max_supply_is_not_two_decimal_lock :
  RES_MAX_SUPPLY <> 21000000 * 100.
Proof.
  unfold RES_MAX_SUPPLY.
  rewrite res_coin_is_1e6.
  discriminate.
Qed.

(** Quotient / remainder of the six-decimal formatter.
    [FormatResAmount] prints [q "." r] with [r] zero-padded to 6 digits. *)
Definition res_display_parts (atoms : Z) : Z * Z :=
  (atoms / RES_COIN, atoms mod RES_COIN).

Lemma display_zero_res :
  res_display_parts 0 = (0, 0).
Proof. vm_compute. reflexivity. Qed.

Lemma display_one_res :
  res_display_parts RES_COIN = (1, 0).
Proof. vm_compute. reflexivity. Qed.

Lemma display_tenth_res :
  res_display_parts (RES_COIN / 10) = (0, 100000).
Proof. vm_compute. reflexivity. Qed.

Lemma display_one_atom :
  res_display_parts RES_ATOMIC_UNIT = (0, 1).
Proof. vm_compute. reflexivity. Qed.

Lemma tenth_res_atoms :
  RES_COIN / 10 = 100000.
Proof. vm_compute. reflexivity. Qed.

Lemma display_parts_nonneg :
  forall atoms, 0 <= atoms ->
    0 <= fst (res_display_parts atoms) /\
    0 <= snd (res_display_parts atoms) < RES_COIN.
Proof.
  intros atoms H.
  unfold res_display_parts, RES_COIN.
  split.
  - apply Z.div_pos; lia.
  - apply Z.mod_pos_bound. lia.
Qed.

Lemma display_reconstructs :
  forall atoms, 0 <= atoms ->
    fst (res_display_parts atoms) * RES_COIN
      + snd (res_display_parts atoms) = atoms.
Proof.
  intros atoms H.
  unfold res_display_parts; simpl.
  rewrite Z.mul_comm.
  symmetry.
  apply Z.div_mod.
  unfold RES_COIN. lia.
Qed.

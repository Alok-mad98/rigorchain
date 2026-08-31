(** RIGOR v0.1 — T(W) parameterization (P0-034).

    Shipping formula (params.h / Selection.v), with W an explicit argument:

      UNWINDOWED = floor(2^246 / MIN_CONVERGENCE_AMOUNT)
      T(W)       = floor(UNWINDOWED / W)

    C++ currently encodes the W = 32 case as [UNWINDOWED >> 5] because
    32 = 2^5. That shift is an implementation optimization, not a
    protocol necessity. This module uses integer division for every W > 0.

    Production T hex is NOT modified. T(36) is a formal candidate only.

    Honesty: no [Axiom] / [Admitted]. refinement_proved = false. *)
From Coq Require Import ZArith.
From Coq Require Import Lia.
Require Import RigorV01.WindowGeneric.
Open Scope Z_scope.

Definition MIN_CONVERGENCE_AMOUNT : Z := 100000000.

Definition RES_PROBABILITY_TARGET_UNWINDOWED : Z :=
  Z.shiftl 1 246 / MIN_CONVERGENCE_AMOUNT.

Definition T_of_W (W : Z) : Z :=
  RES_PROBABILITY_TARGET_UNWINDOWED / W.

(** Mint cost C = 2^(256-E) ORU at E = 246. Independent of draw-window W. *)
Definition CONVERGENCE_MINT_COST : Z :=
  Z.shiftl 1 256 / RES_PROBABILITY_TARGET_UNWINDOWED.

Definition unwindowed_mass (W : Z) : Z := T_of_W W * W.

(** A-9C pair: window W together with T(W), not W alone. *)
Definition target_matches_window (W T : Z) : Prop :=
  T = T_of_W W.

Lemma unwindowed_nonneg :
  0 <= RES_PROBABILITY_TARGET_UNWINDOWED.
Proof. vm_compute. discriminate. Qed.

Lemma mint_cost_is_1024_oru :
  CONVERGENCE_MINT_COST = 1024 * MIN_CONVERGENCE_AMOUNT.
Proof. vm_compute. reflexivity. Qed.

Theorem mint_cost_independent_of_W :
  forall W1 W2 : Z,
    CONVERGENCE_MINT_COST = 1024 * MIN_CONVERGENCE_AMOUNT.
Proof. intros. exact mint_cost_is_1024_oru. Qed.

(** Floor-division bracketing: T(W) is the unique integer satisfying
    T*W <= UNWINDOWED < (T+1)*W. *)
Theorem T_floor_bracket :
  forall W : Z,
    0 < W ->
    T_of_W W * W <= RES_PROBABILITY_TARGET_UNWINDOWED <
      (T_of_W W + 1) * W.
Proof.
  intros W HW.
  unfold T_of_W.
  pose proof unwindowed_nonneg as Hunw.
  pose proof (Z.div_mod RES_PROBABILITY_TARGET_UNWINDOWED W).
  assert (W <> 0) by lia.
  specialize (H H0).
  pose proof (Z.mod_pos_bound RES_PROBABILITY_TARGET_UNWINDOWED W HW).
  lia.
Qed.

Lemma div_bracket_unique :
  forall (n d t : Z),
    0 < d ->
    t * d <= n < (t + 1) * d ->
    t = n / d.
Proof.
  intros n d t Hd [Hlo Hhi].
  assert (t <= n / d).
  { apply Z.div_le_lower_bound; lia. }
  assert (n / d < t + 1).
  { apply Z.div_lt_upper_bound; lia. }
  lia.
Qed.

Theorem T_floor_bracket_unique :
  forall W T : Z,
    0 < W ->
    (T * W <= RES_PROBABILITY_TARGET_UNWINDOWED < (T + 1) * W) <->
    T = T_of_W W.
Proof.
  intros W T HW. split.
  - intros Hbr. unfold T_of_W.
    apply (div_bracket_unique RES_PROBABILITY_TARGET_UNWINDOWED W T HW Hbr).
  - intros Heq. subst T. apply T_floor_bracket. exact HW.
Qed.

(** Neutrality: retuning T with W keeps T(W)*W within a remainder < W of
    the same UNWINDOWED. Expected wins scale as n/C, not as n/(C)*W. *)
Theorem expected_wins_neutral_integer :
  forall (W n : Z),
    0 < W ->
    T_of_W W * W * n =
      (RES_PROBABILITY_TARGET_UNWINDOWED -
       RES_PROBABILITY_TARGET_UNWINDOWED mod W) * n.
Proof.
  intros W n HW.
  unfold T_of_W.
  pose proof (Z.div_mod RES_PROBABILITY_TARGET_UNWINDOWED W).
  assert (W <> 0) by lia.
  specialize (H H0).
  lia.
Qed.

Theorem mass_near_unwindowed :
  forall W : Z,
    0 < W ->
    0 <= RES_PROBABILITY_TARGET_UNWINDOWED - unwindowed_mass W < W.
Proof.
  intros W HW.
  unfold unwindowed_mass, T_of_W.
  pose proof (Z.div_mod RES_PROBABILITY_TARGET_UNWINDOWED W).
  assert (W <> 0) by lia.
  specialize (H H0).
  pose proof (Z.mod_pos_bound RES_PROBABILITY_TARGET_UNWINDOWED W HW).
  lia.
Qed.

(** Using a pinned T computed for W0 with a different window W is a
    different protocol state: the floor pair fails. *)
Theorem T_pinned_old_W_is_not_generic :
  T_of_W 32 <> T_of_W 36 /\
  ~ (T_of_W 32 * 36 <= RES_PROBABILITY_TARGET_UNWINDOWED <
       (T_of_W 32 + 1) * 36).
Proof.
  split.
  - vm_compute. discriminate.
  - intro H.
    apply (T_floor_bracket_unique 36 (T_of_W 32)) in H; [ | lia ].
    unfold T_of_W in H.
    vm_compute in H.
    discriminate.
Qed.

Theorem T32_matches_shipping_shift_encoding :
  T_of_W 32 = RES_PROBABILITY_TARGET_UNWINDOWED / 32 /\
  T_of_W 32 = Z.shiftr RES_PROBABILITY_TARGET_UNWINDOWED 5.
Proof.
  split.
  - reflexivity.
  - vm_compute. reflexivity.
Qed.

(** The shift encoding equals division ONLY because 32 = 2^5.
    It does not hold for W = 36. *)
Theorem shift5_is_not_T_of_36 :
  Z.shiftr RES_PROBABILITY_TARGET_UNWINDOWED 5 <> T_of_W 36.
Proof. vm_compute. discriminate. Qed.

Theorem T32_floor_pair :
  T_of_W 32 * 32 <= RES_PROBABILITY_TARGET_UNWINDOWED <
    (T_of_W 32 + 1) * 32.
Proof. apply T_floor_bracket. lia. Qed.

Theorem T36_floor_pair :
  T_of_W 36 * 36 <= RES_PROBABILITY_TARGET_UNWINDOWED <
    (T_of_W 36 + 1) * 36.
Proof. apply T_floor_bracket. lia. Qed.

(** Stale pair: W=36 with T(32) overshoots UNWINDOWED (issuance would scale). *)
Theorem stale_T32_with_W36_overshoots :
  RES_PROBABILITY_TARGET_UNWINDOWED < T_of_W 32 * 36.
Proof. vm_compute. reflexivity. Qed.

Theorem W32_residue_mod_window :
  RES_PROBABILITY_TARGET_UNWINDOWED mod 32 = 27.
Proof. vm_compute. reflexivity. Qed.

Theorem W36_residue_mod_window :
  RES_PROBABILITY_TARGET_UNWINDOWED mod 36 = 31.
Proof. vm_compute. reflexivity. Qed.

(** Saturation weight C·W is W-dependent by design (A-9C), not a hidden 32. *)
Definition saturation_weight (W : Z) : Z := CONVERGENCE_MINT_COST * W.

Theorem saturation_weight_at_32 :
  saturation_weight 32 = 32768 * MIN_CONVERGENCE_AMOUNT.
Proof.
  unfold saturation_weight.
  rewrite mint_cost_is_1024_oru.
  vm_compute. reflexivity.
Qed.

Theorem saturation_weight_at_36 :
  saturation_weight 36 = 36864 * MIN_CONVERGENCE_AMOUNT.
Proof.
  unfold saturation_weight.
  rewrite mint_cost_is_1024_oru.
  vm_compute. reflexivity.
Qed.

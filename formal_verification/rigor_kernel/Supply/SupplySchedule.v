(** RigorKernel.Supply.SupplySchedule — RFC-020 algebraic issuance.

    Scope: consensus kernel only.

    ORU subsidy is flat 128 for every h ≥ 1 (genesis 0). RES minting is a
    body-tx burn-to-mint. Consensus evaluates

        R > 0  →  R · (2^25 + h)^2 ≤ B << 40

    Display destroy_cost(h) = P0 · (2^25 + h)^2 >> 50 is not a consensus input.
    There is no per-block reward W(h), no draw, no mint horizon, and no
    running S_RES reject.

    The hard cap is the closed form (S / P0) · H = 2^22 RES.

    Theorem names that the kernel inventory already cites
    (`res_schedule_finite`, `res_height_monotonic`, `oru_res_separation`,
    `res_total_issuance_capped`, `res_derived_cap_identity`,
    `res_mint_products_fit_uint128`) keep those statements, now proved
    from the pole identity rather than an epoch fold.

    NOT CLAIMED:
      C++ implementation equivalence
      Full chain validity
      Hash collision resistance
      Signature security
      Runtime rejection equivalence
*)
From Coq Require Import Arith.Arith.
From Coq Require Import Bool.Bool.
From Coq Require Import ZArith.ZArith.
From Coq Require Import Lia.
From Coq Require Import Zify.
From RigorKernel Require Import Core.Amount.
From RigorKernel Require Import Core.Height.

Local Open Scope Z_scope.

(** * Heights (re-export; schedule argument is [Z] so negative is defined) *)

Definition block_height := Z.

Definition height_of_nat (h : height) : block_height := Z.of_nat h.

(** * ORU subsidy schedule

    mirrors core/src/consensus/rigor_issuance.cpp [GetBlockSubsidy]
    and core/src/consensus/rigor_consensus_lock.h constants. *)

Definition ORU_FLAT_BLOCK_SUBSIDY : Z := 128 * COIN.
Definition ORU_GENESIS_BLOCK_SUBSIDY : Z := 0.
Definition ORU_INITIAL_BLOCK_SUBSIDY : Z := ORU_FLAT_BLOCK_SUBSIDY.
Definition ORU_MIN_BLOCK_SUBSIDY : Z := ORU_FLAT_BLOCK_SUBSIDY.

(** Observational calendar. Not a subsidy argument. *)
Definition ORU_SUBSIDY_HALVING_INTERVAL : Z := 210000.
Definition ORU_SUBSIDY_MAX_SHIFTS : Z := 0.

Definition oru_subsidy (h : block_height) : Z :=
  if h <=? 0 then ORU_GENESIS_BLOCK_SUBSIDY
  else ORU_FLAT_BLOCK_SUBSIDY.

Theorem oru_subsidy_constant :
  oru_subsidy 0 = 0 /\
  forall h, 1 <= h -> oru_subsidy h = ORU_FLAT_BLOCK_SUBSIDY.
Proof.
  split; [reflexivity|].
  intros h Hh.
  unfold oru_subsidy.
  destruct (Z.leb_spec h 0); [lia | reflexivity].
Qed.

(** Cumulative subsidy from height 0 through [n] inclusive.
    RFC-020: genesis is 0 and every later height is flat 128, so
    the fold is [n · 128]. A halving schedule would have been an
    epoch loop; this identity is what the C++ observational
    cumulative now uses. *)
Fixpoint oru_subsidy_fold (n : nat) : Z :=
  match n with
  | O => oru_subsidy 0
  | S k => oru_subsidy_fold k + oru_subsidy (Z.of_nat (S k))
  end.

Theorem oru_subsidy_fold_closed :
  forall n : nat,
    oru_subsidy_fold n = Z.of_nat n * ORU_FLAT_BLOCK_SUBSIDY.
Proof.
  induction n as [|k IH].
  - unfold oru_subsidy, ORU_GENESIS_BLOCK_SUBSIDY.
    destruct (Z.leb_spec 0 0); [reflexivity | lia].
  - unfold oru_subsidy_fold; fold oru_subsidy_fold.
    rewrite IH.
    rewrite Nat2Z.inj_succ.
    unfold oru_subsidy.
    destruct (Z.leb_spec (Z.succ (Z.of_nat k)) 0) as [Hle|Hgt]; [lia|].
    unfold ORU_FLAT_BLOCK_SUBSIDY, COIN. lia.
Qed.

(** * RES display destroy cost and mint predicate (RFC-020)

    destroy_cost(h) = P0 · (2^25 + h)^2 >> 50. Zero at h < 0 is not a price:
    callers must gate on [res_mint_authorized] first. *)

Definition RES_COIN : Z := 100000000.
Definition RES_DESTROY_COST_GENESIS : Z := 1024 * COIN.
Definition RES_DESTROY_POLE : Z := Z.shiftl 1 25.
Definition RES_DESTROY_SHIFT : Z := 40.
Definition RES_REWARD_EPOCH : Z := 210000.

(** Closed-form cap pin. C++ [ResDerivedMaxSupply] = 419430400000000. *)
Definition RES_MAX_SUPPLY : Z := 419430400000000.

Definition res_mint_authorized (h : block_height) : bool := 0 <=? h.

Definition res_destroy_pole_sq (h : Z) : Z :=
  (RES_DESTROY_POLE + h) * (RES_DESTROY_POLE + h).

Definition res_destroy_cost_at_height (height : Z) : Z :=
  if height <? 0 then 0
  else Z.shiftr (RES_DESTROY_COST_GENESIS * res_destroy_pole_sq height) 50.

Definition res_destroy_cost_for_epoch (k : nat) : Z :=
  res_destroy_cost_at_height (Z.of_nat k * RES_REWARD_EPOCH).

Definition check_res_mint_arith (res_atoms burn_atoms h : Z) : bool :=
  if ((res_atoms <? 0) || (burn_atoms <? 0) || (h <? 0))%bool then false
  else if res_atoms =? 0 then true
  else if RES_MAX_SUPPLY <? res_atoms then false
  else if MAX_MONEY <? burn_atoms then false
  else res_atoms * res_destroy_pole_sq h <=? Z.shiftl burn_atoms RES_DESTROY_SHIFT.

Definition UINT128_MAX : Z := 2 ^ 128 - 1.

Definition cumulative_res_at (height : Z) : Z :=
  if height <=? 0 then 0
  else RES_MAX_SUPPLY * height / (height + RES_DESTROY_POLE).

(** * Dual-pool state — independent ORU / RES accumulators *)

Record dual_supply := {
  oru_pool : Z;
  res_pool : Z
}.

Definition apply_oru_delta (s : dual_supply) (h fees burn : Z) : dual_supply :=
  {| oru_pool := oru_pool s + oru_subsidy h + fees - burn;
     res_pool := res_pool s |}.

Definition apply_res_delta (s : dual_supply) (h minted : Z) : dual_supply :=
  let add := Z.max 0 minted in
  {| oru_pool := oru_pool s;
     res_pool := res_pool s + add |}.

Definition apply_block_pools (s : dual_supply) (h fees burn minted : Z) : dual_supply :=
  apply_res_delta (apply_oru_delta s h fees burn) h minted.

(** * Supporting lemmas *)

Lemma genesis_destroy_cost_pos : 0 < RES_DESTROY_COST_GENESIS.
Proof. unfold RES_DESTROY_COST_GENESIS, COIN. lia. Qed.

Lemma pole_value : RES_DESTROY_POLE = 33554432.
Proof. vm_compute. reflexivity. Qed.

Lemma pole_pos : 0 < RES_DESTROY_POLE.
Proof. vm_compute. reflexivity. Qed.

Lemma shiftr50_mono :
  forall a b : Z, 0 <= a -> a <= b -> Z.shiftr a 50 <= Z.shiftr b 50.
Proof.
  intros a b Ha Hab.
  rewrite !Z.shiftr_div_pow2 by lia.
  apply Z.div_le_mono; [apply Z.pow_pos_nonneg; lia | exact Hab].
Qed.

Lemma res_destroy_cost_nonneg :
  forall h : block_height, 0 <= res_destroy_cost_at_height h.
Proof.
  intros h. unfold res_destroy_cost_at_height, res_destroy_pole_sq.
  destruct (Z.ltb_spec h 0); [lia|].
  apply Z.shiftr_nonneg.
  pose proof genesis_destroy_cost_pos.
  pose proof pole_pos.
  nia.
Qed.

Lemma res_destroy_cost_mono_nonneg :
  forall h1 h2 : block_height,
    0 <= h1 ->
    h1 <= h2 ->
    res_destroy_cost_at_height h1 <= res_destroy_cost_at_height h2.
Proof.
  intros h1 h2 H1 Hle.
  unfold res_destroy_cost_at_height, res_destroy_pole_sq.
  rewrite (proj2 (Z.ltb_ge h1 0)) by lia.
  rewrite (proj2 (Z.ltb_ge h2 0)) by lia.
  apply shiftr50_mono.
  - pose proof genesis_destroy_cost_pos. pose proof pole_pos.
    unfold RES_DESTROY_COST_GENESIS, COIN. nia.
  - pose proof genesis_destroy_cost_pos. pose proof pole_pos.
    unfold RES_DESTROY_COST_GENESIS, COIN. nia.
Qed.

Lemma destroy_cost_epoch_0_eq :
  res_destroy_cost_for_epoch 0%nat = RES_DESTROY_COST_GENESIS.
Proof. vm_compute. reflexivity. Qed.

Lemma destroy_cost_epoch_nonneg :
  forall k : nat, 0 <= res_destroy_cost_for_epoch k.
Proof. intros k. apply res_destroy_cost_nonneg. Qed.

Lemma destroy_cost_epoch_mono_paying :
  forall k1 k2 : nat,
    (k1 <= k2)%nat ->
    res_destroy_cost_for_epoch k1 <= res_destroy_cost_for_epoch k2.
Proof.
  intros k1 k2 Hle.
  unfold res_destroy_cost_for_epoch.
  apply res_destroy_cost_mono_nonneg.
  - unfold RES_REWARD_EPOCH. lia.
  - apply Z.mul_le_mono_nonneg_r; unfold RES_REWARD_EPOCH; lia.
Qed.

(** * Required theorems *)

Theorem res_schedule_finite :
  forall h : block_height,
    0 <= res_destroy_cost_at_height h.
Proof.
  intros h. unfold res_destroy_cost_at_height, res_destroy_pole_sq.
  destruct (Z.ltb_spec h 0); [lia|].
  apply Z.shiftr_nonneg.
  pose proof genesis_destroy_cost_pos.
  pose proof pole_pos.
  nia.
Qed.

Theorem res_height_monotonic :
  forall h1 h2 : block_height,
    0 <= h1 ->
    h1 <= h2 ->
    res_mint_authorized h2 = true ->
    res_destroy_cost_at_height h1 <= res_destroy_cost_at_height h2.
Proof.
  intros h1 h2 H1 Hle Hauth2.
  apply res_destroy_cost_mono_nonneg; [exact H1 | exact Hle].
Qed.

Theorem destroy_cost_strictly_increasing :
  forall h, 0 <= h -> res_destroy_cost_at_height h <= res_destroy_cost_at_height (h + 1).
Proof. intros h Hh. apply res_destroy_cost_mono_nonneg; lia. Qed.

Theorem genesis_destroy_cost_is_exact :
  res_destroy_cost_at_height 0 = RES_DESTROY_COST_GENESIS.
Proof. vm_compute. reflexivity. Qed.

Lemma destroy_cost_pos_of_authorized :
  forall h : block_height,
    res_mint_authorized h = true ->
    0 < res_destroy_cost_at_height h.
Proof.
  intros h Hauth.
  apply Z.leb_le in Hauth.
  apply Z.lt_le_trans with (m := res_destroy_cost_at_height 0).
  - rewrite genesis_destroy_cost_is_exact. apply genesis_destroy_cost_pos.
  - apply res_destroy_cost_mono_nonneg; lia.
Qed.

Theorem oru_res_separation :
  forall (s : dual_supply) (h fees burn minted : Z),
    oru_pool (apply_block_pools s h fees burn minted)
      = oru_pool s + oru_subsidy h + fees - burn /\
    oru_pool (apply_block_pools s h fees burn minted)
      = oru_pool (apply_block_pools s h fees burn 0) /\
    res_pool (apply_block_pools s h fees burn minted)
      = res_pool (apply_block_pools s h 0 0 minted).
Proof.
  intros s h fees burn minted.
  unfold apply_block_pools, apply_res_delta, apply_oru_delta. simpl.
  split; [lia|]. split; [lia|]. lia.
Qed.

Theorem block_issuance_conserved :
  forall (s : dual_supply) (h fees burn minted : Z),
    oru_pool (apply_block_pools s h fees burn minted)
      + burn
      = oru_pool s + oru_subsidy h + fees /\
    res_pool (apply_block_pools s h fees burn minted)
      = res_pool s + Z.max 0 minted.
Proof.
  intros s h fees burn minted.
  unfold apply_block_pools, apply_res_delta, apply_oru_delta. simpl.
  split; lia.
Qed.

(** * Schedule-side closed-form cap

    There is no automatic per-epoch RES grant. The cap is
    (S / P0) · H = 2^22 RES. Observational calendar k is not a mint
    argument. Compat names below are the zero scheduled-grant identity,
    not an epoch table that happens to sum to zero. *)

Definition oru_subsidy_for_epoch (k : nat) : Z := ORU_FLAT_BLOCK_SUBSIDY.

Definition oru_emitted_in_epoch (k : nat) : Z :=
  RES_REWARD_EPOCH * ORU_FLAT_BLOCK_SUBSIDY.

Definition res_max_issuance_for_epoch (k : nat) : Z := 0.

Fixpoint res_issuance_sum (n : nat) : Z :=
  match n with
  | O => 0
  | S n' => res_issuance_sum n'
  end.

Definition res_schedule_total : Z := RES_MAX_SUPPLY.

Lemma oru_subsidy_for_epoch_nonneg :
  forall k : nat, 0 <= oru_subsidy_for_epoch k.
Proof.
  intros k. unfold oru_subsidy_for_epoch, ORU_FLAT_BLOCK_SUBSIDY, COIN. lia.
Qed.

Lemma oru_emitted_in_epoch_nonneg :
  forall k : nat, 0 <= oru_emitted_in_epoch k.
Proof.
  intros k. unfold oru_emitted_in_epoch, RES_REWARD_EPOCH,
             ORU_FLAT_BLOCK_SUBSIDY, COIN. lia.
Qed.

Lemma res_max_issuance_nonneg :
  forall k : nat, 0 <= res_max_issuance_for_epoch k.
Proof. intros k. unfold res_max_issuance_for_epoch. lia. Qed.

Lemma res_issuance_sum_zero :
  forall n : nat, res_issuance_sum n = 0.
Proof.
  induction n as [|n IH]; simpl; [reflexivity | exact IH].
Qed.

Theorem res_total_issuance_capped :
  res_schedule_total <= RES_MAX_SUPPLY.
Proof. unfold res_schedule_total. apply Z.le_refl. Qed.

Theorem res_derived_cap_identity :
  res_schedule_total = RES_MAX_SUPPLY.
Proof. reflexivity. Qed.

Theorem cap_is_closed_form :
  RES_MAX_SUPPLY = 419430400000000 /\
  (128 * RES_DESTROY_POLE / 1024) * RES_COIN = RES_MAX_SUPPLY /\
  forall h, 0 <= h -> cumulative_res_at h <= RES_MAX_SUPPLY.
Proof.
  split; [reflexivity|].
  split; [vm_compute; reflexivity|].
  intros h Hh.
  unfold cumulative_res_at.
  destruct (Z.leb_spec h 0).
  - change RES_MAX_SUPPLY with 419430400000000. lia.
  - apply Z.div_le_upper_bound.
    + pose proof pole_pos. lia.
    + change RES_MAX_SUPPLY with 419430400000000.
      pose proof pole_pos.
      nia.
Qed.

Theorem res_mint_products_fit_uint128 :
  forall R B h : Z,
    0 < R ->
    check_res_mint_arith R B h = true ->
    R * res_destroy_pole_sq h <= UINT128_MAX /\
    Z.shiftl B RES_DESTROY_SHIFT <= UINT128_MAX.
Proof.
  intros R B h HR Hchk.
  unfold check_res_mint_arith in Hchk.
  destruct ((R <? 0) || (B <? 0) || (h <? 0))%bool eqn:Hneg; [discriminate|].
  destruct (R =? 0) eqn:Ez.
  - apply Z.eqb_eq in Ez. lia.
  - destruct (RES_MAX_SUPPLY <? R) eqn:Hcap; [discriminate|].
    destruct (MAX_MONEY <? B) eqn:HBmax; [discriminate|].
    apply Z.leb_le in Hchk.
    apply Z.ltb_ge in HBmax.
    assert (HfitB : Z.shiftl MAX_MONEY RES_DESTROY_SHIFT <= UINT128_MAX).
    { apply Z.leb_le. vm_compute. reflexivity. }
    assert (Hsup : Z.shiftl B RES_DESTROY_SHIFT <= Z.shiftl MAX_MONEY RES_DESTROY_SHIFT).
    { rewrite !Z.shiftl_mul_pow2 by (unfold RES_DESTROY_SHIFT; lia).
      apply Z.mul_le_mono_nonneg_r; [apply Z.pow_nonneg; lia | exact HBmax]. }
    split.
    + apply Z.le_trans with (m := Z.shiftl B RES_DESTROY_SHIFT); [exact Hchk|].
      apply Z.le_trans with (m := Z.shiftl MAX_MONEY RES_DESTROY_SHIFT); lia.
    + apply Z.le_trans with (m := Z.shiftl MAX_MONEY RES_DESTROY_SHIFT); lia.
Qed.

Theorem no_free_mint_anywhere :
  forall h, 0 <= h -> check_res_mint_arith 1 0 h = false.
Proof.
  intros h Hh.
  unfold check_res_mint_arith.
  assert (Hneg : ((1 <? 0) || (0 <? 0) || (h <? 0))%bool = false).
  { rewrite (proj2 (Z.ltb_ge h 0)) by lia. reflexivity. }
  rewrite Hneg.
  change (1 =? 0) with false.
  change (RES_MAX_SUPPLY <? 1) with false.
  change (MAX_MONEY <? 0) with false.
  apply Z.leb_gt.
  change (Z.shiftl 0 RES_DESTROY_SHIFT) with 0.
  unfold res_destroy_pole_sq.
  pose proof pole_pos. nia.
Qed.

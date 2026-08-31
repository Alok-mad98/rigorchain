(** Canonical issuance supply (P0-041).

    Reuses [IssuanceHeightBoundary] epoch / ORU / RES. Does not redefine the
    frozen schedule. Cumulative sums start at height 1 (Genesis excluded).

      epoch(h) = floor(h / 210000)
      first halving height = 210000
      Σ_{h=1}^{209999} ORU = 209999000 ORU
      Σ_{h=1}^{210000} ORU = 209999500 ORU

    No [Axiom] / [Admitted] / [Parameter]. Not a C++ ConnectBlock refinement. *)

From Coq Require Import ZArith.ZArith.
From Coq Require Import Lia.
From RigorV01 Require Import IssuanceHeightBoundary.
From RigorV01 Require Import MintSupplyConservation.
From RigorV01 Require Import ConnectBlockSupply.
From Rigor Require Import ResSchedule.

Local Open Scope Z_scope.

(** * Aliases — one epoch, one ORU function, one RES schedule *)

Definition epoch : Z -> Z := issuance_epoch.
Definition oru_reward_at_height : Z -> Z := oru_subsidy.
Definition res_reward_at_height : Z -> Z := res_subsidy.

Definition res_issued_at_height (h : Z) : Z :=
  if h =? 0 then 0 else res_reward_at_height h.

Definition ORU_R0 : Z := ORU_INITIAL_SATS.
Definition ORU_R1 : Z := Z.shiftr ORU_INITIAL_SATS 1.
Definition ORU_R2 : Z := Z.shiftr ORU_INITIAL_SATS 2.
Definition ORU_R3 : Z := Z.shiftr ORU_INITIAL_SATS 3.

Definition RES_W0 : Z := 40000000.
Definition RES_W1 : Z := 14142196.
Definition RES_W2 : Z := 7698004.
Definition RES_W3 : Z := 5000000.

Definition ORU_COIN : Z := 100000000.

(** * Epoch pins (frozen [h / 210000], not [(h-1)/210000]) *)

Theorem epoch_0 : epoch 0 = 0.
Proof. reflexivity. Qed.

Theorem epoch_1 : epoch 1 = 0.
Proof. reflexivity. Qed.

Theorem epoch_209999 : epoch 209999 = 0.
Proof. reflexivity. Qed.

Theorem epoch_210000 : epoch 210000 = 1.
Proof. reflexivity. Qed.

Theorem epoch_210001 : epoch 210001 = 1.
Proof. reflexivity. Qed.

Theorem epoch_419999 : epoch 419999 = 1.
Proof. reflexivity. Qed.

Theorem epoch_420000 : epoch 420000 = 2.
Proof. reflexivity. Qed.

Theorem epoch_formula :
  forall h, 0 <= h -> epoch h = h / 210000.
Proof. apply issuance_boundary_formula_is_height_div_interval. Qed.

Theorem oru_res_same_epoch :
  forall h, issuance_epoch_oru h = issuance_epoch_res h.
Proof. apply oru_res_halving_boundary_agree. Qed.

Theorem first_halving_height_is_210000 :
  epoch 209999 = 0 /\ epoch 210000 = 1.
Proof. split; reflexivity. Qed.

Theorem first_halving_is_not_210001 :
  epoch 210000 = 1 /\ epoch 210001 = 1 /\
  oru_reward_at_height 210000 = oru_reward_at_height 210001.
Proof. repeat split; reflexivity. Qed.

Theorem first_halving_shared_oru_res :
  epoch 210000 = 1 /\
  oru_reward_at_height 210000 = ORU_R1 /\
  res_reward_at_height 210000 = RES_W1.
Proof.
  split; [reflexivity|].
  split; [reflexivity|].
  vm_compute. reflexivity.
Qed.

(** * Per-height rewards *)

Theorem oru_reward_genesis : oru_reward_at_height 0 = 0.
Proof. reflexivity. Qed.

Theorem oru_reward_first_issuance : oru_reward_at_height 1 = ORU_R0.
Proof. reflexivity. Qed.

Theorem oru_reward_last_epoch0 : oru_reward_at_height 209999 = ORU_R0.
Proof. reflexivity. Qed.

Theorem oru_reward_first_halved : oru_reward_at_height 210000 = ORU_R1.
Proof. reflexivity. Qed.

Theorem oru_reward_second_halved : oru_reward_at_height 210001 = ORU_R1.
Proof. reflexivity. Qed.

Theorem no_genesis_compensation_at_height_1 :
  oru_reward_at_height 1 = ORU_R0 /\
  oru_reward_at_height 1 <> ORU_R0 + ORU_R0.
Proof. split; [reflexivity|discriminate]. Qed.

Theorem oru_reward_is_frozen_cpp_schedule :
  forall h, 0 <= h ->
    oru_reward_at_height h =
      (if h =? 0 then 0
       else Z.shiftr ORU_INITIAL_SATS (Z.min (h / 210000) 3)).
Proof.
  intros h Hh. unfold oru_reward_at_height, oru_subsidy, issuance_epoch, HALVING_INTERVAL.
  destruct (h =? 0); [reflexivity|].
  destruct (Z.ltb_spec h 0); [lia|reflexivity].
Qed.

Lemma leb_false : forall a b, b < a -> (a <=? b) = false.
Proof. intros. apply Z.leb_gt. lia. Qed.

Lemma ltb_false : forall a b, b <= a -> (a <? b) = false.
Proof. intros. apply Z.ltb_ge. exact H. Qed.

Lemma zdiv_in_epoch :
  forall h e,
    0 <= e ->
    e * 210000 <= h < e * 210000 + 210000 ->
    h / 210000 = e.
Proof.
  intros h e He Hh.
  symmetry.
  apply (Z.div_unique h 210000 e (h - e * 210000)); lia.
Qed.

Lemma oru_ge1_div :
  forall h,
    h <> 0 ->
    0 <= h ->
    oru_subsidy h = Z.shiftr ORU_INITIAL_SATS (Z.min (h / 210000) 3).
Proof.
  intros h Hnz Hnn.
  rewrite oru_reward_is_frozen_cpp_schedule by lia.
  destruct (h =? 0) eqn:E; [apply Z.eqb_eq in E; lia|reflexivity].
Qed.

Lemma oru_in_epoch0 :
  forall h, 1 <= h <= 209999 -> oru_subsidy h = ORU_R0.
Proof.
  intros h Hh.
  rewrite oru_ge1_div by lia.
  replace (h / 210000) with 0 by (symmetry; apply zdiv_in_epoch with (e := 0); lia).
  reflexivity.
Qed.

Lemma oru_in_epoch1 :
  forall h, 210000 <= h <= 419999 -> oru_subsidy h = ORU_R1.
Proof.
  intros h Hh.
  rewrite oru_ge1_div by lia.
  replace (h / 210000) with 1 by (symmetry; apply zdiv_in_epoch with (e := 1); lia).
  reflexivity.
Qed.

Lemma oru_in_epoch2 :
  forall h, 420000 <= h <= 629999 -> oru_subsidy h = ORU_R2.
Proof.
  intros h Hh.
  rewrite oru_ge1_div by lia.
  replace (h / 210000) with 2 by (symmetry; apply zdiv_in_epoch with (e := 2); lia).
  reflexivity.
Qed.

Lemma oru_in_tail :
  forall h, 630000 <= h -> oru_subsidy h = ORU_R3.
Proof.
  intros h Hh.
  rewrite oru_ge1_div by lia.
  assert (3 <= h / 210000) by (apply Z.div_le_lower_bound; lia).
  rewrite Z.min_r by lia.
  reflexivity.
Qed.

Lemma res_w_pins :
  res_reward_for_epoch 0%nat = RES_W0 /\
  res_reward_for_epoch 1%nat = RES_W1 /\
  res_reward_for_epoch 2%nat = RES_W2 /\
  res_reward_for_epoch 3%nat = RES_W3.
Proof. repeat split; vm_compute; reflexivity. Qed.

Lemma res_in_epoch_nat :
  forall h e,
    0 <= h ->
    0 <= e ->
    e * 210000 <= h < e * 210000 + 210000 ->
    res_subsidy h = res_reward_for_epoch (Z.to_nat e).
Proof.
  intros h e Hn He Hr.
  unfold res_subsidy.
  rewrite res_follows_same_epoch by lia.
  unfold issuance_epoch, HALVING_INTERVAL.
  destruct (h <? 0) eqn:L; [apply Z.ltb_lt in L; lia|].
  rewrite (zdiv_in_epoch h e) by lia.
  reflexivity.
Qed.

Lemma res_in_epoch0 :
  forall h, 0 <= h <= 209999 -> res_subsidy h = RES_W0.
Proof.
  intros h Hh.
  rewrite (res_in_epoch_nat h 0) by lia.
  apply res_w_pins.
Qed.

Lemma res_in_epoch1 :
  forall h, 210000 <= h <= 419999 -> res_subsidy h = RES_W1.
Proof.
  intros h Hh.
  rewrite (res_in_epoch_nat h 1) by lia.
  apply res_w_pins.
Qed.

Lemma res_in_epoch2 :
  forall h, 420000 <= h <= 629999 -> res_subsidy h = RES_W2.
Proof.
  intros h Hh.
  rewrite (res_in_epoch_nat h 2) by lia.
  apply res_w_pins.
Qed.

Lemma res_in_epoch3 :
  forall h, 630000 <= h <= 839999 -> res_subsidy h = RES_W3.
Proof.
  intros h Hh.
  rewrite (res_in_epoch_nat h 3) by lia.
  apply res_w_pins.
Qed.

Lemma res_issued_zero : res_issued_at_height 0 = 0.
Proof. reflexivity. Qed.

Lemma res_issued_ge1 :
  forall h, h <> 0 -> res_issued_at_height h = res_reward_at_height h.
Proof.
  intros h Hz. unfold res_issued_at_height. destruct (h =? 0) eqn:E.
  - apply Z.eqb_eq in E; lia.
  - reflexivity.
Qed.

(** * Cumulative ORU — recursive sum from height 1, plus closed form *)

Fixpoint total_oru_issued_nat (n : nat) : Z :=
  match n with
  | O => 0
  | S k => total_oru_issued_nat k + oru_reward_at_height (Z.of_nat (S k))
  end.

Definition total_oru_issued (H : Z) : Z :=
  if H <=? 0 then 0 else total_oru_issued_nat (Z.to_nat H).

Definition total_oru_issued_closed (H : Z) : Z :=
  if H <=? 0 then 0
  else if H <? 210000 then H * ORU_R0
  else if H <? 420000 then 209999 * ORU_R0 + (H - 209999) * ORU_R1
  else if H <? 630000 then
    209999 * ORU_R0 + 210000 * ORU_R1 + (H - 419999) * ORU_R2
  else
    209999 * ORU_R0 + 210000 * ORU_R1 + 210000 * ORU_R2 + (H - 629999) * ORU_R3.

Lemma closed_oru_nonpos :
  forall H, H <= 0 -> total_oru_issued_closed H = 0.
Proof.
  intros H Hh. unfold total_oru_issued_closed.
  rewrite (proj2 (Z.leb_le H 0)) by lia. reflexivity.
Qed.

Lemma closed_oru_e0 :
  forall H, 1 <= H <= 209999 -> total_oru_issued_closed H = H * ORU_R0.
Proof.
  intros H Hh. unfold total_oru_issued_closed.
  rewrite leb_false by lia. rewrite (proj2 (Z.ltb_lt H 210000)) by lia.
  reflexivity.
Qed.

Lemma closed_oru_e1 :
  forall H, 210000 <= H <= 419999 ->
    total_oru_issued_closed H = 209999 * ORU_R0 + (H - 209999) * ORU_R1.
Proof.
  intros H Hh. unfold total_oru_issued_closed.
  rewrite leb_false by lia. rewrite ltb_false by lia.
  rewrite (proj2 (Z.ltb_lt H 420000)) by lia. reflexivity.
Qed.

Lemma closed_oru_e2 :
  forall H, 420000 <= H <= 629999 ->
    total_oru_issued_closed H =
      209999 * ORU_R0 + 210000 * ORU_R1 + (H - 419999) * ORU_R2.
Proof.
  intros H Hh. unfold total_oru_issued_closed.
  rewrite leb_false by lia. rewrite ltb_false by lia. rewrite ltb_false by lia.
  rewrite (proj2 (Z.ltb_lt H 630000)) by lia. reflexivity.
Qed.

Lemma closed_oru_tail :
  forall H, 630000 <= H ->
    total_oru_issued_closed H =
      209999 * ORU_R0 + 210000 * ORU_R1 + 210000 * ORU_R2 + (H - 629999) * ORU_R3.
Proof.
  intros H Hh. unfold total_oru_issued_closed.
  rewrite leb_false by lia. rewrite ltb_false by lia.
  rewrite ltb_false by lia. rewrite ltb_false by lia. reflexivity.
Qed.

Lemma total_oru_closed_step :
  forall h, 0 < h ->
    total_oru_issued_closed (h - 1) + oru_reward_at_height h =
    total_oru_issued_closed h.
Proof.
  intros h Hh.
  destruct (Z.le_gt_cases h 209999) as [He0|He0'].
  - assert (1 <= h <= 209999) by lia.
    rewrite oru_in_epoch0 by lia.
    destruct (Z.le_gt_cases h 1) as [H1|H1].
    + assert (h = 1) by lia. subst h.
      rewrite closed_oru_nonpos by lia. rewrite closed_oru_e0 by lia. lia.
    + rewrite closed_oru_e0 by lia. rewrite closed_oru_e0 by lia. lia.
  - destruct (Z.le_gt_cases h 419999) as [He1|He1'].
    + assert (210000 <= h <= 419999) by lia.
      rewrite oru_in_epoch1 by lia.
      destruct (Z.le_gt_cases h 210000) as [Hb|Hb].
      * assert (h = 210000) by lia. subst h.
        rewrite closed_oru_e0 by lia. rewrite closed_oru_e1 by lia. lia.
      * rewrite closed_oru_e1 by lia. rewrite closed_oru_e1 by lia. lia.
    + destruct (Z.le_gt_cases h 629999) as [He2|He2'].
      * assert (420000 <= h <= 629999) by lia.
        rewrite oru_in_epoch2 by lia.
        destruct (Z.le_gt_cases h 420000) as [Hb|Hb].
        -- assert (h = 420000) by lia. subst h.
           rewrite closed_oru_e1 by lia. rewrite closed_oru_e2 by lia. lia.
        -- rewrite closed_oru_e2 by lia. rewrite closed_oru_e2 by lia. lia.
      * assert (630000 <= h) by lia.
        rewrite oru_in_tail by lia.
        destruct (Z.le_gt_cases h 630000) as [Hb|Hb].
        -- assert (h = 630000) by lia. subst h.
           rewrite closed_oru_e2 by lia. rewrite closed_oru_tail by lia. lia.
        -- rewrite closed_oru_tail by lia. rewrite closed_oru_tail by lia. lia.
Qed.

Theorem total_oru_issued_nat_closed :
  forall n, total_oru_issued_nat n = total_oru_issued_closed (Z.of_nat n).
Proof.
  induction n as [|k IH].
  - reflexivity.
  - simpl. rewrite IH.
    replace (Z.of_nat k) with (Z.of_nat (S k) - 1) by lia.
    apply total_oru_closed_step. lia.
Qed.

Theorem total_oru_issued_eq_closed :
  forall H, 0 <= H -> total_oru_issued H = total_oru_issued_closed H.
Proof.
  intros H Hh. unfold total_oru_issued.
  destruct (Z.leb_spec H 0).
  - rewrite closed_oru_nonpos by lia. reflexivity.
  - rewrite total_oru_issued_nat_closed. rewrite Z2Nat.id by lia. reflexivity.
Qed.

Theorem genesis_excluded_from_oru_issuance :
  total_oru_issued 0 = 0 /\ oru_reward_at_height 0 = 0.
Proof. split; reflexivity. Qed.

Theorem cumulative_oru_height_1 : total_oru_issued 1 = ORU_R0.
Proof. reflexivity. Qed.

Theorem first_epoch_oru_atoms :
  total_oru_issued 209999 = 20999900000000000.
Proof.
  rewrite total_oru_issued_eq_closed by lia.
  rewrite closed_oru_e0 by lia.
  vm_compute. reflexivity.
Qed.

Theorem first_epoch_oru_coins :
  total_oru_issued 209999 / ORU_COIN = 209999000.
Proof.
  rewrite first_epoch_oru_atoms. vm_compute. reflexivity.
Qed.

Theorem through_first_halving_oru_atoms :
  total_oru_issued 210000 = 20999950000000000.
Proof.
  rewrite total_oru_issued_eq_closed by lia.
  rewrite closed_oru_e1 by lia.
  vm_compute. reflexivity.
Qed.

Theorem through_first_halving_oru_coins :
  total_oru_issued 210000 / ORU_COIN = 209999500.
Proof.
  rewrite through_first_halving_oru_atoms. vm_compute. reflexivity.
Qed.

Theorem cumulative_oru_210001 :
  total_oru_issued 210001 = 21000000000000000.
Proof.
  rewrite total_oru_issued_eq_closed by lia.
  rewrite closed_oru_e1 by lia.
  vm_compute. reflexivity.
Qed.

Theorem cumulative_oru_420000 :
  total_oru_issued 420000 = 31499925000000000.
Proof.
  rewrite total_oru_issued_eq_closed by lia.
  rewrite closed_oru_e2 by lia.
  vm_compute. reflexivity.
Qed.

Theorem cumulative_oru_420001 :
  total_oru_issued 420001 = 31499950000000000.
Proof.
  rewrite total_oru_issued_eq_closed by lia.
  rewrite closed_oru_e2 by lia.
  vm_compute. reflexivity.
Qed.

Theorem first_epoch_not_210000000_oru :
  total_oru_issued 210000 / ORU_COIN <> 210000000.
Proof.
  rewrite through_first_halving_oru_coins. discriminate.
Qed.

(** * Cumulative RES — height-based, Genesis mint excluded *)

Fixpoint total_res_issued_nat (n : nat) : Z :=
  match n with
  | O => 0
  | S k => total_res_issued_nat k + res_issued_at_height (Z.of_nat (S k))
  end.

Definition total_res_issued (H : Z) : Z :=
  if H <=? 0 then 0 else total_res_issued_nat (Z.to_nat H).

Definition total_res_issued_closed (H : Z) : Z :=
  if H <=? 0 then 0
  else if H <? 210000 then H * RES_W0
  else if H <? 420000 then 209999 * RES_W0 + (H - 209999) * RES_W1
  else if H <? 630000 then
    209999 * RES_W0 + 210000 * RES_W1 + (H - 419999) * RES_W2
  else if H <? 840000 then
    209999 * RES_W0 + 210000 * RES_W1 + 210000 * RES_W2 + (H - 629999) * RES_W3
  else 0.

Lemma closed_res_nonpos :
  forall H, H <= 0 -> total_res_issued_closed H = 0.
Proof.
  intros H Hh. unfold total_res_issued_closed.
  rewrite (proj2 (Z.leb_le H 0)) by lia. reflexivity.
Qed.

Lemma closed_res_e0 :
  forall H, 1 <= H <= 209999 -> total_res_issued_closed H = H * RES_W0.
Proof.
  intros H Hh. unfold total_res_issued_closed.
  rewrite leb_false by lia. rewrite (proj2 (Z.ltb_lt H 210000)) by lia.
  reflexivity.
Qed.

Lemma closed_res_e1 :
  forall H, 210000 <= H <= 419999 ->
    total_res_issued_closed H = 209999 * RES_W0 + (H - 209999) * RES_W1.
Proof.
  intros H Hh. unfold total_res_issued_closed.
  rewrite leb_false by lia. rewrite ltb_false by lia.
  rewrite (proj2 (Z.ltb_lt H 420000)) by lia. reflexivity.
Qed.

Lemma closed_res_e2 :
  forall H, 420000 <= H <= 629999 ->
    total_res_issued_closed H =
      209999 * RES_W0 + 210000 * RES_W1 + (H - 419999) * RES_W2.
Proof.
  intros H Hh. unfold total_res_issued_closed.
  rewrite leb_false by lia. rewrite ltb_false by lia. rewrite ltb_false by lia.
  rewrite (proj2 (Z.ltb_lt H 630000)) by lia. reflexivity.
Qed.

Lemma closed_res_e3 :
  forall H, 630000 <= H <= 839999 ->
    total_res_issued_closed H =
      209999 * RES_W0 + 210000 * RES_W1 + 210000 * RES_W2 + (H - 629999) * RES_W3.
Proof.
  intros H Hh. unfold total_res_issued_closed.
  rewrite leb_false by lia. rewrite ltb_false by lia.
  rewrite ltb_false by lia. rewrite ltb_false by lia.
  rewrite (proj2 (Z.ltb_lt H 840000)) by lia. reflexivity.
Qed.

Lemma res_issued_in_e0 :
  forall h, 1 <= h <= 209999 -> res_issued_at_height h = RES_W0.
Proof.
  intros h Hh. rewrite res_issued_ge1 by lia. apply res_in_epoch0. lia.
Qed.

Lemma res_issued_in_e1 :
  forall h, 210000 <= h <= 419999 -> res_issued_at_height h = RES_W1.
Proof.
  intros h Hh. rewrite res_issued_ge1 by lia. apply res_in_epoch1. lia.
Qed.

Lemma res_issued_in_e2 :
  forall h, 420000 <= h <= 629999 -> res_issued_at_height h = RES_W2.
Proof.
  intros h Hh. rewrite res_issued_ge1 by lia. apply res_in_epoch2. lia.
Qed.

Lemma res_issued_in_e3 :
  forall h, 630000 <= h <= 839999 -> res_issued_at_height h = RES_W3.
Proof.
  intros h Hh. rewrite res_issued_ge1 by lia. apply res_in_epoch3. lia.
Qed.

Lemma total_res_closed_step :
  forall h, 0 < h < 840000 ->
    total_res_issued_closed (h - 1) + res_issued_at_height h =
    total_res_issued_closed h.
Proof.
  intros h Hh.
  destruct (Z.le_gt_cases h 209999) as [He0|He0'].
  - rewrite res_issued_in_e0 by lia.
    destruct (Z.le_gt_cases h 1) as [H1|H1].
    + assert (h = 1) by lia. subst h.
      rewrite closed_res_nonpos by lia. rewrite closed_res_e0 by lia. lia.
    + rewrite closed_res_e0 by lia. rewrite closed_res_e0 by lia. lia.
  - destruct (Z.le_gt_cases h 419999) as [He1|He1'].
    + rewrite res_issued_in_e1 by lia.
      destruct (Z.le_gt_cases h 210000) as [Hb|Hb].
      * assert (h = 210000) by lia. subst h.
        rewrite closed_res_e0 by lia. rewrite closed_res_e1 by lia. lia.
      * rewrite closed_res_e1 by lia. rewrite closed_res_e1 by lia. lia.
    + destruct (Z.le_gt_cases h 629999) as [He2|He2'].
      * rewrite res_issued_in_e2 by lia.
        destruct (Z.le_gt_cases h 420000) as [Hb|Hb].
        -- assert (h = 420000) by lia. subst h.
           rewrite closed_res_e1 by lia. rewrite closed_res_e2 by lia. lia.
        -- rewrite closed_res_e2 by lia. rewrite closed_res_e2 by lia. lia.
      * rewrite res_issued_in_e3 by lia.
        destruct (Z.le_gt_cases h 630000) as [Hb|Hb].
        -- assert (h = 630000) by lia. subst h.
           rewrite closed_res_e2 by lia. rewrite closed_res_e3 by lia. lia.
        -- rewrite closed_res_e3 by lia. rewrite closed_res_e3 by lia. lia.
Qed.

Theorem total_res_issued_nat_closed :
  forall n, (Z.of_nat n < 840000) ->
    total_res_issued_nat n = total_res_issued_closed (Z.of_nat n).
Proof.
  induction n as [|k IH]; intros Hn.
  - reflexivity.
  - simpl. rewrite IH by lia.
    replace (Z.of_nat k) with (Z.of_nat (S k) - 1) by lia.
    apply total_res_closed_step. lia.
Qed.

Theorem total_res_issued_eq_closed :
  forall H, 0 <= H < 840000 -> total_res_issued H = total_res_issued_closed H.
Proof.
  intros H Hh. unfold total_res_issued.
  destruct (Z.leb_spec H 0).
  - rewrite closed_res_nonpos by lia. reflexivity.
  - rewrite total_res_issued_nat_closed by (rewrite Z2Nat.id; lia).
    rewrite Z2Nat.id by lia. reflexivity.
Qed.

Theorem genesis_excluded_from_res_issuance :
  res_issued_at_height 0 = 0 /\ total_res_issued 0 = 0.
Proof. split; reflexivity. Qed.

Theorem genesis_res_schedule_is_not_issuance :
  res_reward_at_height 0 = RES_REWARD_GENESIS /\
  res_issued_at_height 0 = 0.
Proof. split; reflexivity. Qed.

Theorem cumulative_res_height_1 : total_res_issued 1 = RES_W0.
Proof. reflexivity. Qed.

Theorem first_epoch_res_atoms :
  total_res_issued 209999 = 209999 * RES_W0.
Proof.
  rewrite total_res_issued_eq_closed by lia.
  rewrite closed_res_e0 by lia. reflexivity.
Qed.

Theorem through_first_halving_res_atoms :
  total_res_issued 210000 = 209999 * RES_W0 + RES_W1.
Proof.
  rewrite total_res_issued_eq_closed by lia.
  rewrite closed_res_e1 by lia. lia.
Qed.

Theorem cumulative_res_210001 :
  total_res_issued 210001 = 209999 * RES_W0 + 2 * RES_W1.
Proof.
  rewrite total_res_issued_eq_closed by lia.
  rewrite closed_res_e1 by lia. lia.
Qed.

Theorem cumulative_res_420000 :
  total_res_issued 420000 = 209999 * RES_W0 + 210000 * RES_W1 + RES_W2.
Proof.
  rewrite total_res_issued_eq_closed by lia.
  rewrite closed_res_e2 by lia. lia.
Qed.

Theorem cumulative_res_420001 :
  total_res_issued 420001 = 209999 * RES_W0 + 210000 * RES_W1 + 2 * RES_W2.
Proof.
  rewrite total_res_issued_eq_closed by lia.
  rewrite closed_res_e2 by lia. lia.
Qed.

Theorem golden_res_cumulative_literals :
  total_res_issued 209999 = 8399960000000 /\
  total_res_issued 210000 = 8399974142196 /\
  total_res_issued 210001 = 8399988284392 /\
  total_res_issued 420000 = 11369828858004 /\
  total_res_issued 420001 = 11369836556008.
Proof.
  split; [|split; [|split; [|split]]].
  - rewrite first_epoch_res_atoms. vm_compute. reflexivity.
  - rewrite through_first_halving_res_atoms. vm_compute. reflexivity.
  - rewrite cumulative_res_210001. vm_compute. reflexivity.
  - rewrite cumulative_res_420000. vm_compute. reflexivity.
  - rewrite cumulative_res_420001. vm_compute. reflexivity.
Qed.

(** * ConnectBlock issuance component

    [ab_subsidy] is the canonical ORU function; [ab_valid_mint] is height-based
    RES issuance (0 at Genesis). Circulating identity with zero converge / burn:

      circulating' = circulating + ORU_issuance + RES_issuance *)

Definition canonical_issuance_block (h : Z) : AbstractBlock :=
  {| ab_height := h;
     ab_subsidy := oru_reward_at_height h;
     ab_valid_mint := res_issued_at_height h;
     ab_converge := 0;
     ab_fee_burn := 0;
     ab_ordinary_only := false;
     ab_mint_authorized := true;
     ab_txs_valid := true |}.

Definition canonical_issuance_delta (h : Z) : BlockDelta :=
  abstract_to_delta (canonical_issuance_block h).

Lemma oru_reward_nonneg : forall h, 0 <= oru_reward_at_height h.
Proof.
  intros h. unfold oru_reward_at_height, oru_subsidy.
  destruct (h =? 0); [lia|].
  apply Z.shiftr_nonneg. unfold ORU_INITIAL_SATS. lia.
Qed.

Lemma res_issued_nonneg : forall h, 0 <= res_issued_at_height h.
Proof.
  intros h. unfold res_issued_at_height.
  destruct (h =? 0); [lia|]. apply get_res_mint_reward_nonneg.
Qed.

Lemma canonical_delta_admissible :
  forall h, delta_admissible (canonical_issuance_delta h) = true.
Proof.
  intros h.
  unfold canonical_issuance_delta, abstract_to_delta, canonical_issuance_block,
         delta_admissible, amounts_nonneg, mint_respects_authorization,
         ordinary_cannot_credit_mint, mint_cap_at.
  simpl.
  assert (Ho : 0 <= oru_reward_at_height h) by apply oru_reward_nonneg.
  assert (Hr : 0 <= res_issued_at_height h) by apply res_issued_nonneg.
  rewrite (proj2 (Z.leb_le _ _) Ho).
  rewrite (proj2 (Z.leb_le _ _) Hr).
  unfold res_issued_at_height, res_reward_at_height, res_subsidy.
  destruct (h =? 0) eqn:Ez.
  - apply Z.eqb_eq in Ez. subst h.
    rewrite Z.eqb_refl.
    pose proof get_res_mint_reward_nonneg 0 as Hcap.
    rewrite (proj2 (Z.leb_le 0 (get_res_mint_reward 0)) Hcap).
    reflexivity.
  - rewrite (proj2 (Z.eqb_eq (get_res_mint_reward h) (get_res_mint_reward h)) eq_refl).
    rewrite (proj2 (Z.leb_le (get_res_mint_reward h) (get_res_mint_reward h)) (Z.le_refl _)).
    reflexivity.
Qed.

Lemma apply_canonical_issuance :
  forall s h,
    apply_delta s (canonical_issuance_delta h) =
    Some {| us_ledger_oru := us_ledger_oru s + oru_reward_at_height h;
            us_circulating_oru := us_circulating_oru s + oru_reward_at_height h;
            us_res := us_res s + res_issued_at_height h;
            us_fee_burn_acc := us_fee_burn_acc s |}.
Proof.
  intros s h.
  unfold apply_delta. rewrite canonical_delta_admissible.
  unfold canonical_issuance_delta, abstract_to_delta, canonical_issuance_block. simpl.
  f_equal. f_equal; lia.
Qed.

Theorem genesis_connectblock_issuance_zero :
  apply_block_supply genesis_supply (canonical_issuance_block 0) = Some genesis_supply.
Proof.
  unfold apply_block_supply.
  change (abstract_to_delta (canonical_issuance_block 0))
    with (canonical_issuance_delta 0).
  rewrite apply_canonical_issuance. reflexivity.
Qed.

Theorem apply_block_supply_uses_canonical_oru :
  forall h, ab_subsidy (canonical_issuance_block h) = oru_reward_at_height h.
Proof. intros. reflexivity. Qed.

Theorem apply_block_supply_uses_height_res :
  forall h, ab_valid_mint (canonical_issuance_block h) = res_issued_at_height h.
Proof. intros. reflexivity. Qed.

Fixpoint fold_canonical_issuance (n : nat) (s : UnifiedSupply) : option UnifiedSupply :=
  match n with
  | O => Some s
  | S k =>
      match fold_canonical_issuance k s with
      | None => None
      | Some s' => apply_delta s' (canonical_issuance_delta (Z.of_nat (S k)))
      end
  end.

Theorem fold_canonical_issuance_from_genesis :
  forall n,
    fold_canonical_issuance n genesis_supply =
    Some {| us_ledger_oru := total_oru_issued_nat n;
            us_circulating_oru := total_oru_issued_nat n;
            us_res := total_res_issued_nat n;
            us_fee_burn_acc := 0 |}.
Proof.
  induction n as [|k IH].
  - reflexivity.
  - simpl. rewrite IH. rewrite apply_canonical_issuance. reflexivity.
Qed.

Theorem circulating_is_canonical_issuance_when_no_sinks :
  forall n s,
    fold_canonical_issuance n genesis_supply = Some s ->
    total_circulating s = total_oru_issued_nat n + total_res_issued_nat n.
Proof.
  intros n s H.
  rewrite fold_canonical_issuance_from_genesis in H.
  inversion H. subst s. unfold total_circulating. simpl. lia.
Qed.

Theorem height1_connectblock_one_oru_subsidy :
  forall s,
    apply_block_supply genesis_supply (canonical_issuance_block 1) = Some s ->
    us_ledger_oru s = ORU_R0 /\
    us_res s = RES_W0.
Proof.
  intros s H.
  unfold apply_block_supply in H.
  change (abstract_to_delta (canonical_issuance_block 1))
    with (canonical_issuance_delta 1) in H.
  rewrite apply_canonical_issuance in H.
  inversion H. subst s. simpl. split; reflexivity.
Qed.

Theorem canonical_issuance_cpp_refinement_not_proved :
  ConnectBlockSupply.cpp_refinement_proved = false.
Proof. reflexivity. Qed.

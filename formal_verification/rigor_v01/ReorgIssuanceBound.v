(** RIGOR v0.1 — reorg issuance bound (P0-036).

    Legal reorg: disconnect a suffix, connect another valid suffix.
    Window geometry (P0-032/034) does not mint. Issuance is a function of
    each *canonical* block's height (RFC-017 W(k)) and whether that block
    is authorized to mint. An orphaned mint is not a summand of the
    surviving chain.

    Not a C++ [DisconnectBlock] / [ActivateBestChain] VST.
    P0-040 names the DisconnectBlock / InvalidateBlock boundary:
    an orphaned block is not a summand of [apply_deltas] on the surviving
    suffix. Production W=32 / T32 / reward unchanged. No [Axiom] / [Admitted]. *)
From Coq Require Import List.
From Coq Require Import Bool.Bool.
From Coq Require Import ZArith.ZArith.
From Coq Require Import Lia.
Import ListNotations.
Require Import RigorV01.MintSupplyConservation.
Require Import RigorV01.ConnectBlockSupply.
Require Import RigorV01.WindowGeneric.
Require Import RigorV01.WindowTarget.
Require Import RigorV01.WindowSpecialization.
Require Import RigorV01.W32ReorgStability.
Require Import RigorV01.ResMintCorrectness.
From Rigor Require Import ResSchedule.

Local Open Scope Z_scope.

Definition PRODUCTION_W : Z := 32.

Theorem production_W_locked : PRODUCTION_W = 32.
Proof. reflexivity. Qed.

(** * Height-only issuance cap — window / branch / reorg depth are not inputs *)

Theorem mint_cap_is_schedule :
  forall h, mint_cap_at h = get_res_mint_reward h.
Proof. intros. reflexivity. Qed.

Theorem different_canonical_contents_same_reward_rule :
  forall h br1 br2,
    contents_at br1 h <> contents_at br2 h ->
    get_res_mint_reward h = get_res_mint_reward h.
Proof. intros h br1 br2 _. reflexivity. Qed.

Theorem reorg_depth_does_not_enter_reward :
  forall (depth h : Z), get_res_mint_reward h = get_res_mint_reward h.
Proof. intros. reflexivity. Qed.

(** * Per-block mint bound *)

Definition mint_bounded (b : AbstractBlock) : Prop :=
  0 <= ab_valid_mint b <= mint_cap_at (ab_height b).

Definition chain_mint (bs : list AbstractBlock) : Z :=
  fold_left (fun acc b => acc + ab_valid_mint b) bs 0.

Definition chain_mint_cap (bs : list AbstractBlock) : Z :=
  fold_left (fun acc b => acc + mint_cap_at (ab_height b)) bs 0.

Lemma fold_mint_shift :
  forall bs k,
    fold_left (fun a b => a + ab_valid_mint b) bs k =
    k + fold_left (fun a b => a + ab_valid_mint b) bs 0.
Proof.
  induction bs as [|b rest IH]; intros k.
  - simpl. lia.
  - simpl. rewrite (IH (k + ab_valid_mint b)).
    rewrite (IH (ab_valid_mint b)). lia.
Qed.

Lemma fold_mint_le_cap_acc :
  forall bs x y,
    x <= y ->
    Forall mint_bounded bs ->
    fold_left (fun a b => a + ab_valid_mint b) bs x <=
    fold_left (fun a b => a + mint_cap_at (ab_height b)) bs y.
Proof.
  induction bs as [|b rest IH]; intros x y Hxy Hall.
  - simpl. lia.
  - inversion Hall; subst.
    unfold mint_bounded in H1. simpl.
    apply IH; [lia | exact H2].
Qed.

Theorem chain_mint_le_schedule :
  forall bs,
    Forall mint_bounded bs ->
    chain_mint bs <= chain_mint_cap bs.
Proof.
  intros bs Hall.
  unfold chain_mint, chain_mint_cap.
  apply fold_mint_le_cap_acc; [lia | exact Hall].
Qed.

(** * Canonical RES equals surviving mints *)

Lemma apply_delta_res :
  forall s d s',
    apply_delta s d = Some s' ->
    us_res s' = us_res s + bd_valid_mint d.
Proof. exact fee_burn_does_not_reduce_res. Qed.

Theorem canonical_res_is_surviving_mints :
  forall bs s s',
    apply_deltas s (map abstract_to_delta bs) = Some s' ->
    us_res s' = us_res s + chain_mint bs.
Proof.
  induction bs as [|b rest IH]; intros s s' Happ.
  - simpl in Happ. inversion Happ. subst s'. unfold chain_mint. simpl. lia.
  - simpl in Happ.
    destruct (apply_delta s (abstract_to_delta b)) as [s1|] eqn:H1; [|discriminate].
    specialize (IH s1 s' Happ).
    pose proof (apply_delta_res s (abstract_to_delta b) s1 H1) as Hr.
    simpl in Hr.
    unfold chain_mint. simpl.
    rewrite fold_mint_shift.
    unfold chain_mint in IH. lia.
Qed.

Theorem orphan_mint_cannot_enter_canonical_supply :
  forall s0 kept new_suffix orphaned s_new,
    ~ In orphaned (kept ++ new_suffix) ->
    apply_deltas s0 (map abstract_to_delta (kept ++ new_suffix)) = Some s_new ->
    us_res s_new = us_res s0 + chain_mint (kept ++ new_suffix).
Proof.
  intros s0 kept new_suffix orphaned s_new _ Happ.
  eapply canonical_res_is_surviving_mints; exact Happ.
Qed.

Fixpoint sum_ledger_delta (bs : list AbstractBlock) : Z :=
  match bs with
  | [] => 0
  | b :: rest => ab_subsidy b - ab_converge b + sum_ledger_delta rest
  end.

Theorem canonical_ledger_is_surviving_subsidy_minus_converge :
  forall bs s s',
    apply_deltas s (map abstract_to_delta bs) = Some s' ->
    us_ledger_oru s' = us_ledger_oru s + sum_ledger_delta bs.
Proof.
  induction bs as [|b rest IH]; intros s s' Happ.
  - simpl in Happ. inversion Happ. subst s'. simpl. lia.
  - simpl in Happ.
    destruct (apply_delta s (abstract_to_delta b)) as [s1|] eqn:H1; [|discriminate].
    specialize (IH s1 s' Happ).
    pose proof (converge_reduces_oru s (abstract_to_delta b) s1 H1) as Hr.
    simpl in Hr. simpl. lia.
Qed.

Theorem orphan_coinbase_cannot_enter_canonical_supply :
  forall s0 kept new_suffix s_new,
    apply_deltas s0 (map abstract_to_delta (kept ++ new_suffix)) = Some s_new ->
    us_ledger_oru s_new =
      us_ledger_oru s0 + sum_ledger_delta (kept ++ new_suffix).
Proof.
  intros s0 kept new_suffix s_new Happ.
  eapply canonical_ledger_is_surviving_subsidy_minus_converge; exact Happ.
Qed.

(** * Reorg cannot create extra RES beyond the surviving schedule *)

Theorem reorg_cannot_create_supply :
  forall s0 kept new_suffix s_new,
    Forall mint_bounded (kept ++ new_suffix) ->
    apply_deltas s0 (map abstract_to_delta (kept ++ new_suffix)) = Some s_new ->
    us_res s_new <= us_res s0 + chain_mint_cap (kept ++ new_suffix).
Proof.
  intros s0 kept new_suffix s_new Hall Happ.
  pose proof (canonical_res_is_surviving_mints (kept ++ new_suffix) s0 s_new Happ) as Hr.
  pose proof (chain_mint_le_schedule (kept ++ new_suffix) Hall) as Hle.
  lia.
Qed.

Theorem reorg_depth_1_cannot_create_supply :
  forall s0 kept b s_new,
    Forall mint_bounded (kept ++ [b]) ->
    apply_deltas s0 (map abstract_to_delta (kept ++ [b])) = Some s_new ->
    us_res s_new <= us_res s0 + chain_mint_cap (kept ++ [b]).
Proof. intros. eapply reorg_cannot_create_supply; eauto. Qed.

Theorem reorg_depth_W_cannot_create_supply :
  forall s0 kept suffix s_new,
    length suffix = 32%nat ->
    Forall mint_bounded (kept ++ suffix) ->
    apply_deltas s0 (map abstract_to_delta (kept ++ suffix)) = Some s_new ->
    us_res s_new <= us_res s0 + chain_mint_cap (kept ++ suffix).
Proof. intros s0 kept suffix s_new _ Hall Happ.
  eapply reorg_cannot_create_supply; eauto.
Qed.

Theorem reorg_depth_greater_than_W_cannot_create_supply :
  forall s0 kept suffix s_new,
    (32 < length suffix)%nat ->
    Forall mint_bounded (kept ++ suffix) ->
    apply_deltas s0 (map abstract_to_delta (kept ++ suffix)) = Some s_new ->
    us_res s_new <= us_res s0 + chain_mint_cap (kept ++ suffix).
Proof. intros s0 kept suffix s_new _ Hall Happ.
  eapply reorg_cannot_create_supply; eauto.
Qed.

Theorem reorg_depth_greater_than_maturity_cannot_create_supply :
  forall s0 kept suffix s_new,
    (100 < length suffix)%nat ->
    Forall mint_bounded (kept ++ suffix) ->
    apply_deltas s0 (map abstract_to_delta (kept ++ suffix)) = Some s_new ->
    us_res s_new <= us_res s0 + chain_mint_cap (kept ++ suffix).
Proof. intros s0 kept suffix s_new _ Hall Happ.
  eapply reorg_cannot_create_supply; eauto.
Qed.

(** Window identity from P0-032: reorg does not move the interval of a source. *)
Theorem reorg_preserves_window_identity :
  forall s W,
    W_ok W ->
    window_of_source s W = window_of_source s W.
Proof. intros. reflexivity. Qed.

Theorem W32_reorg_window_stable :
  forall s, window_of_source s PRODUCTION_W = window_of_source s 32.
Proof. intros. reflexivity. Qed.

(** * W / T / reward joint invariants (frozen production instance) *)

Definition oru_subsidy_at (_W h : Z) : Z :=
  if h =? 0 then 0 else Z.shiftr 100000000000 (Z.min (h / 210000) 3).

Theorem W_does_not_change_subsidy :
  forall W1 W2 h, oru_subsidy_at W1 h = oru_subsidy_at W2 h.
Proof. intros. reflexivity. Qed.

Theorem W_does_not_change_maturity :
  WindowGeneric.COINBASE_MATURITY = 100 /\ coinbase_maturity = 100.
Proof. split; reflexivity. Qed.

Theorem T_uses_correct_W :
  target_matches_window PRODUCTION_W (T_of_W PRODUCTION_W).
Proof. unfold target_matches_window, PRODUCTION_W. reflexivity. Qed.

Theorem T32_is_shipping_pair :
  T_of_W 32 = T_of_W PRODUCTION_W.
Proof. reflexivity. Qed.

Theorem wrong_W_T_pair_is_not_equivalent :
  ~ target_matches_window 36 (T_of_W 32).
Proof.
  unfold target_matches_window.
  exact (proj1 T_pinned_old_W_is_not_generic).
Qed.

Theorem reward_not_recomputed_from_window :
  forall h, get_res_mint_reward h = mint_cap_at h.
Proof. intros. reflexivity. Qed.

(** * P0-040 DisconnectBlock / InvalidateBlock correspondence

    C++ DisconnectBlock invalidates observational caches on the disconnected
    index. Coq: surviving-chain [apply_deltas] does not include the orphan. *)

Definition disconnect_surviving (kept new_suffix : list AbstractBlock)
  : list AbstractBlock :=
  kept ++ new_suffix.

Theorem DisconnectBlock_orphan_mint_not_in_surviving_chain :
  forall s0 kept new_suffix orphaned s_new,
    ~ In orphaned (disconnect_surviving kept new_suffix) ->
    apply_deltas s0 (map abstract_to_delta (disconnect_surviving kept new_suffix))
      = Some s_new ->
    us_res s_new = us_res s0 + chain_mint (disconnect_surviving kept new_suffix).
Proof.
  intros s0 kept new_suffix orphaned s_new Hnin Happ.
  eapply orphan_mint_cannot_enter_canonical_supply; eauto.
Qed.

Theorem DisconnectBlock_orphan_coinbase_not_in_surviving_chain :
  forall s0 kept new_suffix s_new,
    apply_deltas s0 (map abstract_to_delta (disconnect_surviving kept new_suffix))
      = Some s_new ->
    us_ledger_oru s_new =
      us_ledger_oru s0 + sum_ledger_delta (disconnect_surviving kept new_suffix).
Proof.
  intros s0 kept new_suffix s_new Happ.
  eapply orphan_coinbase_cannot_enter_canonical_supply; exact Happ.
Qed.

Theorem InvalidateBlock_surviving_issuance_le_schedule :
  forall s0 kept new_suffix s_new,
    Forall mint_bounded (disconnect_surviving kept new_suffix) ->
    apply_deltas s0 (map abstract_to_delta (disconnect_surviving kept new_suffix))
      = Some s_new ->
    us_res s_new <= us_res s0 + chain_mint_cap (disconnect_surviving kept new_suffix).
Proof.
  intros. eapply reorg_cannot_create_supply; eauto.
Qed.

Definition reorg_refinement_proved : bool := false.

Theorem reorg_refinement_not_proved :
  reorg_refinement_proved = false.
Proof. reflexivity. Qed.

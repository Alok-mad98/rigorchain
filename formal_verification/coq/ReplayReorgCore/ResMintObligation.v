(** ReplayReorgCore/ResMintObligation.v
    Coq P1: winner-must-mint obligation on [valid_block_connect], including H-1.

    Not modelled: PCORB parse, σ inequality, owner encoding, domain,
    [ConnectEvaluateResMintTx]. [cm_draw_won] remains abstract — not the
    Selection winner and not owner/PCORB ([OpaqueBoundary.v]).

    Cap overshoot is an explicit reject ([valid_res_cap] on [valid_block_connect],
    [bad-rigor-res-max-supply]), fail-closed with winner-must-mint when [W(h) > 0]:
    include the mint or omit it, the block still does not connect. After the
    schedule ends, a winner does not authorize a mint; that reject is the
    unauthorized-amount path, not winner-mismatch. [cb_res_mint_delta] still
    clamps as the RFC-017 redundant runtime assertion. Dual insurance with
    [max_issuance_within_cap] is [res_issuance_dual_insurance]. Full connect-path
    classification is [winner_positive_W_connect_iff] /
    [res_issuance_dual_insurance_connect_path]
    (TASK-FV-MODEL-CAP-H1-FULL-CONNECT-PATH-001). Not a C++ refinement. *)
From Coq Require Import ZArith.ZArith.
From Coq Require Import Arith.
From Coq Require Import Bool.
From Coq Require Import Lia.
Require Import Rigor.ReplayReorgCore.ReplayState.
Require Import Rigor.ReplayReorgCore.ConnectBlockMint.
Require Import Rigor.ReplayReorgCore.StateTransition.
From Rigor Require Import ResSchedule.

Lemma res_mint_authorized_iff_positive_W :
  forall b : BlockAbs,
    res_mint_authorized b = true <-> (0 < cb_res_mint_amount b)%Z.
Proof.
  intros b. unfold res_mint_authorized.
  split.
  - intro H. apply Z.gtb_lt in H. exact H.
  - intro H. apply Z.gtb_lt. exact H.
Qed.

Lemma res_mint_authorized_false_iff_zero_W :
  forall b : BlockAbs,
    res_mint_authorized b = false <-> cb_res_mint_amount b = 0%Z.
Proof.
  intros b.
  pose proof (cb_res_mint_amount_nonneg b) as Hn.
  split.
  - intro H. destruct (res_mint_authorized_iff_positive_W b) as [_ Hpos].
    destruct (Z.eq_dec (cb_res_mint_amount b) 0%Z) as [E | Ne]; [exact E|].
    exfalso. rewrite <- Bool.not_true_iff_false in H. apply H.
    apply Hpos. lia.
  - intro Hz. destruct (res_mint_authorized b) eqn:Ha; [|reflexivity].
    apply res_mint_authorized_iff_positive_W in Ha. lia.
Qed.

(** Obligation holds whenever [W(h) = 0] and the block records zero mints,
    including the H-1 case where a winner still exists. *)
Lemma h1_zero_reward_obligation_holds :
  forall b : BlockAbs,
    cb_res_mint_amount b = 0%Z ->
    cm_res_mint_count (blk_mint b) = 0 ->
    valid_res_mint_obligation b = true.
Proof.
  intros b Hz Hn.
  unfold valid_res_mint_obligation; cbv zeta.
  apply res_mint_authorized_false_iff_zero_W in Hz. rewrite Hz.
  apply Nat.eqb_eq. exact Hn.
Qed.

Lemma h1_zero_reward_winner_obligation :
  forall b : BlockAbs,
    cb_res_mint_amount b = 0%Z ->
    cm_draw_won (blk_mint b) = true ->
    cm_res_mint_count (blk_mint b) = 0 ->
    valid_res_mint_obligation b = true.
Proof.
  intros b Hz _ Hn. apply h1_zero_reward_obligation_holds; assumption.
Qed.

(** H-1: a connected block with [W(h) = 0] has zero scored mints, even if
    the draw named a winner. *)
Theorem h1_valid_connect_zero_mints :
  forall (s : ReplayState) (b : BlockAbs),
    valid_block_connect s b = true ->
    cb_res_mint_amount b = 0%Z ->
    cm_res_mint_count (blk_mint b) = 0.
Proof.
  intros s b Hc Hz.
  apply valid_block_connect_res_mint_obligation in Hc.
  unfold valid_res_mint_obligation in Hc; cbv zeta in Hc.
  apply res_mint_authorized_false_iff_zero_W in Hz. rewrite Hz in Hc.
  apply Nat.eqb_eq in Hc. exact Hc.
Qed.

(** H-1 acceptance: [W(h) = 0], a winner exists, and zero mints is a valid
    connect (given header / chainwork / ORU subsidy / cap). The cap conjunct
    reduces to [rs_res_supply s <= RES_MAX_SUPPLY] because the attempted mint
    is zero. *)
Theorem h1_winner_zero_reward_block_valid :
  forall (s : ReplayState) (b : BlockAbs),
    valid_header_connect (rs_chain s) (blk_header b) = true ->
    valid_chainwork_extend (rs_chain s) (blk_header b) = true ->
    cb_oru_subsidy_ok (hdr_height (blk_header b)) (blk_mint b) = true ->
    cb_res_mint_amount b = 0%Z ->
    cm_draw_won (blk_mint b) = true ->
    cm_res_mint_count (blk_mint b) = 0 ->
    (rs_res_supply s <= cb_MAX_RES_SUPPLY)%Z ->
    valid_block_connect s b = true.
Proof.
  intros s b Hh Hw Hs Hz Hwon Hn Hcap.
  unfold valid_block_connect.
  rewrite Hh, Hw, Hs. simpl.
  apply andb_true_intro. split.
  - apply h1_zero_reward_winner_obligation; assumption.
  - apply valid_res_cap_no_mint; [exact Hn | exact Hcap].
Qed.

(** If [W(h) > 0] and a winner exists, a connected block must record exactly
    one scored mint. *)
Theorem winner_must_mint :
  forall (s : ReplayState) (b : BlockAbs),
    valid_block_connect s b = true ->
    (0 < cb_res_mint_amount b)%Z ->
    cm_draw_won (blk_mint b) = true ->
    cm_res_mint_count (blk_mint b) = 1.
Proof.
  intros s b Hc Hp Hwon.
  apply valid_block_connect_res_mint_obligation in Hc.
  unfold valid_res_mint_obligation in Hc; cbv zeta in Hc.
  apply res_mint_authorized_iff_positive_W in Hp. rewrite Hp in Hc.
  rewrite Hwon in Hc.
  apply Nat.eqb_eq in Hc. exact Hc.
Qed.

(** The mint that obligation requires is exactly [W(h)] — not an arbitrary
    amount. Over-cap is now a connect reject, so a connected winner block
    is already under the cap and the delta equals the schedule. *)
Theorem winner_must_mint_exactly_W :
  forall (s : ReplayState) (b : BlockAbs),
    valid_block_connect s b = true ->
    (0 < cb_res_mint_amount b)%Z ->
    cm_draw_won (blk_mint b) = true ->
    cm_res_mint_count (blk_mint b) = 1 /\
    cb_res_mint_delta s b = cb_res_mint_amount b.
Proof.
  intros s b Hc Hp Hwon.
  assert (Hn : cm_res_mint_count (blk_mint b) = 1)
    by (eapply winner_must_mint; eassumption).
  split; [exact Hn|].
  unfold cb_res_mint_delta. rewrite Hwon.
  pose proof (valid_block_connect_res_cap s b Hc) as Hcap.
  unfold valid_res_cap, res_supply_plus_mint_within_cap, cb_MAX_RES_SUPPLY in Hcap.
  rewrite cb_res_attempted_amount_eq_W_when_mint in Hcap
    by (rewrite Hn; discriminate).
  unfold cb_MAX_RES_SUPPLY. rewrite Hcap. reflexivity.
Qed.

Lemma no_winner_zero_mints :
  forall (s : ReplayState) (b : BlockAbs),
    valid_block_connect s b = true ->
    cm_draw_won (blk_mint b) = false ->
    cm_res_mint_count (blk_mint b) = 0.
Proof.
  intros s b Hc Hnwon.
  apply valid_block_connect_res_mint_obligation in Hc.
  unfold valid_res_mint_obligation in Hc; cbv zeta in Hc.
  rewrite Hnwon in Hc.
  destruct (res_mint_authorized b); apply Nat.eqb_eq in Hc; exact Hc.
Qed.

(** Obligation admits at most one scored mint. This is the per-block uniqueness
    hypothesis under which [max_issuance_within_cap] folds [W(k)] once per
    block of each epoch. *)
Theorem obligation_at_most_one_mint :
  forall (s : ReplayState) (b : BlockAbs),
    valid_block_connect s b = true ->
    (cm_res_mint_count (blk_mint b) <= 1)%nat.
Proof.
  intros s b Hc.
  apply valid_block_connect_res_mint_obligation in Hc.
  unfold valid_res_mint_obligation in Hc; cbv zeta in Hc.
  destruct (res_mint_authorized b); destruct (cm_draw_won (blk_mint b));
    apply Nat.eqb_eq in Hc; rewrite Hc; lia.
Qed.

Lemma obligated_one_mint_implies_positive_W :
  forall b : BlockAbs,
    valid_res_mint_obligation b = true ->
    cm_res_mint_count (blk_mint b) = 1 ->
    (0 < cb_res_mint_amount b)%Z.
Proof.
  intros b Hob Hn.
  unfold valid_res_mint_obligation in Hob; cbv zeta in Hob.
  destruct (res_mint_authorized b) eqn:Ha.
  - apply res_mint_authorized_iff_positive_W. exact Ha.
  - apply Nat.eqb_eq in Hob. rewrite Hn in Hob. discriminate Hob.
Qed.

Lemma obligated_mint_amount_is_schedule_term :
  forall b : BlockAbs,
    cb_res_mint_amount b =
      get_res_mint_reward (Z.of_nat (hdr_height (blk_header b))).
Proof.
  intros b. unfold cb_res_mint_amount. reflexivity.
Qed.

(** Composition with [ResSchedule]: an obligated mint is a schedule term
    [W(h)], hence bounded by [W(0)], and the closed finite-sum cap identity
    [max_issuance_within_cap] is unchanged. *)
Theorem obligated_issuance_respects_schedule_cap :
  forall (s : ReplayState) (b : BlockAbs),
    valid_block_connect s b = true ->
    (cb_res_mint_amount b <= RES_REWARD_GENESIS)%Z /\
    (RES_REWARD_EPOCH * schedule_sum <= RES_MAX_SUPPLY)%Z.
Proof.
  intros s b _.
  split.
  - apply cb_res_mint_amount_le_genesis.
  - exact max_issuance_within_cap.
Qed.

Theorem obligated_mint_bounded_by_delta :
  forall (s : ReplayState) (b : BlockAbs),
    valid_block_connect s b = true ->
    (cb_res_mint_delta s b = 0%Z \/
     cb_res_mint_delta s b = cb_res_mint_amount b)%Z /\
    (cb_res_mint_delta s b <= RES_REWARD_GENESIS)%Z.
Proof.
  intros s b _.
  split.
  - apply cb_res_mint_delta_bounded.
  - apply cb_res_mint_delta_le_genesis.
Qed.

Lemma default_connect_mint_obligation :
  forall (hdr : BlockHeaderAbs) (txs : list TxAbs),
    valid_res_mint_obligation
      {| blk_header := hdr; blk_txs := txs; blk_mint := default_connect_mint |} = true.
Proof.
  intros hdr txs.
  unfold valid_res_mint_obligation, default_connect_mint; cbv zeta; simpl.
  destruct (res_mint_authorized _); reflexivity.
Qed.

(** ** Cap-overshoot reject (P0-2 / ASK-FV-MODEL-CAP-OVERSHOOT-REJECT-001)

    Dual insurance with [max_issuance_within_cap]: the finite sum shows the
    schedule cannot exhaust the cap under one mint per block; the runtime
    gate rejects any block whose attempted mint would. *)

Theorem cap_overshoot_rejects_apply_block :
  forall (s : ReplayState) (b : BlockAbs),
    (cb_MAX_RES_SUPPLY < rs_res_supply s + cb_res_attempted_amount b)%Z ->
    apply_block s b = None.
Proof.
  intros s b Hover. apply apply_block_rejects_overshoot. exact Hover.
Qed.

Theorem cap_exact_does_not_reject :
  forall (s : ReplayState) (b : BlockAbs),
    (rs_res_supply s + cb_res_attempted_amount b = cb_MAX_RES_SUPPLY)%Z ->
    valid_res_cap s b = true.
Proof.
  intros s b Heq. apply valid_res_cap_exact. exact Heq.
Qed.

Theorem cap_overshoot_one_atom_rejects_apply_block :
  forall (s : ReplayState) (b : BlockAbs),
    (rs_res_supply s + cb_res_attempted_amount b = cb_MAX_RES_SUPPLY + 1)%Z ->
    apply_block s b = None.
Proof.
  intros s b Heq. apply apply_block_rejects_one_atom_overshoot. exact Heq.
Qed.

(** Fail-closed with H-1: when [W(h) > 0] a winner must mint, but a mint that
    would breach the cap is rejected. Omitting the mint is winner-mismatch;
    including it is max-supply. No connecting block exists. *)
Theorem winner_cap_overshoot_fail_closed :
  forall (s : ReplayState) (b : BlockAbs),
    (0 < cb_res_mint_amount b)%Z ->
    cm_draw_won (blk_mint b) = true ->
    (cb_MAX_RES_SUPPLY < rs_res_supply s + cb_res_mint_amount b)%Z ->
    apply_block s b = None.
Proof.
  intros s b Hp Hwon Hover.
  unfold apply_block.
  destruct (valid_block_connect s b) eqn:Hc; [|reflexivity].
  exfalso.
  pose proof (valid_block_connect_res_mint_obligation s b Hc) as Hob.
  pose proof (valid_block_connect_res_cap s b Hc) as Hcap.
  unfold valid_res_mint_obligation in Hob; cbv zeta in Hob.
  pose proof Hp as Hp'.
  apply res_mint_authorized_iff_positive_W in Hp'.
  rewrite Hp', Hwon in Hob.
  apply Nat.eqb_eq in Hob.
  apply valid_res_cap_iff in Hcap.
  rewrite cb_res_attempted_amount_eq_W_when_mint in Hcap
    by (rewrite Hob; discriminate).
  unfold cb_MAX_RES_SUPPLY in *. lia.
Qed.

(** After the schedule terminates ([W(h) = 0]) a mint is never allowed, even
    if the draw named a winner. *)
Theorem schedule_exhausted_mint_rejected :
  forall (s : ReplayState) (b : BlockAbs),
    cb_res_mint_amount b = 0%Z ->
    cm_res_mint_count (blk_mint b) <> 0 ->
    apply_block s b = None.
Proof.
  intros s b Hz Hn.
  unfold apply_block.
  destruct (valid_block_connect s b) eqn:Hc; [|reflexivity].
  exfalso.
  pose proof (h1_valid_connect_zero_mints s b Hc Hz) as Hz0.
  contradiction.
Qed.

Theorem schedule_exhausted_winner_cannot_mint :
  forall (s : ReplayState) (b : BlockAbs) (s' : ReplayState),
    cb_res_mint_amount b = 0%Z ->
    cm_draw_won (blk_mint b) = true ->
    apply_block s b = Some s' ->
    cm_res_mint_count (blk_mint b) = 0 /\
    cb_res_mint_delta s b = 0%Z.
Proof.
  intros s b s' Hz _ Hab.
  unfold apply_block in Hab.
  destruct (valid_block_connect s b) eqn:Hc; [|discriminate Hab].
  split.
  - eapply h1_valid_connect_zero_mints; eassumption.
  - apply cb_res_mint_delta_zero_when_amount_zero. exact Hz.
Qed.

Theorem schedule_exhausted_after_horizon_cannot_mint :
  forall (s : ReplayState) (b : BlockAbs),
    (Z.of_nat (S RES_REWARD_LAST_EPOCH) * RES_REWARD_EPOCH <=
       Z.of_nat (hdr_height (blk_header b)))%Z ->
    cm_res_mint_count (blk_mint b) <> 0 ->
    apply_block s b = None.
Proof.
  intros s b Hh Hn.
  apply schedule_exhausted_mint_rejected; [|exact Hn].
  unfold cb_res_mint_amount.
  apply get_res_mint_reward_zero_after_horizon. exact Hh.
Qed.

(** Both belts: the finite-sum identity and the runtime reject. *)
Theorem cap_dual_insurance :
  (RES_REWARD_EPOCH * schedule_sum <= RES_MAX_SUPPLY)%Z /\
  (forall (s : ReplayState) (b : BlockAbs),
      valid_block_connect s b = true -> valid_res_cap s b = true).
Proof.
  split.
  - exact max_issuance_within_cap.
  - exact valid_block_connect_res_cap.
Qed.

Theorem connected_block_res_within_cap :
  forall (s : ReplayState) (b : BlockAbs),
    valid_block_connect s b = true ->
    (rs_res_supply s + cb_res_attempted_amount b <= cb_MAX_RES_SUPPLY)%Z.
Proof.
  intros s b Hc.
  apply valid_res_cap_iff.
  apply valid_block_connect_res_cap. exact Hc.
Qed.

(** ** Reject-reason paths (RFC-017 §4.4)

    Winner-mismatch is live only while [W(h) > 0]. After the schedule ends the
    mint-count check is the unauthorized-amount branch, never mismatch. *)

Lemma winner_mismatch_path_true_iff :
  forall b : BlockAbs,
    winner_mismatch_path b = true <->
    (res_mint_authorized b = true /\
     cm_draw_won (blk_mint b) = true /\
     cm_res_mint_count (blk_mint b) <> 1).
Proof.
  intros b. unfold winner_mismatch_path.
  split.
  - intro H.
    apply andb_true_iff in H as [Ha Hrest].
    apply andb_true_iff in Hrest as [Hw Hn].
    apply negb_true_iff in Hn. apply Nat.eqb_neq in Hn.
    repeat split; assumption.
  - intros [Ha [Hw Hn]].
    apply andb_true_iff. split; [exact Ha|].
    apply andb_true_iff. split; [exact Hw|].
    apply negb_true_iff. apply Nat.eqb_neq. exact Hn.
Qed.

Lemma unauthorized_mint_path_true_iff :
  forall b : BlockAbs,
    unauthorized_mint_path b = true <->
    (res_mint_authorized b = false /\
     cm_res_mint_count (blk_mint b) <> 0).
Proof.
  intros b. unfold unauthorized_mint_path.
  split.
  - intro H.
    apply andb_true_iff in H as [Ha Hn].
    apply negb_true_iff in Ha. apply negb_true_iff in Hn.
    apply Nat.eqb_neq in Hn. split; assumption.
  - intros [Ha Hn].
    apply andb_true_iff. split.
    + apply negb_true_iff. exact Ha.
    + apply negb_true_iff. apply Nat.eqb_neq. exact Hn.
Qed.

Lemma winner_positive_W_one_mint_obligation :
  forall b : BlockAbs,
    (0 < cb_res_mint_amount b)%Z ->
    cm_draw_won (blk_mint b) = true ->
    cm_res_mint_count (blk_mint b) = 1 ->
    valid_res_mint_obligation b = true.
Proof.
  intros b Hp Hwon Hn.
  unfold valid_res_mint_obligation; cbv zeta.
  apply res_mint_authorized_iff_positive_W in Hp. rewrite Hp, Hwon.
  apply Nat.eqb_eq. exact Hn.
Qed.

Lemma winner_positive_W_omit_mint_is_mismatch :
  forall b : BlockAbs,
    (0 < cb_res_mint_amount b)%Z ->
    cm_draw_won (blk_mint b) = true ->
    cm_res_mint_count (blk_mint b) = 0 ->
    winner_mismatch_path b = true /\
    valid_res_mint_obligation b = false.
Proof.
  intros b Hp Hwon Hn.
  split.
  - apply winner_mismatch_path_true_iff.
    apply res_mint_authorized_iff_positive_W in Hp.
    split; [exact Hp |]. split; [exact Hwon |]. rewrite Hn. discriminate.
  - unfold valid_res_mint_obligation; cbv zeta.
    apply res_mint_authorized_iff_positive_W in Hp. rewrite Hp, Hwon, Hn.
    reflexivity.
Qed.

(** After [W(h) = 0], winner-mismatch is never the reject path — including
    when the draw still named a winner. *)
Theorem schedule_exhausted_never_winner_mismatch :
  forall b : BlockAbs,
    cb_res_mint_amount b = 0%Z ->
    winner_mismatch_path b = false.
Proof.
  intros b Hz.
  unfold winner_mismatch_path.
  apply res_mint_authorized_false_iff_zero_W in Hz. rewrite Hz. reflexivity.
Qed.

Theorem schedule_exhausted_winner_zero_mint_not_mismatch :
  forall b : BlockAbs,
    cb_res_mint_amount b = 0%Z ->
    cm_draw_won (blk_mint b) = true ->
    cm_res_mint_count (blk_mint b) = 0 ->
    winner_mismatch_path b = false /\
    unauthorized_mint_path b = false /\
    valid_res_mint_obligation b = true.
Proof.
  intros b Hz Hwon Hn.
  split; [|split].
  - apply schedule_exhausted_never_winner_mismatch. exact Hz.
  - unfold unauthorized_mint_path. rewrite Hn. simpl.
    apply andb_false_r.
  - apply h1_zero_reward_winner_obligation; assumption.
Qed.

Theorem schedule_exhausted_mint_is_unauthorized_not_mismatch :
  forall (s : ReplayState) (b : BlockAbs),
    cb_res_mint_amount b = 0%Z ->
    cm_res_mint_count (blk_mint b) <> 0 ->
    unauthorized_mint_path b = true /\
    winner_mismatch_path b = false /\
    valid_res_mint_obligation b = false /\
    apply_block s b = None.
Proof.
  intros s b Hz Hn.
  split; [|split; [|split]].
  - apply unauthorized_mint_path_true_iff.
    apply res_mint_authorized_false_iff_zero_W in Hz. split; assumption.
  - apply schedule_exhausted_never_winner_mismatch. exact Hz.
  - unfold valid_res_mint_obligation; cbv zeta.
    apply res_mint_authorized_false_iff_zero_W in Hz. rewrite Hz.
    apply Nat.eqb_neq. exact Hn.
  - apply schedule_exhausted_mint_rejected; assumption.
Qed.

(** ** Exact cap vs one-atom overshoot, on [apply_block] *)

Theorem mint_at_exact_cap_allows_cap_conjunct :
  forall (s : ReplayState) (b : BlockAbs),
    (rs_res_supply s + cb_res_attempted_amount b = cb_MAX_RES_SUPPLY)%Z ->
    valid_res_cap s b = true.
Proof.
  intros s b Heq. apply cap_exact_does_not_reject. exact Heq.
Qed.

Theorem winner_exact_cap_connects :
  forall (s : ReplayState) (b : BlockAbs),
    valid_header_connect (rs_chain s) (blk_header b) = true ->
    valid_chainwork_extend (rs_chain s) (blk_header b) = true ->
    cb_oru_subsidy_ok (hdr_height (blk_header b)) (blk_mint b) = true ->
    (0 < cb_res_mint_amount b)%Z ->
    cm_draw_won (blk_mint b) = true ->
    cm_res_mint_count (blk_mint b) = 1 ->
    (rs_res_supply s + cb_res_mint_amount b = cb_MAX_RES_SUPPLY)%Z ->
    valid_block_connect s b = true.
Proof.
  intros s b Hh Hw Hs Hp Hwon Hn Hcap.
  unfold valid_block_connect.
  rewrite Hh, Hw, Hs. simpl.
  apply andb_true_intro. split.
  - apply winner_positive_W_one_mint_obligation; assumption.
  - apply valid_res_cap_exact.
    rewrite cb_res_attempted_amount_eq_W_when_mint by (rewrite Hn; discriminate).
    exact Hcap.
Qed.

Theorem winner_one_atom_over_include_mint_rejected :
  forall (s : ReplayState) (b : BlockAbs),
    (0 < cb_res_mint_amount b)%Z ->
    cm_draw_won (blk_mint b) = true ->
    cm_res_mint_count (blk_mint b) = 1 ->
    (rs_res_supply s + cb_res_mint_amount b = cb_MAX_RES_SUPPLY + 1)%Z ->
    valid_res_mint_obligation b = true /\
    winner_mismatch_path b = false /\
    valid_res_cap s b = false /\
    apply_block s b = None.
Proof.
  intros s b Hp Hwon Hn Hover.
  split; [|split; [|split]].
  - apply winner_positive_W_one_mint_obligation; assumption.
  - destruct (winner_mismatch_path b) eqn:Hm; [|reflexivity].
    apply winner_mismatch_path_true_iff in Hm.
    destruct Hm as [_ [_ Hne]]. rewrite Hn in Hne. contradiction.
  - apply valid_res_cap_overshoot_one_atom.
    rewrite cb_res_attempted_amount_eq_W_when_mint by (rewrite Hn; discriminate).
    exact Hover.
  - apply apply_block_rejects_one_atom_overshoot.
    rewrite cb_res_attempted_amount_eq_W_when_mint by (rewrite Hn; discriminate).
    exact Hover.
Qed.

(** ** H-1 × cap fail-closed combinatorial cases

    [W(h) > 0], a winner exists, and [supply + W(h)] would exceed the cap.
    Including the mint is max-supply; omitting it is winner-mismatch. Either
    way [apply_block] is [None]. *)

Theorem winner_cap_overshoot_include_mint_rejected :
  forall (s : ReplayState) (b : BlockAbs),
    (0 < cb_res_mint_amount b)%Z ->
    cm_draw_won (blk_mint b) = true ->
    cm_res_mint_count (blk_mint b) = 1 ->
    (cb_MAX_RES_SUPPLY < rs_res_supply s + cb_res_mint_amount b)%Z ->
    valid_res_mint_obligation b = true /\
    winner_mismatch_path b = false /\
    valid_res_cap s b = false /\
    apply_block s b = None.
Proof.
  intros s b Hp Hwon Hn Hover.
  split; [|split; [|split]].
  - apply winner_positive_W_one_mint_obligation; assumption.
  - destruct (winner_mismatch_path b) eqn:Hm; [|reflexivity].
    apply winner_mismatch_path_true_iff in Hm.
    destruct Hm as [_ [_ Hne]]. rewrite Hn in Hne. contradiction.
  - apply valid_res_cap_overshoot.
    rewrite cb_res_attempted_amount_eq_W_when_mint by (rewrite Hn; discriminate).
    exact Hover.
  - apply winner_cap_overshoot_fail_closed; assumption.
Qed.

Theorem winner_cap_overshoot_omit_mint_rejected :
  forall (s : ReplayState) (b : BlockAbs),
    (0 < cb_res_mint_amount b)%Z ->
    cm_draw_won (blk_mint b) = true ->
    cm_res_mint_count (blk_mint b) = 0 ->
    (cb_MAX_RES_SUPPLY < rs_res_supply s + cb_res_mint_amount b)%Z ->
    valid_res_mint_obligation b = false /\
    winner_mismatch_path b = true /\
    ((rs_res_supply s <= cb_MAX_RES_SUPPLY)%Z -> valid_res_cap s b = true) /\
    apply_block s b = None.
Proof.
  intros s b Hp Hwon Hn Hover.
  destruct (winner_positive_W_omit_mint_is_mismatch b Hp Hwon Hn) as [Hm Hob].
  split; [|split; [|split]].
  - exact Hob.
  - exact Hm.
  - intro Hpre. apply valid_res_cap_no_mint; [exact Hn | exact Hpre].
  - apply winner_cap_overshoot_fail_closed; assumption.
Qed.

Theorem winner_cap_overshoot_fail_closed_any_count :
  forall (s : ReplayState) (b : BlockAbs),
    (0 < cb_res_mint_amount b)%Z ->
    cm_draw_won (blk_mint b) = true ->
    (cb_MAX_RES_SUPPLY < rs_res_supply s + cb_res_mint_amount b)%Z ->
    apply_block s b = None.
Proof.
  intros s b Hp Hwon Hover.
  apply winner_cap_overshoot_fail_closed; assumption.
Qed.

(** Top-level dual insurance: the compile-time finite sum and the runtime
    [apply_block] reject, including the exact-cap / one-atom pins.
    [res_mint_respects_cap] remains the redundant delta clamp. Not a C++
    refinement; honesty flags unchanged. *)
Theorem res_issuance_dual_insurance :
  (RES_REWARD_EPOCH * schedule_sum <= RES_MAX_SUPPLY)%Z /\
  (forall (s : ReplayState) (b : BlockAbs),
      (cb_MAX_RES_SUPPLY < rs_res_supply s + cb_res_attempted_amount b)%Z ->
      apply_block s b = None) /\
  (forall (s : ReplayState) (b : BlockAbs),
      valid_block_connect s b = true ->
      (rs_res_supply s + cb_res_attempted_amount b <= cb_MAX_RES_SUPPLY)%Z) /\
  (forall (s : ReplayState) (b : BlockAbs),
      (rs_res_supply s + cb_res_attempted_amount b = cb_MAX_RES_SUPPLY)%Z ->
      valid_res_cap s b = true) /\
  (forall (s : ReplayState) (b : BlockAbs),
      (rs_res_supply s + cb_res_attempted_amount b = cb_MAX_RES_SUPPLY + 1)%Z ->
      apply_block s b = None).
Proof.
  split; [|split; [|split; [|split]]].
  - exact max_issuance_within_cap.
  - exact cap_overshoot_rejects_apply_block.
  - exact connected_block_res_within_cap.
  - exact cap_exact_does_not_reject.
  - exact cap_overshoot_one_atom_rejects_apply_block.
Qed.

(** ** Full [valid_block_connect] classification
    (TASK-FV-MODEL-CAP-H1-FULL-CONNECT-PATH-001)

    Mint amount at this layer is definitionally [W(h)] ([cb_res_mint_amount]).
    A wrong body amount is rejected as mint-shape ([res_mint_shape_rejects_wrong_amount]),
    not as a second field on [ConnectMintSpec]. *)

Lemma without_winner_mint_path_true_iff :
  forall b : BlockAbs,
    without_winner_mint_path b = true <->
    (cm_draw_won (blk_mint b) = false /\
     cm_res_mint_count (blk_mint b) <> 0).
Proof.
  intros b. unfold without_winner_mint_path.
  split.
  - intro H.
    apply andb_true_iff in H as [Hw Hn].
    apply negb_true_iff in Hw. apply negb_true_iff in Hn.
    apply Nat.eqb_neq in Hn. split; assumption.
  - intros [Hw Hn].
    apply andb_true_iff. split.
    + apply negb_true_iff. exact Hw.
    + apply negb_true_iff. apply Nat.eqb_neq. exact Hn.
Qed.

Lemma valid_res_mint_obligation_true_iff :
  forall b : BlockAbs,
    valid_res_mint_obligation b = true <->
      ((res_mint_authorized b = true /\
        cm_draw_won (blk_mint b) = true /\
        cm_res_mint_count (blk_mint b) = 1) \/
       (res_mint_authorized b = true /\
        cm_draw_won (blk_mint b) = false /\
        cm_res_mint_count (blk_mint b) = 0) \/
       (res_mint_authorized b = false /\
        cm_res_mint_count (blk_mint b) = 0)).
Proof.
  intros b.
  unfold valid_res_mint_obligation; cbv zeta.
  destruct (res_mint_authorized b) eqn:Ha;
    destruct (cm_draw_won (blk_mint b)) eqn:Hw;
    split; intro H.
  - left. split; [reflexivity|]. split; [reflexivity|]. apply Nat.eqb_eq. exact H.
  - destruct H as [[_ [_ Hn]] | [[_ [Hw' _]] | [Ha' _]]].
    + apply Nat.eqb_eq. exact Hn.
    + congruence.
    + congruence.
  - right. left. split; [reflexivity|]. split; [reflexivity|]. apply Nat.eqb_eq. exact H.
  - destruct H as [[_ [Hw' _]] | [[_ [_ Hn]] | [Ha' _]]].
    + congruence.
    + apply Nat.eqb_eq. exact Hn.
    + congruence.
  - right. right. split; [reflexivity|apply Nat.eqb_eq; exact H].
  - destruct H as [[Ha' _] | [[Ha' _] | [_ Hn]]].
    + congruence.
    + congruence.
    + apply Nat.eqb_eq. exact Hn.
  - right. right. split; [reflexivity|apply Nat.eqb_eq; exact H].
  - destruct H as [[Ha' _] | [[Ha' _] | [_ Hn]]].
    + congruence.
    + congruence.
    + apply Nat.eqb_eq. exact Hn.
Qed.

Theorem mint_obligation_reject_paths :
  forall b : BlockAbs,
    valid_res_mint_obligation b = false <->
      (winner_mismatch_path b = true \/
       unauthorized_mint_path b = true \/
       without_winner_mint_path b = true).
Proof.
  intros b.
  split.
  - intro Hf.
    rewrite <- Bool.not_true_iff_false in Hf.
    destruct (cm_draw_won (blk_mint b)) eqn:Hw;
      destruct (res_mint_authorized b) eqn:Ha;
      destruct (Nat.eqb (cm_res_mint_count (blk_mint b)) 0) eqn:H0.
    + left. apply winner_mismatch_path_true_iff.
      apply Nat.eqb_eq in H0. split; [exact Ha|]. split; [exact Hw|].
      rewrite H0. discriminate.
    + destruct (Nat.eqb (cm_res_mint_count (blk_mint b)) 1) eqn:H1.
      * exfalso. apply Hf. unfold valid_res_mint_obligation; cbv zeta.
        rewrite Ha, Hw, H1. reflexivity.
      * left. apply winner_mismatch_path_true_iff.
        apply Nat.eqb_neq in H1. split; [exact Ha|]. split; [exact Hw|]. exact H1.
    + exfalso. apply Hf. unfold valid_res_mint_obligation; cbv zeta.
      rewrite Ha. apply Nat.eqb_eq in H0. rewrite H0. reflexivity.
    + right. left. apply unauthorized_mint_path_true_iff.
      apply Nat.eqb_neq in H0. split; [exact Ha|exact H0].
    + exfalso. apply Hf. unfold valid_res_mint_obligation; cbv zeta.
      rewrite Ha, Hw. apply Nat.eqb_eq in H0. rewrite H0. reflexivity.
    + right. right. apply without_winner_mint_path_true_iff.
      apply Nat.eqb_neq in H0. split; [exact Hw|exact H0].
    + exfalso. apply Hf. unfold valid_res_mint_obligation; cbv zeta.
      rewrite Ha. apply Nat.eqb_eq in H0. rewrite H0. reflexivity.
    + right. left. apply unauthorized_mint_path_true_iff.
      apply Nat.eqb_neq in H0. split; [exact Ha|exact H0].
  - intros [Hm | [Hu | Hnw]].
    + apply winner_mismatch_path_true_iff in Hm.
      destruct Hm as [Ha [Hw Hn]].
      unfold valid_res_mint_obligation; cbv zeta.
      rewrite Ha, Hw. apply Nat.eqb_neq. exact Hn.
    + apply unauthorized_mint_path_true_iff in Hu.
      destruct Hu as [Ha Hn].
      unfold valid_res_mint_obligation; cbv zeta.
      rewrite Ha. apply Nat.eqb_neq. exact Hn.
    + apply without_winner_mint_path_true_iff in Hnw.
      destruct Hnw as [Hw Hn].
      unfold valid_res_mint_obligation; cbv zeta.
      rewrite Hw.
      destruct (res_mint_authorized b); apply Nat.eqb_neq; exact Hn.
Qed.

Lemma winner_positive_W_obligation_iff_one_mint :
  forall b : BlockAbs,
    cm_draw_won (blk_mint b) = true ->
    (0 < cb_res_mint_amount b)%Z ->
    valid_res_mint_obligation b = true <->
      cm_res_mint_count (blk_mint b) = 1.
Proof.
  intros b Hwon Hp. split.
  - intro Hob.
    unfold valid_res_mint_obligation in Hob; cbv zeta in Hob.
    apply res_mint_authorized_iff_positive_W in Hp. rewrite Hp, Hwon in Hob.
    apply Nat.eqb_eq. exact Hob.
  - intro Hn. apply winner_positive_W_one_mint_obligation; assumption.
Qed.

Lemma winner_positive_W_cap_iff_when_one_mint :
  forall (s : ReplayState) (b : BlockAbs),
    cm_res_mint_count (blk_mint b) = 1 ->
    valid_res_cap s b = true <->
      (rs_res_supply s + cb_res_mint_amount b <= cb_MAX_RES_SUPPLY)%Z.
Proof.
  intros s b Hn.
  unfold valid_res_cap.
  rewrite cb_res_attempted_amount_eq_W_when_mint by (rewrite Hn; discriminate).
  unfold res_supply_plus_mint_within_cap, cb_MAX_RES_SUPPLY.
  split; apply Z.leb_le.
Qed.

(** Top-level success classification: a winner with [W(h) > 0] connects iff
    header / chainwork / ORU subsidy hold, the block carries exactly one mint,
    and that mint does not exceed the hard cap. The mint amount is [W(h)] by
    definition. *)
Theorem winner_positive_W_connect_iff :
  forall (s : ReplayState) (b : BlockAbs),
    cm_draw_won (blk_mint b) = true ->
    (0 < cb_res_mint_amount b)%Z ->
    valid_block_connect s b = true <->
      (valid_header_connect (rs_chain s) (blk_header b) = true /\
       valid_chainwork_extend (rs_chain s) (blk_header b) = true /\
       cb_oru_subsidy_ok (hdr_height (blk_header b)) (blk_mint b) = true /\
       cm_res_mint_count (blk_mint b) = 1 /\
       (rs_res_supply s + cb_res_mint_amount b <= cb_MAX_RES_SUPPLY)%Z).
Proof.
  intros s b Hwon Hp.
  split.
  - intro Hc.
    apply valid_block_connect_iff in Hc.
    destruct Hc as [Hh [Hw [Hs [Hob Hcap]]]].
    apply (winner_positive_W_obligation_iff_one_mint b Hwon Hp) in Hob.
    apply (winner_positive_W_cap_iff_when_one_mint s b Hob) in Hcap.
    repeat split; assumption.
  - intros [Hh [Hw [Hs [Hn Hcap]]]].
    apply valid_block_connect_iff.
    repeat split; try assumption.
    + apply winner_positive_W_obligation_iff_one_mint; assumption.
    + apply winner_positive_W_cap_iff_when_one_mint; assumption.
Qed.

Theorem winner_positive_W_connect_failure_iff :
  forall (s : ReplayState) (b : BlockAbs),
    cm_draw_won (blk_mint b) = true ->
    (0 < cb_res_mint_amount b)%Z ->
    valid_header_connect (rs_chain s) (blk_header b) = true ->
    valid_chainwork_extend (rs_chain s) (blk_header b) = true ->
    cb_oru_subsidy_ok (hdr_height (blk_header b)) (blk_mint b) = true ->
    valid_block_connect s b = false <->
      (cm_res_mint_count (blk_mint b) <> 1 \/
       (cm_res_mint_count (blk_mint b) = 1 /\
        (cb_MAX_RES_SUPPLY < rs_res_supply s + cb_res_mint_amount b)%Z)).
Proof.
  intros s b Hwon Hp Hh Hw Hs.
  split.
  - intro Hf.
    destruct (Nat.eq_dec (cm_res_mint_count (blk_mint b)) 1) as [Hn|Hn].
    + right. split; [exact Hn|].
      destruct (Z.ltb cb_MAX_RES_SUPPLY (rs_res_supply s + cb_res_mint_amount b)) eqn:Hlt.
      * apply Z.ltb_lt. exact Hlt.
      * exfalso.
        apply Bool.not_true_iff_false in Hf. apply Hf.
        apply winner_positive_W_connect_iff; try assumption.
        repeat split; try assumption.
        apply Z.ltb_ge in Hlt. exact Hlt.
    + left. exact Hn.
  - intros [Hn | [Hn Hover]].
    + apply Bool.not_true_iff_false. intro Hc.
      apply winner_positive_W_connect_iff in Hc; try assumption.
      destruct Hc as [_ [_ [_ [Hn' _]]]]. contradiction.
    + apply Bool.not_true_iff_false. intro Hc.
      apply winner_positive_W_connect_iff in Hc; try assumption.
      destruct Hc as [_ [_ [_ [_ Hle]]]]. lia.
Qed.

Theorem winner_wrong_mint_count_rejected :
  forall (s : ReplayState) (b : BlockAbs),
    (0 < cb_res_mint_amount b)%Z ->
    cm_draw_won (blk_mint b) = true ->
    cm_res_mint_count (blk_mint b) <> 1 ->
    winner_mismatch_path b = true /\
    valid_res_mint_obligation b = false /\
    apply_block s b = None.
Proof.
  intros s b Hp Hwon Hn.
  split; [|split].
  - apply winner_mismatch_path_true_iff.
    apply res_mint_authorized_iff_positive_W in Hp.
    repeat split; assumption.
  - unfold valid_res_mint_obligation; cbv zeta.
    apply res_mint_authorized_iff_positive_W in Hp. rewrite Hp, Hwon.
    apply Nat.eqb_neq. exact Hn.
  - unfold apply_block.
    destruct (valid_block_connect s b) eqn:Hc; [|reflexivity].
    exfalso.
    pose proof (winner_must_mint s b Hc Hp Hwon) as H1. contradiction.
Qed.

Theorem without_winner_mint_rejected :
  forall (s : ReplayState) (b : BlockAbs),
    cm_draw_won (blk_mint b) = false ->
    cm_res_mint_count (blk_mint b) <> 0 ->
    without_winner_mint_path b = true /\
    valid_res_mint_obligation b = false /\
    apply_block s b = None.
Proof.
  intros s b Hnwon Hn.
  split; [|split].
  - apply without_winner_mint_path_true_iff. split; assumption.
  - apply mint_obligation_reject_paths. right. right.
    apply without_winner_mint_path_true_iff. split; assumption.
  - unfold apply_block.
    destruct (valid_block_connect s b) eqn:Hc; [|reflexivity].
    exfalso.
    pose proof (no_winner_zero_mints s b Hc Hnwon) as Hz. contradiction.
Qed.

Theorem exhausted_schedule_connect_iff :
  forall (s : ReplayState) (b : BlockAbs),
    cb_res_mint_amount b = 0%Z ->
    valid_block_connect s b = true <->
      (valid_header_connect (rs_chain s) (blk_header b) = true /\
       valid_chainwork_extend (rs_chain s) (blk_header b) = true /\
       cb_oru_subsidy_ok (hdr_height (blk_header b)) (blk_mint b) = true /\
       cm_res_mint_count (blk_mint b) = 0 /\
       (rs_res_supply s <= cb_MAX_RES_SUPPLY)%Z).
Proof.
  intros s b Hz.
  split.
  - intro Hc.
    apply valid_block_connect_iff in Hc.
    destruct Hc as [Hh [Hw [Hs [Hob Hcap]]]].
    assert (Hn0 : cm_res_mint_count (blk_mint b) = 0).
    { unfold valid_res_mint_obligation in Hob; cbv zeta in Hob.
      pose proof Hz as Hz'.
      apply res_mint_authorized_false_iff_zero_W in Hz'. rewrite Hz' in Hob.
      apply Nat.eqb_eq. exact Hob. }
    apply (proj1 (valid_res_cap_no_mint s b Hn0)) in Hcap.
    repeat split; assumption.
  - intros [Hh [Hw [Hs [Hn Hcap]]]].
    apply valid_block_connect_iff.
    repeat split; try assumption.
    + apply h1_zero_reward_obligation_holds; assumption.
    + apply valid_res_cap_no_mint; assumption.
Qed.

Theorem no_winner_connect_iff :
  forall (s : ReplayState) (b : BlockAbs),
    cm_draw_won (blk_mint b) = false ->
    valid_block_connect s b = true <->
      (valid_header_connect (rs_chain s) (blk_header b) = true /\
       valid_chainwork_extend (rs_chain s) (blk_header b) = true /\
       cb_oru_subsidy_ok (hdr_height (blk_header b)) (blk_mint b) = true /\
       cm_res_mint_count (blk_mint b) = 0 /\
       (rs_res_supply s <= cb_MAX_RES_SUPPLY)%Z).
Proof.
  intros s b Hnwon.
  split.
  - intro Hc.
    apply valid_block_connect_iff in Hc.
    destruct Hc as [Hh [Hw [Hs [Hob Hcap]]]].
    assert (Hn0 : cm_res_mint_count (blk_mint b) = 0).
    { unfold valid_res_mint_obligation in Hob; cbv zeta in Hob.
      rewrite Hnwon in Hob.
      destruct (res_mint_authorized b); apply Nat.eqb_eq; exact Hob. }
    apply (proj1 (valid_res_cap_no_mint s b Hn0)) in Hcap.
    repeat split; assumption.
  - intros [Hh [Hw [Hs [Hn Hcap]]]].
    apply valid_block_connect_iff.
    repeat split; try assumption.
    + unfold valid_res_mint_obligation; cbv zeta.
      rewrite Hnwon.
      destruct (res_mint_authorized b); apply Nat.eqb_eq; exact Hn.
    + apply valid_res_cap_no_mint; assumption.
Qed.

(** After a block lands exactly on the cap, the next winner mint with [W>0]
    cannot connect — fail-closed, any mint count. *)
Theorem at_hard_cap_next_winner_mint_rejected :
  forall (s : ReplayState) (b : BlockAbs),
    rs_res_supply s = cb_MAX_RES_SUPPLY ->
    (0 < cb_res_mint_amount b)%Z ->
    cm_draw_won (blk_mint b) = true ->
    valid_block_connect s b = false /\
    apply_block s b = None.
Proof.
  intros s b Hat Hp Hwon.
  assert (Hover : (cb_MAX_RES_SUPPLY < rs_res_supply s + cb_res_mint_amount b)%Z)
    by (rewrite Hat; lia).
  split.
  - apply Bool.not_true_iff_false. intro Hc.
    apply winner_positive_W_connect_iff in Hc; try assumption.
    destruct Hc as [_ [_ [_ [_ Hle]]]]. lia.
  - apply winner_cap_overshoot_fail_closed; assumption.
Qed.

Theorem last_paying_epoch_still_requires_mint :
  forall (s : ReplayState) (b : BlockAbs),
    Z.of_nat (hdr_height (blk_header b)) =
      (Z.of_nat (S RES_REWARD_LAST_EPOCH) * RES_REWARD_EPOCH - 1)%Z ->
    (0 < cb_res_mint_amount b)%Z.
Proof.
  intros s b Hh.
  unfold cb_res_mint_amount. rewrite Hh.
  rewrite get_res_mint_reward_epoch_last.
  apply reward_at_last_epoch_is_positive.
Qed.

Theorem first_exhausted_epoch_forbids_mint :
  forall (s : ReplayState) (b : BlockAbs),
    Z.of_nat (hdr_height (blk_header b)) =
      (Z.of_nat (S RES_REWARD_LAST_EPOCH) * RES_REWARD_EPOCH)%Z ->
    cb_res_mint_amount b = 0%Z.
Proof.
  intros s b Hh.
  unfold cb_res_mint_amount. rewrite Hh.
  rewrite get_res_mint_reward_epoch_start.
  apply reward_after_last_epoch_is_zero.
Qed.

Theorem first_exhausted_epoch_winner_mint_rejected :
  forall (s : ReplayState) (b : BlockAbs),
    Z.of_nat (hdr_height (blk_header b)) =
      (Z.of_nat (S RES_REWARD_LAST_EPOCH) * RES_REWARD_EPOCH)%Z ->
    cm_res_mint_count (blk_mint b) <> 0 ->
    apply_block s b = None.
Proof.
  intros s b Hh Hn.
  apply schedule_exhausted_mint_rejected; [|exact Hn].
  eapply first_exhausted_epoch_forbids_mint; eassumption.
Qed.

Theorem connect_mint_amount_is_schedule :
  forall (s : ReplayState) (b : BlockAbs),
    valid_block_connect s b = true ->
    cb_res_mint_amount b =
      get_res_mint_reward (Z.of_nat (hdr_height (blk_header b))).
Proof.
  intros s b _. apply obligated_mint_amount_is_schedule_term.
Qed.

(** Exhaustive connect reject: every [valid_block_connect = false] is header,
    chainwork, ORU subsidy, one of the three mint-obligation paths, or cap. *)
Theorem valid_block_connect_reject_classification :
  forall (s : ReplayState) (b : BlockAbs),
    valid_block_connect s b = false <->
      (valid_header_connect (rs_chain s) (blk_header b) = false \/
       valid_chainwork_extend (rs_chain s) (blk_header b) = false \/
       cb_oru_subsidy_ok (hdr_height (blk_header b)) (blk_mint b) = false \/
       winner_mismatch_path b = true \/
       unauthorized_mint_path b = true \/
       without_winner_mint_path b = true \/
       valid_res_cap s b = false).
Proof.
  intros s b.
  rewrite valid_block_connect_false_iff.
  rewrite mint_obligation_reject_paths.
  tauto.
Qed.

Theorem connect_path_first_epoch_boundary :
  get_res_mint_reward (RES_REWARD_EPOCH - 1)%Z = RES_REWARD_GENESIS /\
  get_res_mint_reward RES_REWARD_EPOCH = res_reward_for_epoch 1%nat /\
  (get_res_mint_reward RES_REWARD_EPOCH <
     get_res_mint_reward (RES_REWARD_EPOCH - 1)%Z)%Z.
Proof.
  exact reward_crosses_first_epoch_boundary.
Qed.

Theorem connect_path_last_epoch_boundary :
  (0 < get_res_mint_reward
         (Z.of_nat RES_REWARD_LAST_EPOCH * RES_REWARD_EPOCH)%Z)%Z /\
  (0 < get_res_mint_reward
         (Z.of_nat (S RES_REWARD_LAST_EPOCH) * RES_REWARD_EPOCH - 1)%Z)%Z /\
  get_res_mint_reward
    (Z.of_nat (S RES_REWARD_LAST_EPOCH) * RES_REWARD_EPOCH)%Z = 0%Z.
Proof.
  exact reward_crosses_last_epoch_boundary.
Qed.

(** Dual insurance plus the full connect-path classification
    (TASK-FV-MODEL-CAP-H1-FULL-CONNECT-PATH-001). *)
Theorem res_issuance_dual_insurance_connect_path :
  (RES_REWARD_EPOCH * schedule_sum <= RES_MAX_SUPPLY)%Z /\
  (forall (s : ReplayState) (b : BlockAbs),
      cm_draw_won (blk_mint b) = true ->
      (0 < cb_res_mint_amount b)%Z ->
      valid_block_connect s b = true <->
        (valid_header_connect (rs_chain s) (blk_header b) = true /\
         valid_chainwork_extend (rs_chain s) (blk_header b) = true /\
         cb_oru_subsidy_ok (hdr_height (blk_header b)) (blk_mint b) = true /\
         cm_res_mint_count (blk_mint b) = 1 /\
         (rs_res_supply s + cb_res_mint_amount b <= cb_MAX_RES_SUPPLY)%Z)) /\
  (forall (s : ReplayState) (b : BlockAbs),
      valid_block_connect s b = false <->
        (valid_header_connect (rs_chain s) (blk_header b) = false \/
         valid_chainwork_extend (rs_chain s) (blk_header b) = false \/
         cb_oru_subsidy_ok (hdr_height (blk_header b)) (blk_mint b) = false \/
         winner_mismatch_path b = true \/
         unauthorized_mint_path b = true \/
         without_winner_mint_path b = true \/
         valid_res_cap s b = false)) /\
  (forall (s : ReplayState) (b : BlockAbs),
      rs_res_supply s = cb_MAX_RES_SUPPLY ->
      (0 < cb_res_mint_amount b)%Z ->
      cm_draw_won (blk_mint b) = true ->
      apply_block s b = None) /\
  (forall (s : ReplayState) (b : BlockAbs),
      (cb_MAX_RES_SUPPLY < rs_res_supply s + cb_res_attempted_amount b)%Z ->
      apply_block s b = None) /\
  (forall (s : ReplayState) (b : BlockAbs),
      valid_block_connect s b = true ->
      (rs_res_supply s + cb_res_attempted_amount b <= cb_MAX_RES_SUPPLY)%Z) /\
  (forall (tx : TxAbs) (ins : list TxOut) (reward : Z),
      (sum_asset AssetRES (tx_creates tx) <> reward)%Z ->
      res_mint_shape tx ins reward = false) /\
  (get_res_mint_reward (RES_REWARD_EPOCH - 1)%Z = RES_REWARD_GENESIS /\
   get_res_mint_reward RES_REWARD_EPOCH = res_reward_for_epoch 1%nat /\
   (get_res_mint_reward RES_REWARD_EPOCH <
      get_res_mint_reward (RES_REWARD_EPOCH - 1)%Z)%Z) /\
  ((0 < get_res_mint_reward
          (Z.of_nat RES_REWARD_LAST_EPOCH * RES_REWARD_EPOCH)%Z)%Z /\
   (0 < get_res_mint_reward
          (Z.of_nat (S RES_REWARD_LAST_EPOCH) * RES_REWARD_EPOCH - 1)%Z)%Z /\
   get_res_mint_reward
     (Z.of_nat (S RES_REWARD_LAST_EPOCH) * RES_REWARD_EPOCH)%Z = 0%Z).
Proof.
  split; [|split; [|split; [|split; [|split; [|split; [|split; [|split]]]]]]].
  - exact max_issuance_within_cap.
  - exact winner_positive_W_connect_iff.
  - exact valid_block_connect_reject_classification.
  - intros s b Hat Hp Hwon.
    apply at_hard_cap_next_winner_mint_rejected; assumption.
  - exact cap_overshoot_rejects_apply_block.
  - exact connected_block_res_within_cap.
  - exact res_mint_shape_rejects_wrong_amount.
  - exact connect_path_first_epoch_boundary.
  - exact connect_path_last_epoch_boundary.
Qed.

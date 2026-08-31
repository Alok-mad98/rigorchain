(** ReplayReorgCore/DualPoolConservation.v
    Coq P1 + TASK-FV-MODEL-DUALPOOL-APPLY-TX-001 + HARDENING-001 +
    BLOCK-LEVEL-INVARIANT-001:
    dual-pool transfer conservation ([res_in ≥ res_out]) and the mint-shape
    exception (RES may increase by exactly [W(h)], and no more).

    Predicates live in [StateTransition] and are a conjunct of [apply_tx].
    After successful [apply_block] / [valid_block_connect], body RES net is
    ordinary slack (≤ 0) plus [n] scheduled mints of [W(h)]; scalar supply
    follows protocol mint; rollback-1 (and multi-block rollback) restores
    the pre-state. Citable projection:
    [apply_block_res_net_bounded_by_scheduled_mints]. Not a C++ refinement.
    Out of scope: PCORB field *semantics*, σ, owner encoding,
    [ConnectEvaluateResMintTx], RFC-018 ORU fee burn, 100-block maturity.
    Finite script *shape* (allowed-template / spend-CVG2 / PCORB sizes) is
    a conjunct of [apply_tx] ([ScriptShape.v]). Pooling class is
    [output_asset] = [asset_kind_of_script out_script]. Residual opaque
    boundaries ([cm_draw_won], [rs_res_supply] independent of [utxo_res],
    no script VM): [OpaqueBoundary.v]. *)
From Coq Require Import ZArith.ZArith.
From Coq Require Import List.
From Coq Require Import Arith.
From Coq Require Import Bool.
From Coq Require Import Lia.
Import ListNotations.
Require Import Rigor.ReplayReorgCore.ReplayState.
Require Import Rigor.ReplayReorgCore.ConnectBlockMint.
Require Import Rigor.ReplayReorgCore.StateTransition.
Require Import Rigor.ReplayReorgCore.ScriptShape.
Require Import Rigor.ReplayReorgCore.CvgCovenant.
Require Import Rigor.ReplayReorgCore.ReplayInvariants.
Require Import Rigor.ReplayReorgCore.ResMintObligation.
Require Import Rigor.ReplayReorgCore.ReorgRules.
Require Import Rigor.ReplayReorgCore.SupplyConservation.
Require Import Rigor.ReplayReorgCore.ReplayDeterminism.
From Rigor Require Import ResSchedule.

(** * Asset pooling *)

Lemma asset_eqb_refl : forall k, asset_eqb k k = true.
Proof. intros k. destruct k; reflexivity. Qed.

Lemma asset_eqb_eq : forall a b, asset_eqb a b = true -> a = b.
Proof. intros a b H. destruct a, b; simpl in H; try discriminate; reflexivity. Qed.

(** * Retired OP_CVG_CLAIM scripts (W3 design study)

    C++ has no such template. The classifier maps them to [AssetInvalid],
    not a live pooling class. [count_cvg] still recognizes the byte shape
    for design-study lemmas. There is no [AssetCVG] constructor (TASK-010 R-9).
    Convergence therefore cannot create RES. *)

Lemma count_cvg_zero_of_negb :
  forall os : list TxOut,
    forallb (fun o => negb (is_cvg_covenant_output o)) os = true ->
    count_cvg os = 0%nat.
Proof.
  intros os H. unfold count_cvg.
  induction os as [|o rest IH]; [reflexivity|].
  simpl in H. apply andb_true_iff in H as [Ho Hrest].
  simpl. destruct (is_cvg_covenant_output o) eqn:Ec.
  - simpl in Ho. discriminate Ho.
  - apply IH. exact Hrest.
Qed.

Lemma sum_asset_res_of_cvg_list :
  forall outs : list TxOut,
    forallb is_cvg_covenant_output outs = true ->
    sum_asset AssetRES outs = 0%Z.
Proof.
  intros outs H. induction outs as [|o rest IH]; [reflexivity|].
  simpl in H. apply andb_true_iff in H as [Ho Hrest].
  simpl. rewrite (txout_cvg_not_res o Ho). rewrite (IH Hrest). reflexivity.
Qed.

Lemma count_asset_res_of_cvg_list :
  forall outs : list TxOut,
    forallb is_cvg_covenant_output outs = true ->
    count_asset AssetRES outs = 0%nat.
Proof.
  intros outs H. unfold count_asset.
  induction outs as [|o rest IH]; [reflexivity|].
  simpl in H. apply andb_true_iff in H as [Ho Hrest].
  simpl. rewrite (output_asset_cvg o Ho). simpl. apply IH. exact Hrest.
Qed.

Theorem convergence_cannot_create_res :
  forall tx : TxAbs,
    forallb is_cvg_covenant_output (tx_creates tx) = true ->
    count_asset AssetRES (tx_creates tx) = 0%nat /\
    sum_asset AssetRES (tx_creates tx) = 0%Z.
Proof.
  intros tx H.
  split.
  - apply count_asset_res_of_cvg_list. exact H.
  - apply sum_asset_res_of_cvg_list. exact H.
Qed.

Theorem covenant_spend_cannot_recreate_cvg :
  forall (ins : list TxOut) (tx : TxAbs),
    valid_cvg_covenant_spend ins tx = true ->
    count_cvg ins = 1%nat ->
    count_cvg (tx_creates tx) = 0%nat.
Proof.
  intros ins tx H Hc.
  apply (valid_cvg_covenant_spend_no_recreate ins tx H Hc).
Qed.

Theorem cvg_ticket_not_recirculated_as_oru :
  forall o : TxOut,
    is_cvg_covenant_output o = true ->
    txout_asset_value AssetORU o = 0%Z.
Proof.
  intros o H. apply txout_cvg_not_oru. exact H.
Qed.

Theorem sum_asset_oru_skips_cvg :
  forall outs : list TxOut,
    sum_asset AssetORU outs =
    sum_asset AssetORU (filter (fun o => negb (is_cvg_covenant_output o)) outs).
Proof.
  intros outs. induction outs as [|o rest IH]; [reflexivity|].
  simpl. destruct (is_cvg_covenant_output o) eqn:Ec.
  - rewrite (txout_cvg_not_oru o Ec). simpl. exact IH.
  - simpl. rewrite IH. reflexivity.
Qed.

Lemma sum_asset_app :
  forall (k : AssetKind) (xs ys : list TxOut),
    sum_asset k (xs ++ ys) = (sum_asset k xs + sum_asset k ys)%Z.
Proof.
  intros k xs ys. induction xs as [|o rest IH]; simpl; lia.
Qed.

(** * Ordinary conservation: [res_in ≥ res_out] *)

Theorem dual_pool_conservation :
  forall (u : UTXOSet) (tx : TxAbs),
    valid_non_mint_tx u tx = true ->
    exists ri : Z,
      res_in_of u tx = Some ri /\
      (ri >= res_out_of tx)%Z.
Proof.
  intros u tx H.
  unfold valid_non_mint_tx in H.
  destruct (gather_inputs u (tx_spends tx)) as [ins|] eqn:G; [|discriminate H].
  unfold valid_ordinary_dual_pool in H.
  apply andb_true_iff in H as [Hge _].
  apply Z.geb_le in Hge.
  exists (sum_asset AssetRES ins).
  split.
  - unfold res_in_of. rewrite G. reflexivity.
  - unfold res_out_of. lia.
Qed.

Lemma dual_pool_conservation_on_ins :
  forall (ins : list TxOut) (tx : TxAbs),
    valid_ordinary_dual_pool ins tx = true ->
    (sum_asset AssetRES ins >= sum_asset AssetRES (tx_creates tx))%Z.
Proof.
  intros ins tx H.
  unfold valid_ordinary_dual_pool in H.
  apply andb_true_iff in H as [Hge _].
  apply Z.geb_le in Hge. lia.
Qed.

(** * Mint exception: RES increases by exactly [W(h)], never more *)

Lemma res_mint_shape_zero_res_in :
  forall tx ins reward,
    res_mint_shape tx ins reward = true ->
    sum_asset AssetRES ins = 0%Z.
Proof.
  intros tx ins reward H.
  unfold res_mint_shape in H.
  apply andb_true_iff in H as [_ H].
  apply andb_true_iff in H as [Hin _].
  apply Z.eqb_eq in Hin. exact Hin.
Qed.

Lemma res_mint_shape_reward_positive :
  forall tx ins reward,
    res_mint_shape tx ins reward = true ->
    (0 < reward)%Z.
Proof.
  intros tx ins reward H.
  unfold res_mint_shape in H.
  apply andb_true_iff in H as [Hp _].
  apply Z.gtb_lt in Hp. exact Hp.
Qed.

Lemma res_mint_shape_one_res_out :
  forall tx ins reward,
    res_mint_shape tx ins reward = true ->
    count_asset AssetRES (tx_creates tx) = 1%nat /\
    sum_asset AssetRES (tx_creates tx) = reward.
Proof.
  intros tx ins reward H.
  unfold res_mint_shape in H.
  apply andb_true_iff in H as [_ H].
  apply andb_true_iff in H as [_ H].
  apply andb_true_iff in H as [_ H].
  apply andb_true_iff in H as [Hc Hv].
  apply Nat.eqb_eq in Hc. apply Z.eqb_eq in Hv.
  split; assumption.
Qed.

Theorem mint_tx_increases_res_by_exactly_W :
  forall (tx : TxAbs) (ins : list TxOut) (reward : Z),
    res_mint_shape tx ins reward = true ->
    (sum_asset AssetRES (tx_creates tx) - sum_asset AssetRES ins = reward)%Z /\
    (sum_asset AssetRES (tx_creates tx) = reward)%Z /\
    (sum_asset AssetRES ins = 0)%Z /\
    (0 < reward)%Z.
Proof.
  intros tx ins reward H.
  pose proof (res_mint_shape_zero_res_in tx ins reward H) as Hin.
  pose proof (res_mint_shape_one_res_out tx ins reward H) as [Hc Hv].
  pose proof (res_mint_shape_reward_positive tx ins reward H) as Hp.
  repeat split; lia.
Qed.

Theorem mint_tx_cannot_increase_res_by_more_than_W :
  forall (tx : TxAbs) (ins : list TxOut) (reward : Z),
    res_mint_shape tx ins reward = true ->
    (sum_asset AssetRES (tx_creates tx) - sum_asset AssetRES ins <= reward)%Z.
Proof.
  intros tx ins reward H.
  pose proof (mint_tx_increases_res_by_exactly_W tx ins reward H) as [Heq _].
  lia.
Qed.

Theorem scheduled_mint_increases_res_by_exactly_W :
  forall (tx : TxAbs) (ins : list TxOut) (reward : Z),
    is_scheduled_mint_shape tx ins reward = true ->
    (sum_asset AssetRES (tx_creates tx) - sum_asset AssetRES ins = reward)%Z /\
    (sum_asset AssetRES (tx_creates tx) = reward)%Z /\
    (sum_asset AssetRES ins = 0)%Z /\
    (0 < reward)%Z.
Proof.
  intros tx ins reward H.
  unfold is_scheduled_mint_shape in H.
  apply orb_true_iff in H as [Hor|Hcvg].
  - apply mint_tx_increases_res_by_exactly_W. exact Hor.
  - apply cvg_mint_increases_res_by_exactly_W. exact Hcvg.
Qed.

(** Mixed ORU+RES inputs cannot take the mint exception, so they cannot
    create extra RES: conservation falls back to [res_in ≥ res_out]. *)
Theorem mixed_inputs_cannot_mint_extra_res :
  forall (u : UTXOSet) (tx : TxAbs) (reward : Z) (ins : list TxOut),
    valid_dual_pool_tx u tx reward = true ->
    gather_inputs u (tx_spends tx) = Some ins ->
    (0 < sum_asset AssetRES ins)%Z ->
    (sum_asset AssetRES (tx_creates tx) <= sum_asset AssetRES ins)%Z.
Proof.
  intros u tx reward ins Hv Hg Hres.
  unfold valid_dual_pool_tx in Hv. rewrite Hg in Hv.
  destruct (is_scheduled_mint_shape tx ins reward) eqn:Hm.
  - unfold is_scheduled_mint_shape in Hm.
    apply orb_true_iff in Hm as [Hor|Hcvg].
    + pose proof (res_mint_shape_zero_res_in tx ins reward Hor) as Hz. lia.
    + pose proof (cvg_mint_shape_zero_res_in tx ins reward Hcvg) as Hz. lia.
  - apply dual_pool_conservation_on_ins in Hv. lia.
Qed.

Lemma count_asset_zero_sum :
  forall (k : AssetKind) (outs : list TxOut),
    count_asset k outs = 0%nat ->
    sum_asset k outs = 0%Z.
Proof.
  intros k outs H.
  induction outs as [|o rest IH]; simpl; [reflexivity|].
  unfold count_asset in H. simpl in H.
  destruct (asset_eqb (output_asset o) k) eqn:E.
  - simpl in H. lia.
  - unfold txout_asset_value. rewrite E. apply IH.
    unfold count_asset. exact H.
Qed.

Theorem cvg_mint_oru_balance_unchanged :
  forall (tx : TxAbs) (ins : list TxOut) (reward : Z),
    cvg_mint_shape tx ins reward = true ->
    sum_asset AssetORU ins = 0%Z /\
    sum_asset AssetORU (tx_creates tx) = 0%Z.
Proof.
  intros tx ins reward H.
  pose proof (cvg_mint_shape_oru_unchanged tx ins reward H) as [Hin Hc].
  split; [exact Hin|].
  apply count_asset_zero_sum. exact Hc.
Qed.

Theorem ordinary_tx_cannot_create_res_from_oru_only :
  forall (ins : list TxOut) (tx : TxAbs),
    valid_ordinary_dual_pool ins tx = true ->
    sum_asset AssetRES ins = 0%Z ->
    count_asset AssetRES (tx_creates tx) = 0%nat /\
    sum_asset AssetRES (tx_creates tx) = 0%Z.
Proof.
  intros ins tx H Hz.
  unfold valid_ordinary_dual_pool, no_oru_to_res in H.
  apply andb_true_iff in H as [_ H].
  apply andb_true_iff in H as [_ Hnone].
  rewrite Hz in Hnone.
  replace (Z.eqb 0 0) with true in Hnone by reflexivity.
  apply Nat.eqb_eq in Hnone.
  split; [exact Hnone | apply count_asset_zero_sum; exact Hnone].
Qed.

(** Mint amount is a schedule term, so it sits under [W(0)] and under the
    finite-sum identity [max_issuance_within_cap]. *)
Theorem mint_shape_respects_schedule :
  forall (tx : TxAbs) (ins : list TxOut) (h : Z),
    res_mint_shape tx ins (get_res_mint_reward h) = true ->
    (0 < get_res_mint_reward h)%Z /\
    (get_res_mint_reward h <= RES_REWARD_GENESIS)%Z /\
    (RES_REWARD_EPOCH * schedule_sum <= RES_MAX_SUPPLY)%Z.
Proof.
  intros tx ins h H.
  pose proof (res_mint_shape_reward_positive tx ins (get_res_mint_reward h) H) as Hp.
  split; [exact Hp|].
  split; [| exact max_issuance_within_cap].
  unfold get_res_mint_reward.
  destruct (Z.ltb h 0); [unfold RES_REWARD_GENESIS, RES_COIN; lia |].
  apply res_reward_for_epoch_le_genesis.
Qed.

Theorem mint_shape_at_block_height :
  forall (tx : TxAbs) (ins : list TxOut) (b : BlockAbs),
    res_mint_shape tx ins (cb_res_mint_amount b) = true ->
    (sum_asset AssetRES (tx_creates tx) = cb_res_mint_amount b)%Z /\
    (cb_res_mint_amount b <= RES_REWARD_GENESIS)%Z.
Proof.
  intros tx ins b H.
  split.
  - pose proof (res_mint_shape_one_res_out tx ins (cb_res_mint_amount b) H) as [_ Hv].
    exact Hv.
  - apply cb_res_mint_amount_le_genesis.
Qed.

(** * UTXO-RES projection (ordinary non-increasing; mint = +W(h)) *)

Definition utxo_res (u : UTXOSet) : Z :=
  sum_asset AssetRES (map snd u).

Lemma outpoint_eqb_sym :
  forall a b : OutPoint, outpoint_eqb a b = outpoint_eqb b a.
Proof.
  intros [ha ia] [hb ib]. simpl.
  rewrite Nat.eqb_sym.
  destruct (hash32_eq_dec ha hb) as [Ea|Na]; destruct (hash32_eq_dec hb ha) as [Eb|Nb];
    subst; try reflexivity; congruence.
Qed.

Lemma utxo_res_cons :
  forall (op : OutPoint) (o : TxOut) (t : UTXOSet),
    utxo_res ((op, o) :: t) = (txout_asset_value AssetRES o + utxo_res t)%Z.
Proof. intros. reflexivity. Qed.

Lemma utxo_remove_absent :
  forall (u : UTXOSet) (op : OutPoint),
    utxo_lookup u op = None ->
    utxo_remove u op = u.
Proof.
  intros u op H. unfold utxo_remove.
  induction u as [|[op' o'] t IH]; simpl.
  - reflexivity.
  - simpl in H.
    destruct (outpoint_eqb op op') eqn:E; [discriminate H|].
    destruct (outpoint_eqb op' op) eqn:E2.
    + apply outpoint_eqb_eq in E2. subst op'.
      rewrite outpoint_eqb_refl in E. discriminate E.
    + simpl. f_equal. apply IH. exact H.
Qed.

Lemma utxo_res_remove_absent :
  forall (u : UTXOSet) (op : OutPoint),
    utxo_lookup u op = None ->
    utxo_res (utxo_remove u op) = utxo_res u.
Proof.
  intros u op H. rewrite (utxo_remove_absent u op H). reflexivity.
Qed.

Lemma utxo_insert_fresh_res :
  forall (u : UTXOSet) (op : OutPoint) (o : TxOut),
    utxo_lookup u op = None ->
    utxo_res (utxo_insert u op o) =
      (utxo_res u + txout_asset_value AssetRES o)%Z.
Proof.
  intros u op o H.
  unfold utxo_insert.
  rewrite (utxo_remove_absent u op H).
  unfold utxo_res. rewrite map_app, sum_asset_app. simpl. lia.
Qed.

Lemma lookup_some_in_keys :
  forall (u : UTXOSet) (op : OutPoint) (o : TxOut),
    utxo_lookup u op = Some o ->
    In op (map fst u).
Proof.
  induction u as [|[op' o'] t IH]; intros op o H; simpl in H; try discriminate.
  destruct (outpoint_eqb op op') eqn:E.
  - apply outpoint_eqb_eq in E. subst op'. left. reflexivity.
  - right. eapply IH. exact H.
Qed.

Lemma nodup_lookup_tail_none :
  forall (op : OutPoint) (o : TxOut) (t : UTXOSet),
    NoDup (op :: map fst t) ->
    utxo_lookup t op = None.
Proof.
  intros op o t Hd.
  inversion Hd as [|x l Hni Hdt]; subst.
  destruct (utxo_lookup t op) as [o'|] eqn:L; [|reflexivity].
  exfalso. apply Hni. eapply lookup_some_in_keys. exact L.
Qed.

Lemma utxo_res_remove_unique :
  forall (u : UTXOSet) (op : OutPoint) (o : TxOut),
    NoDup (map fst u) ->
    utxo_lookup u op = Some o ->
    utxo_res (utxo_remove u op) = (utxo_res u - txout_asset_value AssetRES o)%Z.
Proof.
  induction u as [|[op' o'] t IH]; intros op o Hd Hl; simpl in Hl; [discriminate Hl|].
  inversion Hd as [|? ? Hni Hdt]; subst.
  unfold utxo_remove; simpl.
  destruct (outpoint_eqb op' op) eqn:Ef.
  - apply outpoint_eqb_eq in Ef. subst op'.
    simpl in Hl. rewrite outpoint_eqb_refl in Hl. inversion Hl; subst o'.
    simpl.
    assert (Hnone : utxo_lookup t op = None)
      by (eapply nodup_lookup_tail_none; [exact o | constructor; [exact Hni | exact Hdt]]).
    change (filter (fun p : OutPoint * TxOut => negb (outpoint_eqb (fst p) op)) t)
      with (utxo_remove t op).
    rewrite (utxo_remove_absent t op Hnone).
    rewrite utxo_res_cons. lia.
  - simpl in Hl. rewrite (outpoint_eqb_sym op op') in Hl. rewrite Ef in Hl.
    simpl.
    rewrite utxo_res_cons.
    change (filter (fun p : OutPoint * TxOut => negb (outpoint_eqb (fst p) op)) t)
      with (utxo_remove t op).
    rewrite (IH op o Hdt Hl).
    rewrite utxo_res_cons. lia.
Qed.

Lemma lookup_remove_other :
  forall (u : UTXOSet) (op q : OutPoint),
    op <> q ->
    utxo_lookup (utxo_remove u q) op = utxo_lookup u op.
Proof.
  intros u op q Hne. unfold utxo_remove.
  induction u as [|[op' o'] t IH]; simpl.
  - reflexivity.
  - destruct (outpoint_eqb op' q) eqn:Eq; simpl.
    + apply outpoint_eqb_eq in Eq. subst op'.
      destruct (outpoint_eqb op q) eqn:Eop.
      * apply outpoint_eqb_eq in Eop. subst op. contradiction.
      * exact IH.
    + destruct (outpoint_eqb op op') eqn:Eop; [reflexivity | exact IH].
Qed.

Lemma gather_inputs_remove_other :
  forall (ops : list OutPoint) (u : UTXOSet) (q : OutPoint),
    ~ In q ops ->
    gather_inputs (utxo_remove u q) ops = gather_inputs u ops.
Proof.
  induction ops as [|op rest IH]; intros u q Hni; simpl.
  - reflexivity.
  - assert (Hop : op <> q) by (intro Heq; apply Hni; left; exact Heq).
    rewrite (lookup_remove_other u op q Hop).
    destruct (utxo_lookup u op) as [o|]; [|reflexivity].
    rewrite (IH u q).
    + reflexivity.
    + intro Hin. apply Hni. right. exact Hin.
Qed.

Lemma nodup_remove_preserves :
  forall (u : UTXOSet) (q : OutPoint),
    NoDup (map fst u) ->
    NoDup (map fst (utxo_remove u q)).
Proof.
  intros u q Hd. unfold utxo_remove.
  induction u as [|[op o] t IH]; simpl.
  - constructor.
  - inversion Hd as [|x l Hni Hdt]; subst.
    destruct (outpoint_eqb op q) eqn:E; simpl.
    + apply IH. exact Hdt.
    + constructor.
      * intro Hin.
        apply in_map_iff in Hin.
        destruct Hin as [p [Hp Hinp]].
        apply filter_In in Hinp as [Hinp _].
        apply Hni. rewrite <- Hp. apply in_map. exact Hinp.
      * apply IH. exact Hdt.
Qed.

Lemma gather_inputs_cons_inv :
  forall u op rest ins,
    gather_inputs u (op :: rest) = Some ins ->
    exists o ins',
      utxo_lookup u op = Some o /\
      gather_inputs u rest = Some ins' /\
      ins = o :: ins'.
Proof.
  intros u op rest ins H. simpl in H.
  destruct (utxo_lookup u op) as [o|] eqn:L; [|discriminate H].
  destruct (gather_inputs u rest) as [ins'|] eqn:G; [|discriminate H].
  inversion H. exists o, ins'. auto.
Qed.

Lemma fold_remove_res :
  forall (ops : list OutPoint) (u : UTXOSet) (ins : list TxOut),
    NoDup (map fst u) ->
    NoDup ops ->
    gather_inputs u ops = Some ins ->
    utxo_res (fold_left (fun u' q => utxo_remove u' q) ops u) =
      (utxo_res u - sum_asset AssetRES ins)%Z.
Proof.
  induction ops as [|op rest IH]; intros u ins Hu Ho Hg; simpl.
  - inversion Hg. subst ins. simpl. lia.
  - apply gather_inputs_cons_inv in Hg.
    destruct Hg as [o [ins' [Hl [Hg' Hins]]]]. subst ins.
    inversion Ho as [|x l Hni Hdt]; subst.
    simpl.
    assert (Hremu : NoDup (map fst (utxo_remove u op)))
      by (apply nodup_remove_preserves; exact Hu).
    assert (Hgrem : gather_inputs (utxo_remove u op) rest = Some ins').
    { rewrite gather_inputs_remove_other; [exact Hg'|exact Hni]. }
    rewrite (IH (utxo_remove u op) ins' Hremu Hdt Hgrem).
    rewrite (utxo_res_remove_unique u op o Hu Hl).
    simpl. lia.
Qed.

Lemma fold_create_fresh_res :
  forall (creates : list TxOut) (u : UTXOSet) (hid : Hash32) (base : nat),
    (forall i, (i < length creates)%nat ->
               utxo_lookup u (MkOutPoint hid (base + i)) = None) ->
    utxo_res (fold_create_outputs creates u hid base) =
      (utxo_res u + sum_asset AssetRES creates)%Z.
Proof.
  unfold fold_create_outputs.
  induction creates as [|o rest IH]; intros u hid base Hfresh; simpl.
  - lia.
  - simpl.
    assert (H0 : utxo_lookup u (MkOutPoint hid base) = None).
    { specialize (Hfresh 0%nat). rewrite Nat.add_0_r in Hfresh.
      apply Hfresh. simpl. lia. }
    rewrite (IH (utxo_insert u (MkOutPoint hid base) o) hid (S base)).
    + rewrite (utxo_insert_fresh_res u (MkOutPoint hid base) o H0). lia.
    + intros i Hi.
      apply utxo_lookup_insert_stable.
      * specialize (Hfresh (S i)).
        replace (base + S i)%nat with (S base + i)%nat in Hfresh by lia.
        apply Hfresh. simpl. lia.
      * intros Heq. injection Heq as Hb. lia.
Qed.

Lemma gather_inputs_of_forallb :
  forall (u : UTXOSet) (ops : list OutPoint),
    forallb (fun op =>
               match utxo_lookup u op with Some _ => true | None => false end)
            ops = true ->
    exists ins, gather_inputs u ops = Some ins.
Proof.
  intros u ops H.
  induction ops as [|op rest IH]; simpl in *.
  - exists nil. reflexivity.
  - apply andb_true_iff in H as [Hhead Hrest].
    destruct (utxo_lookup u op) as [o|] eqn:L; [|discriminate Hhead].
    destruct (IH Hrest) as [ins Hg].
    exists (o :: ins). rewrite Hg. reflexivity.
Qed.

Lemma apply_tx_utxo_gathers :
  forall (u : UTXOSet) (tx : TxAbs) (idx : nat) (u' : UTXOSet),
    apply_tx_utxo u tx idx = Some u' ->
    exists ins, gather_inputs u (tx_spends tx) = Some ins.
Proof.
  intros u tx idx u' H.
  unfold apply_tx_utxo in H. cbv zeta in H.
  match type of H with
  | (if ?b then _ else _) = _ => destruct b eqn:Hok
  end; [|discriminate H].
  apply gather_inputs_of_forallb. exact Hok.
Qed.

Lemma apply_tx_utxo_structure :
  forall (u : UTXOSet) (tx : TxAbs) (idx : nat) (u' : UTXOSet),
    apply_tx_utxo u tx idx = Some u' ->
    u' = fold_create_outputs (tx_creates tx)
           (fold_left (fun u0 op => utxo_remove u0 op) (tx_spends tx) u)
           (tx_id tx) 0.
Proof.
  intros u tx idx u' H.
  unfold apply_tx_utxo in H. cbv zeta in H.
  match type of H with
  | (if ?b then _ else _) = _ => destruct b eqn:Hok
  end; [|discriminate H].
  inversion H. reflexivity.
Qed.

Theorem ordinary_tx_utxo_res_non_increasing :
  forall (u : UTXOSet) (tx : TxAbs) (idx : nat) (u' : UTXOSet),
    NoDup (map fst u) ->
    NoDup (tx_spends tx) ->
    (forall i, (i < length (tx_creates tx))%nat ->
               utxo_lookup
                 (fold_left (fun u0 op => utxo_remove u0 op) (tx_spends tx) u)
                 (MkOutPoint (tx_id tx) i) = None) ->
    valid_non_mint_tx u tx = true ->
    apply_tx_utxo u tx idx = Some u' ->
    (utxo_res u' <= utxo_res u)%Z.
Proof.
  intros u tx idx u' Hu Hs Hfresh Hval Happ.
  pose proof (apply_tx_utxo_gathers u tx idx u' Happ) as [ins Hg].
  unfold valid_non_mint_tx in Hval. rewrite Hg in Hval.
  pose proof (dual_pool_conservation_on_ins ins tx Hval) as Hge.
  rewrite (apply_tx_utxo_structure u tx idx u' Happ).
  rewrite (fold_create_fresh_res (tx_creates tx)
             (fold_left (fun u0 op => utxo_remove u0 op) (tx_spends tx) u)
             (tx_id tx) 0).
  - rewrite (fold_remove_res (tx_spends tx) u ins Hu Hs Hg). lia.
  - intros i Hi. rewrite Nat.add_0_l. apply Hfresh. exact Hi.
Qed.

Theorem mint_tx_utxo_res_plus_W :
  forall (u : UTXOSet) (tx : TxAbs) (idx : nat) (u' : UTXOSet)
         (ins : list TxOut) (reward : Z),
    NoDup (map fst u) ->
    NoDup (tx_spends tx) ->
    (forall i, (i < length (tx_creates tx))%nat ->
               utxo_lookup
                 (fold_left (fun u0 op => utxo_remove u0 op) (tx_spends tx) u)
                 (MkOutPoint (tx_id tx) i) = None) ->
    gather_inputs u (tx_spends tx) = Some ins ->
    is_scheduled_mint_shape tx ins reward = true ->
    apply_tx_utxo u tx idx = Some u' ->
    utxo_res u' = (utxo_res u + reward)%Z.
Proof.
  intros u tx idx u' ins reward Hu Hs Hfresh Hg Hm Happ.
  pose proof (scheduled_mint_increases_res_by_exactly_W tx ins reward Hm)
    as [Hd [Hout [Hin _]]].
  rewrite (apply_tx_utxo_structure u tx idx u' Happ).
  rewrite (fold_create_fresh_res (tx_creates tx)
             (fold_left (fun u0 op => utxo_remove u0 op) (tx_spends tx) u)
             (tx_id tx) 0).
  - rewrite (fold_remove_res (tx_spends tx) u ins Hu Hs Hg). lia.
  - intros i Hi. rewrite Nat.add_0_l. apply Hfresh. exact Hi.
Qed.

(** * apply_tx enforces the dual-pool predicate

    Ordinary [res_in ≥ res_out] (and mint-shape of exactly [W(h)]) is now a
    success condition of [apply_tx], hence of [apply_txs] / [apply_block_body].
    Not a C++ refinement. *)

Theorem apply_tx_ordinary_conserves_res :
  forall (s : ReplayState) (tx : TxAbs) (idx : nat) (s' : ReplayState)
         (ins : list TxOut),
    apply_tx s tx idx = Some s' ->
    gather_inputs (rs_utxo s) (tx_spends tx) = Some ins ->
    is_scheduled_mint_shape tx ins (apply_tx_mint_reward s) = false ->
    (sum_asset AssetRES ins >= sum_asset AssetRES (tx_creates tx))%Z.
Proof.
  intros s tx idx s' ins Happ Hg Hm.
  pose proof (apply_tx_succeeds_implies_dual_pool s tx idx s' Happ) as Hv.
  unfold valid_dual_pool_tx in Hv. rewrite Hg in Hv. rewrite Hm in Hv.
  apply dual_pool_conservation_on_ins in Hv. exact Hv.
Qed.

Theorem apply_tx_mint_increases_utxo_res :
  forall (s : ReplayState) (tx : TxAbs) (idx : nat) (s' : ReplayState)
         (ins : list TxOut),
    NoDup (map fst (rs_utxo s)) ->
    NoDup (tx_spends tx) ->
    (forall i, (i < length (tx_creates tx))%nat ->
               utxo_lookup
                 (fold_left (fun u0 op => utxo_remove u0 op) (tx_spends tx)
                            (rs_utxo s))
                 (MkOutPoint (tx_id tx) i) = None) ->
    gather_inputs (rs_utxo s) (tx_spends tx) = Some ins ->
    is_scheduled_mint_shape tx ins (apply_tx_mint_reward s) = true ->
    apply_tx s tx idx = Some s' ->
    utxo_res (rs_utxo s') =
      (utxo_res (rs_utxo s) + apply_tx_mint_reward s)%Z.
Proof.
  intros s tx idx s' ins Hu Hs Hfresh Hg Hm Happ.
  apply apply_tx_some_inv in Happ.
  destruct Happ as [u' [Hu' [_ [_ Heq]]]]. subst s'.
  cbn [rs_utxo replay_set_utxo].
  eapply mint_tx_utxo_res_plus_W; eauto.
Qed.

(** * Block-body RES: net increase only via scheduled mint-shape *)

Definition tx_res_delta (ins : list TxOut) (tx : TxAbs) : Z :=
  (sum_asset AssetRES (tx_creates tx) - sum_asset AssetRES ins)%Z.

Lemma apply_tx_gathers :
  forall (s : ReplayState) (tx : TxAbs) (idx : nat) (s' : ReplayState),
    apply_tx s tx idx = Some s' ->
    exists ins, gather_inputs (rs_utxo s) (tx_spends tx) = Some ins.
Proof.
  intros s tx idx s' H.
  apply apply_tx_some_inv in H.
  destruct H as [u' [Hu' _]].
  eapply apply_tx_utxo_gathers. exact Hu'.
Qed.

Theorem apply_tx_covenant_spend_cannot_recreate_cvg :
  forall (s : ReplayState) (tx : TxAbs) (idx : nat) (s' : ReplayState)
         (ins : list TxOut),
    apply_tx s tx idx = Some s' ->
    gather_inputs (rs_utxo s) (tx_spends tx) = Some ins ->
    count_cvg ins = 1%nat ->
    count_cvg (tx_creates tx) = 0%nat.
Proof.
  intros s tx idx s' ins Happ Hg Hc.
  pose proof (apply_tx_succeeds_implies_cvg_spend s tx idx s' Happ) as Hcvg.
  unfold valid_cvg_spend_tx in Hcvg. rewrite Hg in Hcvg.
  apply (covenant_spend_cannot_recreate_cvg ins tx Hcvg Hc).
Qed.

Theorem apply_tx_cvg_mint_oru_conserved :
  forall (s : ReplayState) (tx : TxAbs) (idx : nat) (s' : ReplayState)
         (ins : list TxOut),
    apply_tx s tx idx = Some s' ->
    gather_inputs (rs_utxo s) (tx_spends tx) = Some ins ->
    cvg_mint_shape tx ins (apply_tx_mint_reward s) = true ->
    sum_asset AssetORU ins = 0%Z /\
    sum_asset AssetORU (tx_creates tx) = 0%Z.
Proof.
  intros s tx idx s' ins Happ Hg Hm.
  apply (cvg_mint_oru_balance_unchanged tx ins (apply_tx_mint_reward s) Hm).
Qed.

Theorem apply_tx_res_delta_classified :
  forall (s : ReplayState) (tx : TxAbs) (idx : nat) (s' : ReplayState)
         (ins : list TxOut),
    apply_tx s tx idx = Some s' ->
    gather_inputs (rs_utxo s) (tx_spends tx) = Some ins ->
    (is_scheduled_mint_shape tx ins (apply_tx_mint_reward s) = true /\
     tx_res_delta ins tx = apply_tx_mint_reward s) \/
    (is_scheduled_mint_shape tx ins (apply_tx_mint_reward s) = false /\
     (tx_res_delta ins tx <= 0)%Z).
Proof.
  intros s tx idx s' ins Happ Hg.
  pose proof (apply_tx_succeeds_implies_dual_pool s tx idx s' Happ) as Hv.
  unfold valid_dual_pool_tx in Hv. rewrite Hg in Hv.
  destruct (is_scheduled_mint_shape tx ins (apply_tx_mint_reward s)) eqn:Hm.
  - left. split; [reflexivity|].
    pose proof (scheduled_mint_increases_res_by_exactly_W tx ins
                  (apply_tx_mint_reward s) Hm) as [Hd _].
    unfold tx_res_delta. exact Hd.
  - right. split; [reflexivity|].
    apply dual_pool_conservation_on_ins in Hv.
    unfold tx_res_delta. lia.
Qed.

Theorem apply_tx_res_net_from_scheduled_mint :
  forall (s : ReplayState) (tx : TxAbs) (idx : nat) (s' : ReplayState),
    apply_tx s tx idx = Some s' ->
    exists ins : list TxOut,
      gather_inputs (rs_utxo s) (tx_spends tx) = Some ins /\
      ((tx_res_delta ins tx <= 0)%Z \/
       tx_res_delta ins tx = apply_tx_mint_reward s).
Proof.
  intros s tx idx s' Happ.
  destruct (apply_tx_gathers s tx idx s' Happ) as [ins Hg].
  exists ins. split; [exact Hg|].
  destruct (apply_tx_res_delta_classified s tx idx s' ins Happ Hg) as
    [[_ Heq] | [_ Hle]].
  - right. exact Heq.
  - left. exact Hle.
Qed.

(** Every body tx of a successful [apply_txs] passed [valid_dual_pool_tx]
    at the UTXO it was applied against, with the same [W] as the start state. *)
Fixpoint apply_txs_dual_pool (s : ReplayState) (txs : list TxAbs) (idx : nat)
                             (s_end : ReplayState) : Prop :=
  match txs with
  | nil => s_end = s
  | t :: rest =>
      valid_dual_pool_tx (rs_utxo s) t (apply_tx_mint_reward s) = true /\
      exists s1 : ReplayState,
        apply_tx s t idx = Some s1 /\
        apply_txs_dual_pool s1 rest (S idx) s_end
  end.

Theorem apply_txs_succeeds_implies_dual_pool :
  forall (s : ReplayState) (txs : list TxAbs) (idx : nat) (s' : ReplayState),
    apply_txs s txs idx = Some s' ->
    apply_txs_dual_pool s txs idx s'.
Proof.
  intros s txs idx s'. revert s idx.
  induction txs as [|t rest IH]; intros s idx H; simpl in *.
  - injection H as ->. reflexivity.
  - apply apply_txs_cons_inv in H.
    destruct H as [s1 [Hat Hrest]].
    split.
    + eapply apply_tx_succeeds_implies_dual_pool. exact Hat.
    + exists s1. split; [exact Hat | apply IH; exact Hrest].
Qed.

Theorem apply_block_body_res_from_scheduled_mint :
  forall (s : ReplayState) (b : BlockAbs) (s' : ReplayState),
    apply_block_body s b = Some s' ->
    exists s_tx : ReplayState,
      apply_txs s (blk_txs b) 0 = Some s_tx /\
      apply_txs_dual_pool s (blk_txs b) 0 s_tx.
Proof.
  intros s b s' H.
  unfold apply_block_body in H.
  destruct (apply_txs s (blk_txs b) 0) as [s_tx|] eqn:Ht; [|discriminate H].
  exists s_tx. split; [reflexivity|].
  apply apply_txs_succeeds_implies_dual_pool. exact Ht.
Qed.

Lemma state_of_snapshot_roundtrip :
  forall s : ReplayState,
    state_of_snapshot (snapshot_of_state s) (rs_history s) = s.
Proof. intros []. reflexivity. Qed.

Lemma apply_block_body_rollback_1 :
  forall (s : ReplayState) (b : BlockAbs) (s' : ReplayState),
    apply_block_body s b = Some s' ->
    rollback s' 1 = Some s.
Proof.
  intros s b s' H.
  unfold apply_block_body in H.
  destruct (apply_txs s (blk_txs b) 0) as [s_tx|] eqn:Ht; [|discriminate H].
  inversion H as [Hs']; clear H. subst s'.
  pose proof (apply_txs_preserves_history s (blk_txs b) 0 s_tx Ht) as Hh.
  unfold rollback, push_history, apply_supply_delta,
         apply_connectblock_supply_delta, replay_set_chain.
  simpl. rewrite Hh.
  rewrite state_of_snapshot_roundtrip. reflexivity.
Qed.

(** Joint: accepted block ⇒ body dual-pool at [W(h)], scalar supply is the
    protocol mint, and rollback-1 restores the pre-state. *)
Theorem apply_block_dual_pool_supply_rollback :
  forall (s : ReplayState) (b : BlockAbs) (s' : ReplayState),
    apply_block s b = Some s' ->
    supply_evolution_eq s s' b /\
    apply_tx_mint_reward s = cb_res_mint_amount b /\
    (exists s_tx : ReplayState,
       apply_txs s (blk_txs b) 0 = Some s_tx /\
       apply_txs_dual_pool s (blk_txs b) 0 s_tx) /\
    rollback s' 1 = Some s.
Proof.
  intros s b s' H.
  unfold apply_block in H.
  destruct (valid_block_connect s b) eqn:Hc; [|discriminate H].
  split; [|split; [|split]].
  - apply apply_block_supply_correct. unfold apply_block. rewrite Hc. exact H.
  - apply apply_tx_mint_reward_connecting.
    apply valid_block_connect_inv in Hc. apply Hc.
  - exact (apply_block_body_res_from_scheduled_mint s b s' H).
  - exact (apply_block_body_rollback_1 s b s' H).
Qed.

Theorem apply_block_supply_deterministic_with_dual_pool :
  forall (s : ReplayState) (b : BlockAbs) (s1 s2 : ReplayState),
    apply_block s b = Some s1 ->
    apply_block s b = Some s2 ->
    total_supply s1 = total_supply s2 /\
    rollback s1 1 = Some s /\
    rollback s2 1 = Some s.
Proof.
  intros s b s1 s2 H1 H2.
  split.
  - eapply supply_deterministic; eassumption.
  - split.
    + apply apply_block_dual_pool_supply_rollback in H1.
      destruct H1 as [_ [_ [_ Hrb]]]. exact Hrb.
    + apply apply_block_dual_pool_supply_rollback in H2.
      destruct H2 as [_ [_ [_ Hrb]]]. exact Hrb.
Qed.

Lemma apply_txs_preserves_mint_reward :
  forall (s : ReplayState) (txs : list TxAbs) (idx : nat) (s' : ReplayState),
    apply_txs s txs idx = Some s' ->
    apply_tx_mint_reward s' = apply_tx_mint_reward s.
Proof.
  intros s txs idx s' H.
  unfold apply_tx_mint_reward.
  rewrite (apply_txs_preserves_chain s txs idx s' H). reflexivity.
Qed.

Lemma apply_tx_mint_reward_nonneg :
  forall s : ReplayState, (0 <= apply_tx_mint_reward s)%Z.
Proof.
  intros s. unfold apply_tx_mint_reward. apply get_res_mint_reward_nonneg.
Qed.

(** Body RES net and mint-shape count along a successful [apply_txs].
    Ordinary txs contribute [tx_res_delta ≤ 0]; mint-shape contributes exactly [W]. *)
Fixpoint txs_res_net_and_mints (s : ReplayState) (txs : list TxAbs) (idx : nat)
  : option (Z * nat) :=
  match txs with
  | nil => Some (0%Z, 0%nat)
  | t :: rest =>
      match gather_inputs (rs_utxo s) (tx_spends t) with
      | None => None
      | Some ins =>
          match apply_tx s t idx with
          | None => None
          | Some s1 =>
              match txs_res_net_and_mints s1 rest (S idx) with
              | None => None
              | Some (d, n) =>
                  let W := apply_tx_mint_reward s in
                  if is_scheduled_mint_shape t ins W then
                    Some ((W + d)%Z, S n)
                  else
                    Some ((tx_res_delta ins t + d)%Z, n)
              end
          end
      end
  end.

Theorem apply_txs_res_net_from_scheduled_mint :
  forall (s : ReplayState) (txs : list TxAbs) (idx : nat) (s' : ReplayState),
    apply_txs s txs idx = Some s' ->
    exists (d : Z) (n : nat),
      txs_res_net_and_mints s txs idx = Some (d, n) /\
      (d <= Z.of_nat n * apply_tx_mint_reward s)%Z /\
      (n = 0%nat -> (d <= 0)%Z).
Proof.
  intros s txs idx s'. revert s idx s'.
  induction txs as [|t rest IH]; intros s idx s' Happ.
  - simpl in Happ. injection Happ as <-.
    exists 0%Z, 0%nat. simpl. repeat split; lia.
  - apply apply_txs_cons_inv in Happ.
    destruct Happ as [s1 [Hat Hrest]].
    destruct (apply_tx_gathers s t idx s1 Hat) as [ins Hg].
    destruct (IH s1 (S idx) s' Hrest) as [d [n [Hnet [Hb Hn0]]]].
    pose proof (apply_tx_preserves_mint_reward s t idx s1 Hat) as HW1.
    rewrite HW1 in Hb.
    pose proof (apply_tx_mint_reward_nonneg s) as HWnn.
    destruct (apply_tx_res_delta_classified s t idx s1 ins Hat Hg)
      as [[Hm Heq] | [Hm Hle]].
    + exists (apply_tx_mint_reward s + d)%Z, (S n).
      assert (Hbnd : (apply_tx_mint_reward s + d <=
                       Z.of_nat (S n) * apply_tx_mint_reward s)%Z).
      { rewrite Nat2Z.inj_succ. rewrite Z.mul_succ_l. lia. }
      simpl. rewrite Hg, Hat, Hnet, Hm.
      split; [reflexivity|]. split; [exact Hbnd|].
      intros Hsn. discriminate Hsn.
    + exists (tx_res_delta ins t + d)%Z, n.
      simpl. rewrite Hg, Hat, Hnet, Hm.
      split; [reflexivity|]. split; [lia|].
      intros Hn. pose proof (Hn0 Hn) as Hd0. lia.
Qed.

(** Block-level invariant: after a successful connect, body RES net is
    ordinary slack (≤ 0) plus [n] scheduled mints of [W(h)]. *)
Theorem apply_block_res_invariant :
  forall (s : ReplayState) (b : BlockAbs) (s' : ReplayState),
    apply_block s b = Some s' ->
    supply_evolution_eq s s' b /\
    apply_tx_mint_reward s = cb_res_mint_amount b /\
    rollback s' 1 = Some s /\
    exists (s_tx : ReplayState) (d : Z) (n : nat),
      apply_txs s (blk_txs b) 0 = Some s_tx /\
      apply_txs_dual_pool s (blk_txs b) 0 s_tx /\
      txs_res_net_and_mints s (blk_txs b) 0 = Some (d, n) /\
      (d <= Z.of_nat n * cb_res_mint_amount b)%Z /\
      (n = 0%nat -> (d <= 0)%Z).
Proof.
  intros s b s' H.
  pose proof (apply_block_dual_pool_supply_rollback s b s' H)
    as [Hev [HW [Hdp Hrb]]].
  destruct Hdp as [s_tx [Ht Htrace]].
  destruct (apply_txs_res_net_from_scheduled_mint s (blk_txs b) 0 s_tx Ht)
    as [d [n [Hnet [Hb Hn0]]]].
  rewrite HW in Hb.
  split; [exact Hev|]. split; [exact HW|]. split; [exact Hrb|].
  exists s_tx, d, n. repeat split; assumption.
Qed.

Theorem valid_block_connect_res_invariant :
  forall (s : ReplayState) (b : BlockAbs) (s' : ReplayState),
    valid_block_connect s b = true ->
    apply_block_body s b = Some s' ->
    supply_evolution_eq s s' b /\
    apply_tx_mint_reward s = cb_res_mint_amount b /\
    rollback s' 1 = Some s /\
    exists (s_tx : ReplayState) (d : Z) (n : nat),
      apply_txs s (blk_txs b) 0 = Some s_tx /\
      apply_txs_dual_pool s (blk_txs b) 0 s_tx /\
      txs_res_net_and_mints s (blk_txs b) 0 = Some (d, n) /\
      (d <= Z.of_nat n * cb_res_mint_amount b)%Z /\
      (n = 0%nat -> (d <= 0)%Z).
Proof.
  intros s b s' Hc Hb.
  apply apply_block_res_invariant.
  apply (valid_block_connect_apply_block s b s' Hc Hb).
Qed.

(** Citable projection of [apply_block_res_invariant]: body RES net is
    bounded by scheduled mint count × [W(h)]; a mint-free body cannot
    increase RES. TASK-FV-MODEL-SMALL-LIFT-FOLLOWUPS-001. *)
Theorem apply_block_res_net_bounded_by_scheduled_mints :
  forall (s : ReplayState) (b : BlockAbs) (s' : ReplayState),
    apply_block s b = Some s' ->
    exists (d : Z) (n : nat),
      txs_res_net_and_mints s (blk_txs b) 0 = Some (d, n) /\
      (d <= Z.of_nat n * cb_res_mint_amount b)%Z /\
      (n = 0%nat -> (d <= 0)%Z).
Proof.
  intros s b s' H.
  destruct (apply_block_res_invariant s b s' H) as [_ [_ [_ Hex]]].
  destruct Hex as [_ [d [n [_ [_ [Hnet [Hb Hn0]]]]]]].
  exists d, n. repeat split; assumption.
Qed.

Lemma skipn_1_skipn :
  forall (A : Type) (n : nat) (l : list A),
    skipn 1 (skipn n l) = skipn (S n) l.
Proof.
  intros A n. induction n as [|n IH]; intros l.
  - reflexivity.
  - destruct l as [|x t]; simpl; [reflexivity | apply IH].
Qed.

Lemma rollback_from_history_SS :
  forall (hist : list ReplaySnapshot) (k : nat),
    rollback_from_history hist (S (S k)) =
      match hist with
      | nil => None
      | _ :: t => rollback_from_history t (S k)
      end.
Proof.
  intros hist k. destruct k as [|k']; destruct hist; reflexivity.
Qed.

Lemma rollback_from_history_succ :
  forall (n : nat) (hist : list ReplaySnapshot),
    (0 < n)%nat ->
    rollback_from_history hist (S n) = rollback_from_history (skipn n hist) 1.
Proof.
  intros n. induction n as [|n IH]; intros hist Hn; [lia|].
  destruct n as [|n'].
  - destruct hist as [|x t]; reflexivity.
  - rewrite rollback_from_history_SS.
    destruct hist as [|x t]; [reflexivity|].
    simpl skipn.
    apply IH. apply Nat.lt_0_succ.
Qed.

Lemma rollback_unfold_S :
  forall (s : ReplayState) (n : nat),
    rollback s (S n) =
      match rollback_from_history (rs_history s) (S n) with
      | None => None
      | Some snap => Some (state_of_snapshot snap (skipn (S n) (rs_history s)))
      end.
Proof.
  intros s n. destruct n; reflexivity.
Qed.

Lemma rollback_succ :
  forall (s : ReplayState) (k : nat) (s_mid s_old : ReplayState),
    rollback s k = Some s_mid ->
    rollback s_mid 1 = Some s_old ->
    rollback s (S k) = Some s_old.
Proof.
  intros s k s_mid s_old Hk H1.
  destruct k as [|k'].
  - simpl in Hk. inversion Hk. subst s_mid. exact H1.
  - rewrite rollback_unfold_S in Hk.
    destruct (rollback_from_history (rs_history s) (S k')) as [snap|] eqn:Hs;
      [|discriminate Hk].
    injection Hk as <-.
    rewrite rollback_unfold_S in H1.
    unfold state_of_snapshot in H1.
    cbn [rs_history] in H1.
    change (match rs_history s with
            | nil => nil
            | _ :: l => skipn k' l
            end) with (skipn (S k') (rs_history s)) in H1.
    rewrite skipn_1_skipn in H1.
    rewrite rollback_unfold_S.
    rewrite (rollback_from_history_succ (S k') (rs_history s)) by lia.
    unfold state_of_snapshot.
    exact H1.
Qed.

Theorem fold_apply_blocks_rollback :
  forall (s : ReplayState) (bs : list BlockAbs) (s' : ReplayState),
    fold_apply_blocks s bs = Some s' ->
    rollback s' (length bs) = Some s.
Proof.
  intros s bs. revert s.
  induction bs as [|b rest IH]; intros s s' H.
  - simpl in H. inversion H. subst s'. reflexivity.
  - simpl in H.
    destruct (apply_block s b) as [s1|] eqn:Hb; [|discriminate H].
    pose proof (IH s1 s' H) as Hrest.
    pose proof (apply_block_dual_pool_supply_rollback s b s1 Hb)
      as [_ [_ [_ Hrb1]]].
    simpl length.
    eapply rollback_succ; [exact Hrest | exact Hrb1].
Qed.

Fixpoint fold_blocks_res_invariant (s : ReplayState) (bs : list BlockAbs)
                                   (s_end : ReplayState) : Prop :=
  match bs with
  | nil => s_end = s
  | b :: rest =>
      exists (s1 : ReplayState) (d : Z) (n : nat),
        apply_block s b = Some s1 /\
        txs_res_net_and_mints s (blk_txs b) 0 = Some (d, n) /\
        (d <= Z.of_nat n * cb_res_mint_amount b)%Z /\
        (n = 0%nat -> (d <= 0)%Z) /\
        fold_blocks_res_invariant s1 rest s_end
  end.

Theorem fold_apply_blocks_res_invariant :
  forall (s : ReplayState) (bs : list BlockAbs) (s' : ReplayState),
    fold_apply_blocks s bs = Some s' ->
    fold_blocks_res_invariant s bs s'.
Proof.
  intros s bs. revert s.
  induction bs as [|b rest IH]; intros s s' H; simpl in *.
  - inversion H. subst s'. reflexivity.
  - destruct (apply_block s b) as [s1|] eqn:Hb; [|discriminate H].
    destruct (apply_block_res_invariant s b s1 Hb)
      as [_ [_ [_ [s_tx [d [n [Ht [_ [Hnet [Hbnd Hn0]]]]]]]]]].
    exists s1, d, n.
    repeat split; try assumption.
    apply IH. exact H.
Qed.

Theorem fold_apply_blocks_res_supply_deterministic :
  forall (s : ReplayState) (bs : list BlockAbs) (s1 s2 : ReplayState),
    fold_apply_blocks s bs = Some s1 ->
    fold_apply_blocks s bs = Some s2 ->
    rs_res_supply s1 = rs_res_supply s2 /\
    total_supply s1 = total_supply s2.
Proof.
  intros s bs s1 s2 H1 H2. rewrite H1 in H2. inversion H2. subst s2.
  split; reflexivity.
Qed.

(** Multi-block joint: dual-pool body net, RES/total supply determinism,
    and rollback of the whole suffix restores the pre-state. *)
Theorem fold_apply_blocks_dual_pool_supply_rollback :
  forall (s : ReplayState) (bs : list BlockAbs) (s' : ReplayState),
    fold_apply_blocks s bs = Some s' ->
    fold_blocks_res_invariant s bs s' /\
    rollback s' (length bs) = Some s /\
    (forall s2 : ReplayState,
       fold_apply_blocks s bs = Some s2 ->
       rs_res_supply s' = rs_res_supply s2 /\
       total_supply s' = total_supply s2).
Proof.
  intros s bs s' H.
  split; [|split].
  - apply fold_apply_blocks_res_invariant. exact H.
  - apply fold_apply_blocks_rollback. exact H.
  - intros s2 H2.
    eapply fold_apply_blocks_res_supply_deterministic; eassumption.
Qed.

Theorem fold_apply_blocks_dual_pool_replay_deterministic :
  forall (s : ReplayState) (bs : list BlockAbs) (s1 s2 : ReplayState),
    valid_block_sequence s bs = true ->
    fold_apply_blocks s bs = Some s1 ->
    fold_apply_blocks s bs = Some s2 ->
    s1 = s2 /\
    fold_blocks_res_invariant s bs s1 /\
    rollback s1 (length bs) = Some s /\
    rs_res_supply s1 = rs_res_supply s2 /\
    total_supply s1 = total_supply s2.
Proof.
  intros s bs s1 s2 Hseq H1 H2.
  pose proof (Replay_Determinism s bs s1 s2 Hseq H1 H2) as Heq.
  subst s2.
  split; [reflexivity|].
  split; [apply fold_apply_blocks_res_invariant; exact H1|].
  split; [apply fold_apply_blocks_rollback; exact H1|].
  split; reflexivity.
Qed.

(** ** Golden checks: reject inflation, admit scheduled mint, reject H-1 mint *)

Definition dp_res (v : Z) : TxOut := mk_res v.

Definition dp_oru (v : Z) : TxOut := mk_oru v.

Definition dp_op : OutPoint := MkOutPoint hash32_placeholder 0.

Definition dp_txid : Hash32 := 1 :: hash32_placeholder.

Definition dp_state (u : UTXOSet) (h : nat) : ReplayState :=
  {| rs_utxo := u;
     rs_chain :=
       {| cm_height := h;
          cm_chainwork := 0%Z;
          cm_tip_hash := hash32_placeholder |};
     rs_oru_supply := 0%Z;
     rs_res_supply := 0%Z;
     rs_history := [] |}.

Definition dp_inflate_tx : TxAbs :=
  {| tx_id := dp_txid;
     tx_spends := [dp_op];
     tx_creates := [dp_res 2];
     tx_pcorb := None |}.

Definition dp_mint_tx : TxAbs :=
  {| tx_id := dp_txid;
     tx_spends := [dp_op];
     tx_creates := [dp_res RES_REWARD_GENESIS];
     tx_pcorb := None |}.

Lemma dp_gather_singleton :
  forall o : TxOut, gather_inputs [(dp_op, o)] [dp_op] = Some [o].
Proof.
  intros o. unfold gather_inputs, utxo_lookup.
  rewrite outpoint_eqb_refl. reflexivity.
Qed.

Lemma dp_inflate_not_dual_pool :
  valid_dual_pool_tx [(dp_op, dp_res 1)] dp_inflate_tx
    (apply_tx_mint_reward (dp_state [(dp_op, dp_res 1)] 0)) = false.
Proof.
  unfold valid_dual_pool_tx, dp_inflate_tx.
  rewrite dp_gather_singleton.
  unfold res_mint_shape, valid_ordinary_dual_pool, no_res_to_oru, no_oru_to_res.
  unfold sum_asset, txout_asset_value, count_asset, dp_res.
  vm_compute.
  reflexivity.
Qed.

Lemma apply_tx_rejects_res_inflation :
  apply_tx (dp_state [(dp_op, dp_res 1)] 0) dp_inflate_tx 0 = None.
Proof.
  apply apply_tx_rejects_without_dual_pool.
  exact dp_inflate_not_dual_pool.
Qed.

(** H-1: exhausted schedule pays [W = 0]; mint-shape is then unavailable. *)
Lemma exhausted_schedule_reward_zero :
  get_res_mint_reward (RES_REWARD_EPOCH * Z.of_nat (S RES_REWARD_LAST_EPOCH)) = 0%Z.
Proof. vm_compute. reflexivity. Qed.

Lemma apply_tx_mint_reward_exhausted :
  forall s : ReplayState,
    Z.of_nat (S (cm_height (rs_chain s))) =
      (RES_REWARD_EPOCH * Z.of_nat (S RES_REWARD_LAST_EPOCH))%Z ->
    apply_tx_mint_reward s = 0%Z.
Proof.
  intros s Hh.
  unfold apply_tx_mint_reward. rewrite Hh. exact exhausted_schedule_reward_zero.
Qed.

Lemma dp_mint_not_dual_pool_zero_reward :
  valid_dual_pool_tx [(dp_op, dp_oru 1)] dp_mint_tx 0%Z = false.
Proof.
  unfold valid_dual_pool_tx, dp_mint_tx.
  rewrite dp_gather_singleton.
  unfold res_mint_shape, valid_ordinary_dual_pool, no_res_to_oru, no_oru_to_res.
  unfold sum_asset, txout_asset_value, count_asset, dp_oru, dp_res.
  vm_compute.
  reflexivity.
Qed.

Lemma apply_tx_rejects_h1_unscheduled_mint :
  forall s : ReplayState,
    rs_utxo s = [(dp_op, dp_oru 1)] ->
    apply_tx_mint_reward s = 0%Z ->
    apply_tx s dp_mint_tx 0 = None.
Proof.
  intros s Hu Hr.
  apply apply_tx_rejects_without_dual_pool.
  rewrite Hu, Hr.
  exact dp_mint_not_dual_pool_zero_reward.
Qed.

Lemma dp_genesis_mint_reward :
  apply_tx_mint_reward (dp_state [(dp_op, dp_oru 1)] 0) = RES_REWARD_GENESIS.
Proof. vm_compute. reflexivity. Qed.

Lemma dp_mint_dual_pool_genesis :
  valid_dual_pool_tx [(dp_op, dp_oru 1)] dp_mint_tx RES_REWARD_GENESIS = true.
Proof.
  unfold valid_dual_pool_tx, dp_mint_tx.
  rewrite dp_gather_singleton.
  unfold res_mint_shape, sum_asset, txout_asset_value, count_asset, dp_oru, dp_res.
  vm_compute. reflexivity.
Qed.

Lemma dp_mint_script_shape :
  valid_script_shape_tx [(dp_op, dp_oru 1)] dp_mint_tx = true.
Proof.
  unfold valid_script_shape_tx, dp_mint_tx.
  rewrite dp_gather_singleton.
  vm_compute. reflexivity.
Qed.

Lemma dp_mint_cvg_spend :
  valid_cvg_spend_tx [(dp_op, dp_oru 1)] dp_mint_tx = true.
Proof.
  unfold valid_cvg_spend_tx, dp_mint_tx.
  rewrite dp_gather_singleton.
  vm_compute. reflexivity.
Qed.

Lemma dp_mint_apply_tx_utxo :
  apply_tx_utxo [(dp_op, dp_oru 1)] dp_mint_tx 0 =
    Some (fold_create_outputs (tx_creates dp_mint_tx)
            (utxo_remove [(dp_op, dp_oru 1)] dp_op) dp_txid 0).
Proof.
  unfold apply_tx_utxo, dp_mint_tx. cbv zeta.
  cbn [tx_id tx_spends tx_creates forallb fold_left].
  unfold utxo_lookup. rewrite outpoint_eqb_refl.
  cbn [andb]. reflexivity.
Qed.

Lemma apply_tx_accepts_scheduled_mint :
  match apply_tx (dp_state [(dp_op, dp_oru 1)] 0) dp_mint_tx 0 with
  | Some _ => true
  | None => false
  end = true.
Proof.
  unfold apply_tx.
  change (rs_utxo (dp_state [(dp_op, dp_oru 1)] 0)) with [(dp_op, dp_oru 1)].
  rewrite dp_mint_apply_tx_utxo.
  rewrite dp_genesis_mint_reward.
  rewrite dp_mint_dual_pool_genesis.
  rewrite apply_tx_derived_assets_obligation_true.
  rewrite dp_mint_cvg_spend.
  rewrite dp_mint_script_shape.
  rewrite (valid_cvg_create_tx_no_create _ dp_mint_tx eq_refl).
  reflexivity.
Qed.

(** ** Covenant-funded mint (pure CVG input, no ORU) *)

Definition dp_cvg (v : Z) : TxOut := mk_cvg v.

Definition dp_cvg_mint_tx : TxAbs :=
  {| tx_id := dp_txid;
     tx_spends := [dp_op];
     tx_creates := [mk_mint RES_REWARD_GENESIS];
     tx_pcorb := None |}.

Lemma dp_cvg_genesis_mint_reward :
  apply_tx_mint_reward (dp_state [(dp_op, dp_cvg 1)] 0) = RES_REWARD_GENESIS.
Proof. vm_compute. reflexivity. Qed.

Lemma dp_cvg_mint_dual_pool :
  valid_dual_pool_tx [(dp_op, dp_cvg 1)] dp_cvg_mint_tx RES_REWARD_GENESIS = true.
Proof.
  unfold valid_dual_pool_tx, dp_cvg_mint_tx.
  rewrite dp_gather_singleton.
  vm_compute. reflexivity.
Qed.

Lemma dp_cvg_mint_shape_true :
  cvg_mint_shape dp_cvg_mint_tx [dp_cvg 1] RES_REWARD_GENESIS = true.
Proof. vm_compute. reflexivity. Qed.

Lemma dp_cvg_mint_not_oru_funded :
  res_mint_shape dp_cvg_mint_tx [dp_cvg 1] RES_REWARD_GENESIS = false.
Proof. vm_compute. reflexivity. Qed.

Lemma dp_cvg_mint_script_shape :
  valid_script_shape_tx [(dp_op, dp_cvg 1)] dp_cvg_mint_tx = true.
Proof.
  unfold valid_script_shape_tx, dp_cvg_mint_tx.
  rewrite dp_gather_singleton.
  vm_compute. reflexivity.
Qed.

Lemma dp_cvg_mint_cvg_spend :
  valid_cvg_spend_tx [(dp_op, dp_cvg 1)] dp_cvg_mint_tx = true.
Proof.
  unfold valid_cvg_spend_tx, dp_cvg_mint_tx.
  rewrite dp_gather_singleton.
  vm_compute. reflexivity.
Qed.

Lemma dp_cvg_mint_apply_tx_utxo :
  apply_tx_utxo [(dp_op, dp_cvg 1)] dp_cvg_mint_tx 0 =
    Some (fold_create_outputs (tx_creates dp_cvg_mint_tx)
            (utxo_remove [(dp_op, dp_cvg 1)] dp_op) dp_txid 0).
Proof.
  unfold apply_tx_utxo, dp_cvg_mint_tx. cbv zeta.
  cbn [tx_id tx_spends tx_creates forallb fold_left].
  unfold utxo_lookup. rewrite outpoint_eqb_refl.
  cbn [andb]. reflexivity.
Qed.

Lemma apply_tx_accepts_cvg_funded_mint :
  match apply_tx (dp_state [(dp_op, dp_cvg 1)] 0) dp_cvg_mint_tx 0 with
  | Some _ => true
  | None => false
  end = true.
Proof.
  unfold apply_tx.
  change (rs_utxo (dp_state [(dp_op, dp_cvg 1)] 0)) with [(dp_op, dp_cvg 1)].
  rewrite dp_cvg_mint_apply_tx_utxo.
  rewrite dp_cvg_genesis_mint_reward.
  rewrite dp_cvg_mint_dual_pool.
  rewrite apply_tx_derived_assets_obligation_true.
  rewrite dp_cvg_mint_cvg_spend.
  rewrite dp_cvg_mint_script_shape.
  rewrite (valid_cvg_create_tx_no_create _ dp_cvg_mint_tx eq_refl).
  reflexivity.
Qed.

Lemma apply_tx_cvg_funded_mint_oru_conserved :
  forall s' : ReplayState,
    apply_tx (dp_state [(dp_op, dp_cvg 1)] 0) dp_cvg_mint_tx 0 = Some s' ->
    gather_inputs [(dp_op, dp_cvg 1)] (tx_spends dp_cvg_mint_tx) = Some [dp_cvg 1] /\
    sum_asset AssetORU [dp_cvg 1] = 0%Z /\
    sum_asset AssetORU (tx_creates dp_cvg_mint_tx) = 0%Z /\
    rs_oru_supply s' = 0%Z.
Proof.
  intros s' Happ.
  split.
  - unfold dp_cvg_mint_tx. cbn [tx_spends]. apply dp_gather_singleton.
  - pose proof dp_cvg_mint_shape_true as Hm.
    pose proof (cvg_mint_oru_balance_unchanged dp_cvg_mint_tx [dp_cvg 1]
                  RES_REWARD_GENESIS Hm) as [Hin Hout].
    split; [exact Hin|].
    split; [exact Hout|].
    apply apply_tx_preserves_scalars in Happ. apply Happ.
Qed.

Lemma apply_block_body_rejects_res_inflation :
  apply_block_body (dp_state [(dp_op, dp_res 1)] 0)
    {| blk_header :=
         {| hdr_height := 1;
            hdr_prev_hash := hash32_placeholder;
            hdr_chainwork := 1%Z;
            hdr_nbits := 0%Z;
            hdr_time := 0%Z |};
       blk_txs := [dp_inflate_tx];
       blk_mint := default_connect_mint |} = None.
Proof.
  unfold apply_block_body. cbn [blk_txs].
  unfold apply_txs.
  rewrite apply_tx_rejects_res_inflation.
  reflexivity.
Qed.

(** ** Boundary goldens (TASK-FV-MODEL-DUALPOOL-HARDENING-001) *)

Definition dp_op1 : OutPoint := MkOutPoint hash32_placeholder 1.

Definition dp_txid2 : Hash32 := 2 :: hash32_placeholder.

Lemma dp_op_neq_op1 : dp_op <> dp_op1.
Proof. unfold dp_op, dp_op1. discriminate. Qed.

Lemma dp_lookup_op :
  forall (o : TxOut) (rest : UTXOSet),
    utxo_lookup ((dp_op, o) :: rest) dp_op = Some o.
Proof.
  intros o rest. unfold utxo_lookup. rewrite outpoint_eqb_refl. reflexivity.
Qed.

Lemma dp_gather_head :
  forall (o : TxOut) (rest : UTXOSet),
    gather_inputs ((dp_op, o) :: rest) [dp_op] = Some [o].
Proof.
  intros o rest. unfold gather_inputs. rewrite dp_lookup_op. reflexivity.
Qed.

Lemma dp_apply_tx_mint_reward_0 :
  forall u, apply_tx_mint_reward (dp_state u 0) = RES_REWARD_GENESIS.
Proof. intros u. vm_compute. reflexivity. Qed.

Lemma dp_apply_tx_utxo_spend :
  forall (o : TxOut) (tx : TxAbs),
    tx_spends tx = [dp_op] ->
    apply_tx_utxo [(dp_op, o)] tx 0 =
      Some (fold_create_outputs (tx_creates tx)
              (utxo_remove [(dp_op, o)] dp_op) (tx_id tx) 0).
Proof.
  intros o tx Hs.
  unfold apply_tx_utxo. cbv zeta.
  rewrite Hs.
  cbn [forallb fold_left].
  unfold utxo_lookup. rewrite outpoint_eqb_refl.
  cbn [andb]. reflexivity.
Qed.

(** Zero RES in and out: ordinary ORU payment. *)
Definition dp_oru_pay_tx : TxAbs :=
  {| tx_id := dp_txid;
     tx_spends := [dp_op];
     tx_creates := [dp_oru 4];
     tx_pcorb := None |}.

Lemma dp_oru_pay_dual_pool :
  valid_dual_pool_tx [(dp_op, dp_oru 5)] dp_oru_pay_tx RES_REWARD_GENESIS = true.
Proof.
  unfold valid_dual_pool_tx, dp_oru_pay_tx.
  rewrite dp_gather_singleton.
  unfold res_mint_shape, valid_ordinary_dual_pool, no_res_to_oru, no_oru_to_res.
  unfold sum_asset, txout_asset_value, count_asset, dp_oru.
  vm_compute. reflexivity.
Qed.

Lemma dp_oru_pay_script_shape :
  valid_script_shape_tx [(dp_op, dp_oru 5)] dp_oru_pay_tx = true.
Proof.
  unfold valid_script_shape_tx, dp_oru_pay_tx.
  rewrite dp_gather_singleton.
  vm_compute. reflexivity.
Qed.

Lemma dp_oru_pay_cvg_spend :
  valid_cvg_spend_tx [(dp_op, dp_oru 5)] dp_oru_pay_tx = true.
Proof.
  unfold valid_cvg_spend_tx, dp_oru_pay_tx.
  rewrite dp_gather_singleton.
  vm_compute. reflexivity.
Qed.

Lemma apply_tx_accepts_zero_res_oru_payment :
  match apply_tx (dp_state [(dp_op, dp_oru 5)] 0) dp_oru_pay_tx 0 with
  | Some _ => true
  | None => false
  end = true.
Proof.
  unfold apply_tx.
  change (rs_utxo (dp_state [(dp_op, dp_oru 5)] 0)) with [(dp_op, dp_oru 5)].
  rewrite (dp_apply_tx_utxo_spend (dp_oru 5) dp_oru_pay_tx eq_refl).
  rewrite dp_apply_tx_mint_reward_0.
  rewrite dp_oru_pay_dual_pool.
  rewrite apply_tx_derived_assets_obligation_true.
  rewrite dp_oru_pay_cvg_spend.
  rewrite dp_oru_pay_script_shape.
  rewrite (valid_cvg_create_tx_no_create _ dp_oru_pay_tx eq_refl).
  reflexivity.
Qed.

(** Two RES inputs / two RES outputs, value conserved (burn 0). *)
Definition dp_multi_u : UTXOSet := [(dp_op, dp_res 3); (dp_op1, dp_res 5)].

Definition dp_multi_tx : TxAbs :=
  {| tx_id := dp_txid;
     tx_spends := [dp_op; dp_op1];
     tx_creates := [dp_res 4; dp_res 4];
     tx_pcorb := None |}.

Lemma dp_gather_pair :
  forall o0 o1 : TxOut,
    gather_inputs [(dp_op, o0); (dp_op1, o1)] [dp_op; dp_op1] = Some [o0; o1].
Proof.
  intros o0 o1.
  unfold gather_inputs, utxo_lookup.
  rewrite outpoint_eqb_refl.
  rewrite (outpoint_eqb_false dp_op1 dp_op) by (intro H; apply dp_op_neq_op1; symmetry; exact H).
  rewrite outpoint_eqb_refl.
  reflexivity.
Qed.

Lemma dp_multi_dual_pool :
  valid_dual_pool_tx dp_multi_u dp_multi_tx RES_REWARD_GENESIS = true.
Proof.
  unfold valid_dual_pool_tx, dp_multi_tx, dp_multi_u.
  rewrite dp_gather_pair.
  unfold res_mint_shape, valid_ordinary_dual_pool, no_res_to_oru, no_oru_to_res.
  unfold sum_asset, txout_asset_value, count_asset, dp_res.
  vm_compute. reflexivity.
Qed.

Lemma dp_multi_script_shape :
  valid_script_shape_tx dp_multi_u dp_multi_tx = true.
Proof.
  unfold valid_script_shape_tx, dp_multi_tx, dp_multi_u.
  rewrite dp_gather_pair.
  vm_compute. reflexivity.
Qed.

Lemma dp_multi_cvg_spend :
  valid_cvg_spend_tx dp_multi_u dp_multi_tx = true.
Proof.
  unfold valid_cvg_spend_tx, dp_multi_tx, dp_multi_u.
  rewrite dp_gather_pair.
  vm_compute. reflexivity.
Qed.

Lemma dp_multi_apply_tx_utxo :
  apply_tx_utxo dp_multi_u dp_multi_tx 0 =
    Some (fold_create_outputs (tx_creates dp_multi_tx)
            (fold_left (fun u op => utxo_remove u op) (tx_spends dp_multi_tx)
                       dp_multi_u)
            dp_txid 0).
Proof.
  unfold apply_tx_utxo, dp_multi_tx, dp_multi_u. cbv zeta.
  cbn [tx_id tx_spends tx_creates forallb fold_left].
  unfold utxo_lookup.
  rewrite outpoint_eqb_refl.
  rewrite (outpoint_eqb_false dp_op1 dp_op) by (intro H; apply dp_op_neq_op1; symmetry; exact H).
  rewrite outpoint_eqb_refl.
  cbn [andb]. reflexivity.
Qed.

Lemma apply_tx_accepts_multi_res_conserved :
  match apply_tx (dp_state dp_multi_u 0) dp_multi_tx 0 with
  | Some _ => true
  | None => false
  end = true.
Proof.
  unfold apply_tx.
  change (rs_utxo (dp_state dp_multi_u 0)) with dp_multi_u.
  rewrite dp_multi_apply_tx_utxo.
  rewrite dp_apply_tx_mint_reward_0.
  rewrite dp_multi_dual_pool.
  rewrite apply_tx_derived_assets_obligation_true.
  rewrite dp_multi_cvg_spend.
  rewrite dp_multi_script_shape.
  rewrite (valid_cvg_create_tx_no_create _ dp_multi_tx eq_refl).
  reflexivity.
Qed.

(** Mint amount W(h)+1 and W(h)-1 are rejected. *)
Definition dp_mint_hi_tx : TxAbs :=
  {| tx_id := dp_txid;
     tx_spends := [dp_op];
     tx_creates := [dp_res (RES_REWARD_GENESIS + 1)];
     tx_pcorb := None |}.

Definition dp_mint_lo_tx : TxAbs :=
  {| tx_id := dp_txid;
     tx_spends := [dp_op];
     tx_creates := [dp_res (RES_REWARD_GENESIS - 1)];
     tx_pcorb := None |}.

Lemma dp_mint_hi_not_dual_pool :
  valid_dual_pool_tx [(dp_op, dp_oru 1)] dp_mint_hi_tx RES_REWARD_GENESIS = false.
Proof.
  unfold valid_dual_pool_tx, dp_mint_hi_tx.
  rewrite dp_gather_singleton.
  unfold res_mint_shape, valid_ordinary_dual_pool, no_res_to_oru, no_oru_to_res.
  unfold sum_asset, txout_asset_value, count_asset, dp_oru, dp_res.
  vm_compute. reflexivity.
Qed.

Lemma dp_mint_lo_not_dual_pool :
  valid_dual_pool_tx [(dp_op, dp_oru 1)] dp_mint_lo_tx RES_REWARD_GENESIS = false.
Proof.
  unfold valid_dual_pool_tx, dp_mint_lo_tx.
  rewrite dp_gather_singleton.
  unfold res_mint_shape, valid_ordinary_dual_pool, no_res_to_oru, no_oru_to_res.
  unfold sum_asset, txout_asset_value, count_asset, dp_oru, dp_res.
  vm_compute. reflexivity.
Qed.

Lemma apply_tx_rejects_mint_plus_one_atom :
  apply_tx (dp_state [(dp_op, dp_oru 1)] 0) dp_mint_hi_tx 0 = None.
Proof.
  apply apply_tx_rejects_without_dual_pool.
  rewrite dp_apply_tx_mint_reward_0.
  exact dp_mint_hi_not_dual_pool.
Qed.

Lemma apply_tx_rejects_mint_minus_one_atom :
  apply_tx (dp_state [(dp_op, dp_oru 1)] 0) dp_mint_lo_tx 0 = None.
Proof.
  apply apply_tx_rejects_without_dual_pool.
  rewrite dp_apply_tx_mint_reward_0.
  exact dp_mint_lo_not_dual_pool.
Qed.

(** Ordinary RES transfer then scheduled mint in the same body. *)
Definition dp_xfer_tx : TxAbs :=
  {| tx_id := dp_txid;
     tx_spends := [dp_op];
     tx_creates := [dp_res 4];
     tx_pcorb := None |}.

Definition dp_mint_tx2 : TxAbs :=
  {| tx_id := dp_txid2;
     tx_spends := [dp_op1];
     tx_creates := [dp_res RES_REWARD_GENESIS];
     tx_pcorb := None |}.

Definition dp_mix_u : UTXOSet := [(dp_op, dp_res 5); (dp_op1, dp_oru 1)].

Lemma dp_xfer_dual_pool_mix :
  valid_dual_pool_tx dp_mix_u dp_xfer_tx RES_REWARD_GENESIS = true.
Proof.
  unfold valid_dual_pool_tx, dp_xfer_tx, dp_mix_u.
  rewrite dp_gather_head.
  unfold res_mint_shape, valid_ordinary_dual_pool, no_res_to_oru, no_oru_to_res.
  unfold sum_asset, txout_asset_value, count_asset, dp_res.
  vm_compute. reflexivity.
Qed.

Lemma dp_xfer_script_shape_mix :
  valid_script_shape_tx dp_mix_u dp_xfer_tx = true.
Proof.
  unfold valid_script_shape_tx, dp_xfer_tx, dp_mix_u.
  rewrite dp_gather_head.
  vm_compute. reflexivity.
Qed.

Lemma dp_xfer_cvg_spend_mix :
  valid_cvg_spend_tx dp_mix_u dp_xfer_tx = true.
Proof.
  unfold valid_cvg_spend_tx, dp_xfer_tx, dp_mix_u.
  rewrite dp_gather_head.
  vm_compute. reflexivity.
Qed.

Lemma dp_xfer_apply_tx_utxo_mix :
  apply_tx_utxo dp_mix_u dp_xfer_tx 0 =
    Some (fold_create_outputs (tx_creates dp_xfer_tx)
            (utxo_remove dp_mix_u dp_op) dp_txid 0).
Proof.
  unfold apply_tx_utxo, dp_xfer_tx, dp_mix_u. cbv zeta.
  cbn [tx_id tx_spends tx_creates forallb fold_left].
  rewrite dp_lookup_op.
  cbn [andb]. reflexivity.
Qed.

Definition dp_mix_u_after_xfer : UTXOSet :=
  fold_create_outputs (tx_creates dp_xfer_tx)
    (utxo_remove dp_mix_u dp_op) dp_txid 0.

Lemma dp_op1_eqb_op_false : outpoint_eqb dp_op1 dp_op = false.
Proof.
  apply outpoint_eqb_false. intro H. apply dp_op_neq_op1. symmetry. exact H.
Qed.

Lemma dp_mix_remove_op :
  utxo_remove dp_mix_u dp_op = [(dp_op1, dp_oru 1)].
Proof.
  unfold utxo_remove, dp_mix_u.
  cbn [filter fst].
  rewrite outpoint_eqb_refl.
  cbn [negb filter fst].
  rewrite dp_op1_eqb_op_false.
  cbn [negb].
  reflexivity.
Qed.

Lemma dp_op1_neq_created :
  outpoint_eqb dp_op1 (MkOutPoint dp_txid 0) = false.
Proof.
  apply outpoint_eqb_false. unfold dp_op1, dp_txid. discriminate.
Qed.

Lemma dp_mix_lookup_op1_after_xfer :
  utxo_lookup dp_mix_u_after_xfer dp_op1 = Some (dp_oru 1).
Proof.
  unfold dp_mix_u_after_xfer, fold_create_outputs, dp_xfer_tx.
  cbn [tx_creates fold_left fst].
  rewrite dp_mix_remove_op.
  unfold utxo_insert, utxo_remove.
  cbn [filter fst].
  rewrite dp_op1_neq_created.
  cbn [negb app].
  unfold utxo_lookup.
  rewrite outpoint_eqb_refl.
  reflexivity.
Qed.

Lemma dp_mint_tx2_gather_after_xfer :
  gather_inputs dp_mix_u_after_xfer [dp_op1] = Some [dp_oru 1].
Proof.
  unfold gather_inputs. rewrite dp_mix_lookup_op1_after_xfer. reflexivity.
Qed.

Lemma dp_mint_tx2_dual_pool_after_xfer :
  valid_dual_pool_tx dp_mix_u_after_xfer dp_mint_tx2 RES_REWARD_GENESIS = true.
Proof.
  unfold valid_dual_pool_tx, dp_mint_tx2.
  cbn [tx_spends].
  rewrite dp_mint_tx2_gather_after_xfer.
  unfold res_mint_shape, sum_asset, txout_asset_value, count_asset, dp_oru, dp_res.
  vm_compute. reflexivity.
Qed.

Lemma dp_mint_tx2_script_shape_after_xfer :
  valid_script_shape_tx dp_mix_u_after_xfer dp_mint_tx2 = true.
Proof.
  unfold valid_script_shape_tx, dp_mint_tx2.
  cbn [tx_spends].
  rewrite dp_mint_tx2_gather_after_xfer.
  vm_compute. reflexivity.
Qed.

Lemma dp_mint_tx2_cvg_spend_after_xfer :
  valid_cvg_spend_tx dp_mix_u_after_xfer dp_mint_tx2 = true.
Proof.
  unfold valid_cvg_spend_tx, dp_mint_tx2.
  cbn [tx_spends].
  rewrite dp_mint_tx2_gather_after_xfer.
  vm_compute. reflexivity.
Qed.

Lemma dp_mint_tx2_apply_tx_utxo :
  forall idx : nat,
    apply_tx_utxo dp_mix_u_after_xfer dp_mint_tx2 idx =
      Some (fold_create_outputs (tx_creates dp_mint_tx2)
              (utxo_remove dp_mix_u_after_xfer dp_op1) dp_txid2 0).
Proof.
  intros idx.
  unfold apply_tx_utxo, dp_mint_tx2. cbv zeta.
  cbn [tx_id tx_spends tx_creates forallb fold_left].
  rewrite dp_mix_lookup_op1_after_xfer.
  cbn [andb]. reflexivity.
Qed.

Lemma apply_tx_xfer_mix_some :
  exists s1,
    apply_tx (dp_state dp_mix_u 0) dp_xfer_tx 0 = Some s1 /\
    rs_utxo s1 = dp_mix_u_after_xfer /\
    apply_tx_mint_reward s1 = RES_REWARD_GENESIS.
Proof.
  unfold apply_tx.
  change (rs_utxo (dp_state dp_mix_u 0)) with dp_mix_u.
  rewrite dp_xfer_apply_tx_utxo_mix.
  rewrite dp_apply_tx_mint_reward_0.
  rewrite dp_xfer_dual_pool_mix.
  rewrite apply_tx_derived_assets_obligation_true.
  rewrite dp_xfer_cvg_spend_mix.
  rewrite dp_xfer_script_shape_mix.
  rewrite (valid_cvg_create_tx_no_create _ dp_xfer_tx eq_refl).
  eexists.
  split; [reflexivity|].
  split; [reflexivity|].
  unfold apply_tx_mint_reward, replay_set_utxo, dp_state. cbn.
  vm_compute. reflexivity.
Qed.

Lemma apply_txs_accepts_transfer_then_mint :
  match apply_txs (dp_state dp_mix_u 0) [dp_xfer_tx; dp_mint_tx2] 0 with
  | Some _ => true
  | None => false
  end = true.
Proof.
  unfold apply_txs.
  destruct apply_tx_xfer_mix_some as [s1 [Hxfer [Hu Hr]]].
  rewrite Hxfer.
  unfold apply_tx.
  rewrite Hu, Hr.
  rewrite (dp_mint_tx2_apply_tx_utxo 1).
  rewrite dp_mint_tx2_dual_pool_after_xfer.
  rewrite apply_tx_derived_assets_obligation_true.
  rewrite dp_mint_tx2_cvg_spend_after_xfer.
  rewrite dp_mint_tx2_script_shape_after_xfer.
  rewrite (valid_cvg_create_tx_no_create _ dp_mint_tx2 eq_refl).
  reflexivity.
Qed.

(** ** Mixed body: two ordinary RES transfers then one scheduled mint
    (TASK-FV-MODEL-DUALPOOL-BLOCK-LEVEL-INVARIANT-001). *)

Definition dp_op2 : OutPoint := MkOutPoint hash32_placeholder 2.

Definition dp_txid3 : Hash32 := 3 :: hash32_placeholder.

Lemma dp_op_neq_op2 : dp_op <> dp_op2.
Proof. unfold dp_op, dp_op2. discriminate. Qed.

Lemma dp_op1_neq_op2 : dp_op1 <> dp_op2.
Proof. unfold dp_op1, dp_op2. discriminate. Qed.

Lemma outpoint_eqb_diff_index :
  forall (h : Hash32) (i j : nat),
    i <> j ->
    outpoint_eqb (MkOutPoint h i) (MkOutPoint h j) = false.
Proof.
  intros h i j H.
  unfold outpoint_eqb.
  rewrite (proj2 (Nat.eqb_neq i j) H).
  reflexivity.
Qed.

Lemma apply_tx_utxo_spend_head :
  forall (op : OutPoint) (o : TxOut) (rest : UTXOSet) (tx : TxAbs) (idx : nat),
    tx_spends tx = [op] ->
    apply_tx_utxo ((op, o) :: rest) tx idx =
      Some (fold_create_outputs (tx_creates tx)
              (utxo_remove ((op, o) :: rest) op) (tx_id tx) 0).
Proof.
  intros op o rest tx idx Hs.
  unfold apply_tx_utxo. cbv zeta.
  rewrite Hs.
  cbn [forallb fold_left].
  unfold utxo_lookup. rewrite outpoint_eqb_refl.
  cbn [andb]. reflexivity.
Qed.

Definition dp_e2e_u : UTXOSet :=
  [(dp_op, dp_res 10); (dp_op1, dp_res 7); (dp_op2, dp_oru 1)].

Definition dp_e2e_xfer1 : TxAbs :=
  {| tx_id := dp_txid;
     tx_spends := [dp_op];
     tx_creates := [dp_res 8];
     tx_pcorb := None |}.

Definition dp_e2e_xfer2 : TxAbs :=
  {| tx_id := dp_txid2;
     tx_spends := [dp_op1];
     tx_creates := [dp_res 6];
     tx_pcorb := None |}.

Definition dp_e2e_mint : TxAbs :=
  {| tx_id := dp_txid3;
     tx_spends := [dp_op2];
     tx_creates := [dp_res RES_REWARD_GENESIS];
     tx_pcorb := None |}.

Lemma dp_e2e_xfer1_dual_pool :
  valid_dual_pool_tx dp_e2e_u dp_e2e_xfer1 RES_REWARD_GENESIS = true.
Proof.
  unfold valid_dual_pool_tx, dp_e2e_xfer1, dp_e2e_u.
  rewrite dp_gather_head.
  unfold res_mint_shape, valid_ordinary_dual_pool, no_res_to_oru, no_oru_to_res.
  unfold sum_asset, txout_asset_value, count_asset, dp_res.
  vm_compute. reflexivity.
Qed.

Lemma dp_e2e_xfer1_script_shape :
  valid_script_shape_tx dp_e2e_u dp_e2e_xfer1 = true.
Proof.
  unfold valid_script_shape_tx, dp_e2e_xfer1, dp_e2e_u.
  rewrite dp_gather_head.
  vm_compute. reflexivity.
Qed.

Lemma dp_e2e_xfer1_cvg_spend :
  valid_cvg_spend_tx dp_e2e_u dp_e2e_xfer1 = true.
Proof.
  unfold valid_cvg_spend_tx, dp_e2e_xfer1, dp_e2e_u.
  rewrite dp_gather_head.
  vm_compute. reflexivity.
Qed.

Lemma dp_e2e_remove_op :
  utxo_remove dp_e2e_u dp_op = [(dp_op1, dp_res 7); (dp_op2, dp_oru 1)].
Proof.
  unfold utxo_remove, dp_e2e_u.
  cbn [filter fst].
  rewrite outpoint_eqb_refl.
  cbn [negb filter fst].
  rewrite (outpoint_eqb_false dp_op1 dp_op)
    by (intro H; apply dp_op_neq_op1; symmetry; exact H).
  cbn [negb filter fst].
  rewrite (outpoint_eqb_false dp_op2 dp_op)
    by (intro H; apply dp_op_neq_op2; symmetry; exact H).
  cbn [negb].
  reflexivity.
Qed.

Definition dp_e2e_u1 : UTXOSet :=
  fold_create_outputs (tx_creates dp_e2e_xfer1)
    (utxo_remove dp_e2e_u dp_op) dp_txid 0.

Lemma dp_e2e_u1_compute :
  dp_e2e_u1 =
    [(dp_op1, dp_res 7); (dp_op2, dp_oru 1); (MkOutPoint dp_txid 0, dp_res 8)].
Proof.
  unfold dp_e2e_u1, fold_create_outputs, dp_e2e_xfer1.
  cbn [tx_creates fold_left].
  rewrite dp_e2e_remove_op.
  unfold utxo_insert, utxo_remove.
  cbn [filter fst].
  rewrite (outpoint_eqb_false dp_op1 (MkOutPoint dp_txid 0))
    by (unfold dp_op1, dp_txid; discriminate).
  cbn [negb filter fst].
  rewrite (outpoint_eqb_false dp_op2 (MkOutPoint dp_txid 0))
    by (unfold dp_op2, dp_txid; discriminate).
  cbn [negb app].
  reflexivity.
Qed.

Lemma apply_tx_e2e_xfer1_some :
  exists s1,
    apply_tx (dp_state dp_e2e_u 0) dp_e2e_xfer1 0 = Some s1 /\
    rs_utxo s1 = dp_e2e_u1 /\
    apply_tx_mint_reward s1 = RES_REWARD_GENESIS.
Proof.
  unfold apply_tx.
  change (rs_utxo (dp_state dp_e2e_u 0)) with dp_e2e_u.
  unfold dp_e2e_u at 1.
  rewrite (apply_tx_utxo_spend_head dp_op (dp_res 10)
             [(dp_op1, dp_res 7); (dp_op2, dp_oru 1)] dp_e2e_xfer1 0 eq_refl).
  rewrite dp_apply_tx_mint_reward_0.
  rewrite dp_e2e_xfer1_dual_pool.
  rewrite apply_tx_derived_assets_obligation_true.
  rewrite dp_e2e_xfer1_cvg_spend.
  rewrite dp_e2e_xfer1_script_shape.
  rewrite (valid_cvg_create_tx_no_create _ dp_e2e_xfer1 eq_refl).
  eexists.
  split; [reflexivity|].
  split; [reflexivity|].
  unfold apply_tx_mint_reward, replay_set_utxo, dp_state. cbn.
  vm_compute. reflexivity.
Qed.

Lemma dp_e2e_xfer2_dual_pool :
  valid_dual_pool_tx dp_e2e_u1 dp_e2e_xfer2 RES_REWARD_GENESIS = true.
Proof.
  unfold valid_dual_pool_tx, dp_e2e_xfer2, gather_inputs.
  cbn [tx_spends].
  rewrite dp_e2e_u1_compute.
  cbn [utxo_lookup].
  rewrite outpoint_eqb_refl.
  unfold res_mint_shape, valid_ordinary_dual_pool, no_res_to_oru, no_oru_to_res.
  unfold sum_asset, txout_asset_value, count_asset, dp_res.
  vm_compute. reflexivity.
Qed.

Lemma dp_e2e_xfer2_script_shape :
  valid_script_shape_tx dp_e2e_u1 dp_e2e_xfer2 = true.
Proof.
  unfold valid_script_shape_tx, dp_e2e_xfer2, gather_inputs.
  cbn [tx_spends].
  rewrite dp_e2e_u1_compute.
  cbn [utxo_lookup].
  rewrite outpoint_eqb_refl.
  vm_compute. reflexivity.
Qed.

Lemma dp_e2e_xfer2_cvg_spend :
  valid_cvg_spend_tx dp_e2e_u1 dp_e2e_xfer2 = true.
Proof.
  unfold valid_cvg_spend_tx, dp_e2e_xfer2, gather_inputs.
  cbn [tx_spends].
  rewrite dp_e2e_u1_compute.
  cbn [utxo_lookup].
  rewrite outpoint_eqb_refl.
  vm_compute. reflexivity.
Qed.

Lemma dp_e2e_remove_op1_u1 :
  utxo_remove dp_e2e_u1 dp_op1 =
    [(dp_op2, dp_oru 1); (MkOutPoint dp_txid 0, dp_res 8)].
Proof.
  rewrite dp_e2e_u1_compute.
  unfold utxo_remove.
  cbn [filter fst].
  rewrite outpoint_eqb_refl.
  cbn [negb filter fst].
  rewrite (outpoint_eqb_false dp_op2 dp_op1)
    by (intro H; apply dp_op1_neq_op2; symmetry; exact H).
  cbn [negb filter fst].
  rewrite (outpoint_eqb_false (MkOutPoint dp_txid 0) dp_op1)
    by (unfold dp_op1, dp_txid; discriminate).
  cbn [negb].
  reflexivity.
Qed.

Definition dp_e2e_u2 : UTXOSet :=
  fold_create_outputs (tx_creates dp_e2e_xfer2)
    (utxo_remove dp_e2e_u1 dp_op1) dp_txid2 0.

Lemma dp_e2e_u2_compute :
  dp_e2e_u2 =
    [(dp_op2, dp_oru 1); (MkOutPoint dp_txid 0, dp_res 8);
     (MkOutPoint dp_txid2 0, dp_res 6)].
Proof.
  unfold dp_e2e_u2, fold_create_outputs, dp_e2e_xfer2.
  cbn [tx_creates fold_left].
  rewrite dp_e2e_remove_op1_u1.
  unfold utxo_insert, utxo_remove.
  cbn [filter fst].
  rewrite (outpoint_eqb_false dp_op2 (MkOutPoint dp_txid2 0))
    by (unfold dp_op2, dp_txid2; discriminate).
  cbn [negb filter fst].
  rewrite (outpoint_eqb_false (MkOutPoint dp_txid 0) (MkOutPoint dp_txid2 0))
    by (unfold dp_txid, dp_txid2; discriminate).
  cbn [negb app].
  reflexivity.
Qed.

Lemma apply_tx_e2e_xfer2_some :
  forall s1 : ReplayState,
    rs_utxo s1 = dp_e2e_u1 ->
    apply_tx_mint_reward s1 = RES_REWARD_GENESIS ->
    exists s2,
      apply_tx s1 dp_e2e_xfer2 1 = Some s2 /\
      rs_utxo s2 = dp_e2e_u2 /\
      apply_tx_mint_reward s2 = RES_REWARD_GENESIS.
Proof.
  intros s1 Hu Hr.
  unfold apply_tx.
  rewrite Hu, Hr.
  rewrite dp_e2e_xfer2_dual_pool.
  rewrite apply_tx_derived_assets_obligation_true.
  rewrite dp_e2e_xfer2_cvg_spend.
  rewrite dp_e2e_xfer2_script_shape.
  rewrite (valid_cvg_create_tx_no_create _ dp_e2e_xfer2 eq_refl).
  rewrite dp_e2e_u1_compute.
  rewrite (apply_tx_utxo_spend_head dp_op1 (dp_res 7)
             [(dp_op2, dp_oru 1); (MkOutPoint dp_txid 0, dp_res 8)]
             dp_e2e_xfer2 1 eq_refl).
  eexists.
  split; [reflexivity|].
  split.
  - unfold replay_set_utxo. destruct s1. simpl.
    unfold dp_e2e_u2. rewrite dp_e2e_u1_compute. reflexivity.
  - rewrite <- Hr.
    unfold apply_tx_mint_reward, replay_set_utxo.
    destruct s1. reflexivity.
Qed.

Lemma dp_e2e_mint_dual_pool :
  valid_dual_pool_tx dp_e2e_u2 dp_e2e_mint RES_REWARD_GENESIS = true.
Proof.
  unfold valid_dual_pool_tx, dp_e2e_mint, gather_inputs.
  cbn [tx_spends].
  rewrite dp_e2e_u2_compute.
  cbn [utxo_lookup].
  rewrite outpoint_eqb_refl.
  unfold res_mint_shape, sum_asset, txout_asset_value, count_asset, dp_oru, dp_res.
  vm_compute. reflexivity.
Qed.

Lemma dp_e2e_mint_script_shape :
  valid_script_shape_tx dp_e2e_u2 dp_e2e_mint = true.
Proof.
  unfold valid_script_shape_tx, dp_e2e_mint, gather_inputs.
  cbn [tx_spends].
  rewrite dp_e2e_u2_compute.
  cbn [utxo_lookup].
  rewrite outpoint_eqb_refl.
  vm_compute. reflexivity.
Qed.

Lemma dp_e2e_mint_cvg_spend :
  valid_cvg_spend_tx dp_e2e_u2 dp_e2e_mint = true.
Proof.
  unfold valid_cvg_spend_tx, dp_e2e_mint, gather_inputs.
  cbn [tx_spends].
  rewrite dp_e2e_u2_compute.
  cbn [utxo_lookup].
  rewrite outpoint_eqb_refl.
  vm_compute. reflexivity.
Qed.

Lemma apply_tx_e2e_mint_some :
  forall s2 : ReplayState,
    rs_utxo s2 = dp_e2e_u2 ->
    apply_tx_mint_reward s2 = RES_REWARD_GENESIS ->
    exists s3,
      apply_tx s2 dp_e2e_mint 2 = Some s3.
Proof.
  intros s2 Hu Hr.
  unfold apply_tx.
  rewrite Hu, Hr.
  rewrite dp_e2e_mint_dual_pool.
  rewrite apply_tx_derived_assets_obligation_true.
  rewrite dp_e2e_mint_cvg_spend.
  rewrite dp_e2e_mint_script_shape.
  rewrite (valid_cvg_create_tx_no_create _ dp_e2e_mint eq_refl).
  rewrite dp_e2e_u2_compute.
  rewrite (apply_tx_utxo_spend_head dp_op2 (dp_oru 1)
             [(MkOutPoint dp_txid 0, dp_res 8); (MkOutPoint dp_txid2 0, dp_res 6)]
             dp_e2e_mint 2 eq_refl).
  eexists. reflexivity.
Qed.

Lemma apply_txs_accepts_two_transfers_then_mint :
  match apply_txs (dp_state dp_e2e_u 0)
                  [dp_e2e_xfer1; dp_e2e_xfer2; dp_e2e_mint] 0 with
  | Some _ => true
  | None => false
  end = true.
Proof.
  unfold apply_txs.
  destruct apply_tx_e2e_xfer1_some as [s1 [Hx1 [Hu1 Hr1]]].
  rewrite Hx1.
  destruct (apply_tx_e2e_xfer2_some s1 Hu1 Hr1) as [s2 [Hx2 [Hu2 Hr2]]].
  rewrite Hx2.
  destruct (apply_tx_e2e_mint_some s2 Hu2 Hr2) as [s3 Hx3].
  rewrite Hx3.
  reflexivity.
Qed.

Lemma apply_block_body_accepts_two_transfers_then_mint :
  match apply_block_body (dp_state dp_e2e_u 0)
    {| blk_header :=
         {| hdr_height := 1;
            hdr_prev_hash := hash32_placeholder;
            hdr_chainwork := 1%Z;
            hdr_nbits := 0%Z;
            hdr_time := 0%Z |};
       blk_txs := [dp_e2e_xfer1; dp_e2e_xfer2; dp_e2e_mint];
       blk_mint := default_connect_mint |} with
  | Some _ => true
  | None => false
  end = true.
Proof.
  unfold apply_block_body. cbn [blk_txs].
  pose proof apply_txs_accepts_two_transfers_then_mint as Hacc.
  destruct (apply_txs (dp_state dp_e2e_u 0)
                      [dp_e2e_xfer1; dp_e2e_xfer2; dp_e2e_mint] 0);
    [reflexivity | discriminate Hacc].
Qed.

(** * Winner authorization for body mints, covenant-funded included

    [cvg_mint_shape] is deliberately silent about the draw. It checks amounts, owner-hash
    coupling and one-shot consumption; it says nothing about who won, and neither does
    [res_mint_shape]. Authorization was supposed to come from the block level, where
    [valid_res_mint_obligation] ties mints to [cm_draw_won] — but it ties them through
    [cm_res_mint_count], an abstract [ConnectMintSpec] field that nothing connected to the
    transactions actually in [blk_txs].

    So the model admitted a block carrying a mint-shape transaction while recording zero mints
    and no winner: the transaction-level shape authorized the RES, and the block-level
    obligation was satisfied vacuously by a field that lied about the body. Nothing bounded the
    number of mint-shape transactions in a body either — [apply_block_res_net_bounded_by_
    scheduled_mints] bounds body RES by [n * W(h)] with [n] unconstrained.

    [valid_body_mint_count] is the missing conjunct: the recorded count must equal the count
    [txs_res_net_and_mints] observes in the body. [apply_block_authorized] is the entry point
    that enforces it, and [apply_block_authorized_implies_apply_block] carries every invariant
    already proved about [apply_block] across unchanged.

    The theorems below have no C++ counterpart to keep in step: production reached the same
    conclusion the blunt way, by retiring the covenant entirely (see script.h on 0xbc), which is
    strictly stronger than requiring a winner. What is still *not* modelled is the winner's
    identity —
    [cm_draw_won] remains an unconstrained boolean ([OpaqueBoundary.v]), so these theorems say
    "a mint needs the block to claim a win", not "a mint needs σ to have selected this owner".
    That binding is the σ-preimage chain and [CheckResRewardOwner], and it stays C++. *)

Fixpoint body_has_scheduled_mint (s : ReplayState) (txs : list TxAbs) (idx : nat) : bool :=
  match txs with
  | nil => false
  | t :: rest =>
      match gather_inputs (rs_utxo s) (tx_spends t) with
      | None => false
      | Some ins =>
          match apply_tx s t idx with
          | None => false
          | Some s1 =>
              orb (is_scheduled_mint_shape t ins (apply_tx_mint_reward s))
                  (body_has_scheduled_mint s1 rest (S idx))
          end
      end
  end.

Definition body_mint_count (s : ReplayState) (b : BlockAbs) : option nat :=
  match txs_res_net_and_mints s (blk_txs b) 0 with
  | None => None
  | Some (_, n) => Some n
  end.

Definition valid_body_mint_count (s : ReplayState) (b : BlockAbs) : bool :=
  match body_mint_count s b with
  | None => false
  | Some n => Nat.eqb n (cm_res_mint_count (blk_mint b))
  end.

Definition valid_block_connect_authorized (s : ReplayState) (b : BlockAbs) : bool :=
  andb (valid_block_connect s b) (valid_body_mint_count s b).

Definition apply_block_authorized (s : ReplayState) (b : BlockAbs) : option ReplayState :=
  if valid_block_connect_authorized s b then apply_block_body s b else None.

Lemma apply_block_authorized_inv :
  forall (s : ReplayState) (b : BlockAbs) (s' : ReplayState),
    apply_block_authorized s b = Some s' ->
    valid_block_connect s b = true /\
    valid_body_mint_count s b = true /\
    apply_block_body s b = Some s'.
Proof.
  intros s b s' H. unfold apply_block_authorized in H.
  destruct (valid_block_connect_authorized s b) eqn:Ea; [|discriminate H].
  unfold valid_block_connect_authorized in Ea.
  apply andb_prop in Ea as [Hc Hn].
  repeat split; assumption.
Qed.

(** Strengthening only: every invariant already proved about [apply_block] applies verbatim. *)
Theorem apply_block_authorized_implies_apply_block :
  forall (s : ReplayState) (b : BlockAbs) (s' : ReplayState),
    apply_block_authorized s b = Some s' ->
    apply_block s b = Some s'.
Proof.
  intros s b s' H.
  destruct (apply_block_authorized_inv s b s' H) as [Hc [_ Hbody]].
  unfold apply_block. rewrite Hc. exact Hbody.
Qed.

Lemma apply_block_body_has_txs :
  forall (s : ReplayState) (b : BlockAbs) (s' : ReplayState),
    apply_block_body s b = Some s' ->
    exists s_tx : ReplayState, apply_txs s (blk_txs b) 0 = Some s_tx.
Proof.
  intros s b s' H. unfold apply_block_body in H.
  destruct (apply_txs s (blk_txs b) 0) as [s_tx|] eqn:E; [|discriminate H].
  exists s_tx. reflexivity.
Qed.

Lemma apply_block_authorized_body_count :
  forall (s : ReplayState) (b : BlockAbs) (s' : ReplayState),
    apply_block_authorized s b = Some s' ->
    body_mint_count s b = Some (cm_res_mint_count (blk_mint b)).
Proof.
  intros s b s' H.
  destruct (apply_block_authorized_inv s b s' H) as [_ [Hn _]].
  unfold valid_body_mint_count in Hn.
  destruct (body_mint_count s b) as [n|] eqn:E; [|discriminate Hn].
  apply Nat.eqb_eq in Hn. rewrite Hn. reflexivity.
Qed.

(** A mint-shape transaction anywhere in an applied body makes the observed count positive. *)
Lemma body_has_scheduled_mint_count_pos :
  forall (txs : list TxAbs) (s : ReplayState) (idx : nat) (s' : ReplayState)
         (d : Z) (n : nat),
    apply_txs s txs idx = Some s' ->
    txs_res_net_and_mints s txs idx = Some (d, n) ->
    body_has_scheduled_mint s txs idx = true ->
    (0 < n)%nat.
Proof.
  induction txs as [|t rest IH]; intros s idx s' d n Happ Hnet Hhas.
  - simpl in Hhas. discriminate Hhas.
  - simpl in Hhas. simpl in Hnet.
    destruct (gather_inputs (rs_utxo s) (tx_spends t)) as [ins|] eqn:Eg;
      [|discriminate Hhas].
    destruct (apply_tx s t idx) as [s1|] eqn:Eat; [|discriminate Hhas].
    destruct (txs_res_net_and_mints s1 rest (S idx)) as [[d0 n0]|] eqn:Enet0;
      [|discriminate Hnet].
    destruct (is_scheduled_mint_shape t ins (apply_tx_mint_reward s)) eqn:Es.
    + simpl in Hnet. injection Hnet as _ Hn. subst n. apply Nat.lt_0_succ.
    + simpl in Hnet. simpl in Hhas.
      injection Hnet as _ Hn. subst n.
      simpl in Happ. rewrite Eat in Happ.
      apply (IH s1 (S idx) s' d0 n0 Happ Enet0 Hhas).
Qed.

Lemma body_mint_count_zero_no_scheduled_mint :
  forall (s : ReplayState) (b : BlockAbs) (s' : ReplayState),
    apply_block_body s b = Some s' ->
    body_mint_count s b = Some 0%nat ->
    body_has_scheduled_mint s (blk_txs b) 0 = false.
Proof.
  intros s b s' Hbody Hcount.
  destruct (apply_block_body_has_txs s b s' Hbody) as [s_tx Htxs].
  destruct (body_has_scheduled_mint s (blk_txs b) 0) eqn:Ehas; [|reflexivity].
  exfalso.
  unfold body_mint_count in Hcount.
  destruct (txs_res_net_and_mints s (blk_txs b) 0) as [[d n]|] eqn:Enet;
    [|discriminate Hcount].
  injection Hcount as Hn. subst n.
  pose proof (body_has_scheduled_mint_count_pos (blk_txs b) s 0 s_tx d 0%nat
                Htxs Enet Ehas) as Hpos.
  lia.
Qed.

(** No claimed win, no mint of either funding shape — covenant-funded included. *)
Theorem no_winner_forbids_scheduled_mint :
  forall (s : ReplayState) (b : BlockAbs) (s' : ReplayState),
    apply_block_authorized s b = Some s' ->
    cm_draw_won (blk_mint b) = false ->
    body_has_scheduled_mint s (blk_txs b) 0 = false.
Proof.
  intros s b s' H Hwon.
  destruct (apply_block_authorized_inv s b s' H) as [Hc [_ Hbody]].
  pose proof (no_winner_zero_mints s b Hc Hwon) as Hzero.
  pose proof (apply_block_authorized_body_count s b s' H) as Hcount.
  rewrite Hzero in Hcount.
  apply (body_mint_count_zero_no_scheduled_mint s b s' Hbody Hcount).
Qed.

(** Once the schedule pays nothing, no funding shape can mint either. *)
Theorem schedule_exhausted_forbids_scheduled_mint :
  forall (s : ReplayState) (b : BlockAbs) (s' : ReplayState),
    apply_block_authorized s b = Some s' ->
    res_mint_authorized b = false ->
    body_has_scheduled_mint s (blk_txs b) 0 = false.
Proof.
  intros s b s' H Hauth.
  destruct (apply_block_authorized_inv s b s' H) as [Hc [_ Hbody]].
  apply res_mint_authorized_false_iff_zero_W in Hauth.
  pose proof (h1_valid_connect_zero_mints s b Hc Hauth) as Hzero.
  pose proof (apply_block_authorized_body_count s b s' H) as Hcount.
  rewrite Hzero in Hcount.
  apply (body_mint_count_zero_no_scheduled_mint s b s' Hbody Hcount).
Qed.

(** The converse, and the theorem that closes the covenant hole: a body mint requires the
    block to claim a win *and* the schedule to still pay. Contrapositive of the two above. *)
Theorem body_mint_requires_winner :
  forall (s : ReplayState) (b : BlockAbs) (s' : ReplayState),
    apply_block_authorized s b = Some s' ->
    body_has_scheduled_mint s (blk_txs b) 0 = true ->
    cm_draw_won (blk_mint b) = true /\ res_mint_authorized b = true.
Proof.
  intros s b s' H Hhas.
  split.
  - destruct (cm_draw_won (blk_mint b)) eqn:Ewon; [reflexivity|].
    rewrite (no_winner_forbids_scheduled_mint s b s' H Ewon) in Hhas.
    discriminate Hhas.
  - destruct (res_mint_authorized b) eqn:Eauth; [reflexivity|].
    rewrite (schedule_exhausted_forbids_scheduled_mint s b s' H Eauth) in Hhas.
    discriminate Hhas.
Qed.

(** Covenant-funded mint, stated on [cvg_mint_shape] itself. The first body transaction is
    enough to exhibit the coupling; [body_mint_requires_winner] covers any position. *)
Theorem cvg_mint_requires_winner :
  forall (s : ReplayState) (b : BlockAbs) (s' : ReplayState)
         (t : TxAbs) (rest : list TxAbs) (ins : list TxOut),
    apply_block_authorized s b = Some s' ->
    blk_txs b = t :: rest ->
    gather_inputs (rs_utxo s) (tx_spends t) = Some ins ->
    cvg_mint_shape t ins (apply_tx_mint_reward s) = true ->
    cm_draw_won (blk_mint b) = true /\ res_mint_authorized b = true.
Proof.
  intros s b s' t rest ins H Hbtx Hg Hshape.
  destruct (apply_block_authorized_inv s b s' H) as [_ [_ Hbody]].
  destruct (apply_block_body_has_txs s b s' Hbody) as [s_tx Htxs].
  rewrite Hbtx in Htxs.
  apply apply_txs_cons_inv in Htxs.
  destruct Htxs as [s1 [Hat _]].
  apply (body_mint_requires_winner s b s' H).
  rewrite Hbtx. simpl. rewrite Hg, Hat.
  unfold is_scheduled_mint_shape. rewrite Hshape.
  destruct (res_mint_shape t ins (apply_tx_mint_reward s)); reflexivity.
Qed.

(** With the count coupled to the body, the obligation's [n <= 1] becomes a bound on the
    transactions: a block body carries at most one scheduled mint, so body RES creation is
    bounded by a single [W(h)] rather than by an unconstrained multiple of it. *)
Theorem authorized_body_res_net_bounded_by_one_reward :
  forall (s : ReplayState) (b : BlockAbs) (s' : ReplayState),
    apply_block_authorized s b = Some s' ->
    exists (d : Z) (n : nat),
      txs_res_net_and_mints s (blk_txs b) 0 = Some (d, n) /\
      (n <= 1)%nat /\
      (d <= cb_res_mint_amount b)%Z.
Proof.
  intros s b s' H.
  destruct (apply_block_authorized_inv s b s' H) as [Hc [_ _]].
  pose proof (apply_block_authorized_implies_apply_block s b s' H) as Hab.
  destruct (apply_block_res_net_bounded_by_scheduled_mints s b s' Hab)
    as [d [n [Hnet [Hbnd _]]]].
  pose proof (apply_block_authorized_body_count s b s' H) as Hcount.
  unfold body_mint_count in Hcount. rewrite Hnet in Hcount.
  injection Hcount as Hn.
  pose proof (obligation_at_most_one_mint s b Hc) as Hle.
  rewrite <- Hn in Hle.
  pose proof (cb_res_mint_amount_nonneg b) as HWnn.
  exists d, n. split; [exact Hnet|]. split; [exact Hle|].
  assert (HnZ : (Z.of_nat n <= 1)%Z).
  { apply (Nat2Z.inj_le n 1) in Hle. simpl in Hle. exact Hle. }
  nia.
Qed.

(** * Ticket face value versus the ORU pool

    The audit's double-accounting finding, stated formally. A ticket clears the face-value
    floor, so it is worth something; and it contributes nothing to the ORU pool, so that value
    is no longer ORU. Both halves hold of the same output, which is exactly why a C(t) that
    counts ticket face value as burned ORU is consistent with this model — and why any
    accounting that also counts the ticket as circulating ORU is not. *)
Theorem cvg_ticket_birth_value_leaves_oru_pool :
  forall (ins : list TxOut) (tx : TxAbs) (o : TxOut),
    valid_cvg_create ins tx = true ->
    count_cvg (tx_creates tx) = 1%nat ->
    In o (tx_creates tx) ->
    is_cvg_covenant_output o = true ->
    (MIN_CVG_TICKET_VALUE <= out_value o)%Z /\
    txout_asset_value AssetORU o = 0%Z.
Proof.
  intros ins tx o Hcreate Hc Hin Hcvg.
  split.
  - apply (cvg_creates_value_ok_forall (tx_creates tx) o
             (valid_cvg_create_value_floor ins tx Hcreate Hc) Hin Hcvg).
  - apply cvg_ticket_not_recirculated_as_oru. exact Hcvg.
Qed.

(** And the ticket is not RES either: birth is not a mint. *)
Theorem cvg_ticket_birth_creates_no_res :
  forall (s : ReplayState) (tx : TxAbs) (idx : nat) (s' : ReplayState)
         (ins : list TxOut),
    apply_tx s tx idx = Some s' ->
    gather_inputs (rs_utxo s) (tx_spends tx) = Some ins ->
    count_cvg (tx_creates tx) = 1%nat ->
    count_asset AssetRES (tx_creates tx) = 0%nat /\
    (0 < sum_asset AssetORU ins)%Z.
Proof.
  intros s tx idx s' ins Happ Hg Hc.
  destruct (apply_tx_ticket_birth_is_funded s tx idx s' ins Happ Hg Hc)
    as [Horu [Hres _]].
  split; [exact Hres | exact Horu].
Qed.

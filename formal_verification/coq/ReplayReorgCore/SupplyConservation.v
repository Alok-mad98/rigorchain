(** PHASE-4D/4E supply conservation: mint via [ConnectBlockMint] on [apply_supply_delta]. *)
From Coq Require Import ZArith.ZArith.
From Coq Require Import List.
From Coq Require Import Lia.
Import ListNotations.
Require Import Rigor.ReplayReorgCore.ReplayState.
Require Import Rigor.ReplayReorgCore.ConnectBlockMint.
Require Import Rigor.ReplayReorgCore.StateTransition.
Require Import Rigor.ReplayReorgCore.ReorgRules.
Require Import Rigor.ReplayReorgCore.ReplayDeterminism.

Definition total_supply (s : ReplayState) : Z :=
  rs_oru_supply s + rs_res_supply s.

Definition supply_snapshot (snap : ReplaySnapshot) : Z :=
  snap_oru_supply snap + snap_res_supply snap.

Lemma option_Some_inj {A} (x y : A) : Some x = Some y -> x = y.
Proof. inversion 1. reflexivity. Qed.

Lemma option_Some_eq_iff {A} (x y : A) : Some x = Some y <-> x = y.
Proof.
  split; [apply option_Some_inj | intros ->; reflexivity].
Qed.

Lemma rollback_from_history_unique :
  forall (hist : list ReplaySnapshot) (k : nat) (snap snap' : ReplaySnapshot),
    rollback_from_history hist k = Some snap ->
    rollback_from_history hist k = Some snap' ->
    snap = snap'.
Proof.
  intros hist k snap snap' H1 H2.
  rewrite H1 in H2. apply option_Some_inj in H2. exact H2.
Qed.

Lemma replay_set_chain_preserves_supply :
  forall (s : ReplayState) (cm : ChainMeta),
    (rs_oru_supply (replay_set_chain s cm) + rs_res_supply (replay_set_chain s cm))%Z =
      (rs_oru_supply s + rs_res_supply s)%Z.
Proof.
  intros. unfold replay_set_chain. reflexivity.
Qed.

Lemma protocol_mint_delta_chain_invariant :
  forall (s : ReplayState) (cm : ChainMeta) (b : BlockAbs),
    protocol_mint_total_delta (replay_set_chain s cm) b =
      protocol_mint_total_delta s b.
Proof.
  intros. unfold protocol_mint_total_delta, protocol_oru_mint_delta,
    protocol_res_mint_delta, cb_oru_mint_delta, cb_res_mint_delta, replay_set_chain.
  reflexivity.
Qed.

Lemma supply_evolution_after_chain_delta :
  forall (s_base : ReplayState) (cm : ChainMeta) (b : BlockAbs),
    supply_evolution_eq s_base
      (apply_connectblock_supply_delta (replay_set_chain s_base cm) b) b.
Proof.
  intros s_base cm b.
  unfold supply_evolution_eq.
  rewrite apply_connectblock_supply_delta_total.
  rewrite replay_set_chain_preserves_supply.
  rewrite protocol_mint_delta_chain_invariant. reflexivity.
Qed.

Lemma replay_set_utxo_preserves_supply :
  forall (s : ReplayState) (u : UTXOSet),
    total_supply (replay_set_utxo s u) = total_supply s.
Proof.
  intros s u. unfold total_supply, replay_set_utxo. reflexivity.
Qed.

Lemma state_of_snapshot_supply :
  forall (snap : ReplaySnapshot) (hist : list ReplaySnapshot),
    total_supply (state_of_snapshot snap hist) = supply_snapshot snap.
Proof.
  intros snap hist. unfold total_supply, supply_snapshot, state_of_snapshot. reflexivity.
Qed.

Lemma replay_set_chain_preserves_total :
  forall (s : ReplayState) (cm : ChainMeta),
    total_supply (replay_set_chain s cm) = total_supply s.
Proof.
  intros s cm. unfold total_supply, replay_set_chain. reflexivity.
Qed.

Lemma apply_supply_delta_supply_eq :
  forall (s : ReplayState) (b : BlockAbs),
    supply_evolution_eq s (apply_supply_delta s b) b.
Proof.
  intros s b. unfold apply_supply_delta.
  exact (apply_connectblock_supply_delta_supply_eq s b).
Qed.

Lemma apply_tx_preserves_supply :
  forall (s : ReplayState) (tx : TxAbs) (idx : nat) (s' : ReplayState),
    apply_tx s tx idx = Some s' ->
    total_supply s' = total_supply s.
Proof.
  intros s tx idx s' H.
  apply apply_tx_some_inv in H.
  destruct H as [u [_ [_ [_ Hs]]]]. subst s'.
  rewrite replay_set_utxo_preserves_supply. reflexivity.
Qed.

Lemma apply_txs_preserves_supply :
  forall (s : ReplayState) (txs : list TxAbs) (idx : nat) (s' : ReplayState),
    apply_txs s txs idx = Some s' ->
    total_supply s' = total_supply s.
Proof.
  intros s txs idx s'.
  revert s idx. induction txs as [|t rest IH]; intros s idx H; simpl in H.
  - injection H as ->. reflexivity.
  - destruct (apply_tx s t idx) as [s1|] eqn:Hat; [|discriminate H].
    assert (H1 := apply_tx_preserves_supply s t idx s1 Hat).
    transitivity (total_supply s1); [eauto | exact H1].
Qed.

Lemma push_history_preserves_supply :
  forall (s_pre s_post : ReplayState),
    total_supply (push_history s_pre s_post) = total_supply s_post.
Proof.
  intros. unfold push_history, total_supply. reflexivity.
Qed.

Lemma push_history_supply_evolution :
  forall (s s_post : ReplayState) (b : BlockAbs),
    supply_evolution_eq s s_post b ->
    supply_evolution_eq s (push_history s s_post) b.
Proof.
  intros. unfold supply_evolution_eq, push_history. simpl. exact H.
Qed.

Lemma apply_tx_preserves_scalars :
  forall (s : ReplayState) (tx : TxAbs) (idx : nat) (s' : ReplayState),
    apply_tx s tx idx = Some s' ->
    rs_oru_supply s' = rs_oru_supply s /\
    rs_res_supply s' = rs_res_supply s.
Proof.
  intros s tx idx s' H.
  apply apply_tx_some_inv in H.
  destruct H as [u [_ [_ [_ Hs]]]]. subst s'.
  unfold replay_set_utxo. split; reflexivity.
Qed.

Lemma apply_txs_preserves_scalars :
  forall (s : ReplayState) (txs : list TxAbs) (idx : nat) (s' : ReplayState),
    apply_txs s txs idx = Some s' ->
    rs_oru_supply s' = rs_oru_supply s /\
    rs_res_supply s' = rs_res_supply s.
Proof.
  intros s txs idx s'.
  revert s idx. induction txs as [|t rest IH]; intros s idx H; simpl in H.
  - injection H as ->. split; reflexivity.
  - destruct (apply_tx s t idx) as [s1|] eqn:Hat; [|discriminate H].
    destruct (apply_tx_preserves_scalars s t idx s1 Hat) as [Ho1 Hr1].
    destruct (IH s1 (S idx) H) as [Ho Hr].
    split; [rewrite Ho, Ho1 | rewrite Hr, Hr1]; reflexivity.
Qed.

Lemma mint_delta_scalar_invariant :
  forall (s s' : ReplayState) (b : BlockAbs),
    rs_oru_supply s' = rs_oru_supply s ->
    rs_res_supply s' = rs_res_supply s ->
    protocol_mint_total_delta s' b = protocol_mint_total_delta s b.
Proof.
  intros s s' b Ho Hr.
  unfold protocol_mint_total_delta, protocol_oru_mint_delta,
    protocol_res_mint_delta, cb_oru_mint_delta, cb_res_mint_delta, cb_oru_subsidy.
  rewrite Hr. reflexivity.
Qed.

Lemma supply_evolution_tx_base :
  forall (s s_tx : ReplayState) (b : BlockAbs) (s_post : ReplayState),
    rs_oru_supply s_tx = rs_oru_supply s ->
    rs_res_supply s_tx = rs_res_supply s ->
    supply_evolution_eq s_tx s_post b ->
    supply_evolution_eq s s_post b.
Proof.
  intros s s_tx b s_post Ho Hr Hev.
  unfold supply_evolution_eq in *.
  assert (Hd := mint_delta_scalar_invariant s s_tx b Ho Hr).
  lia.
Qed.

(** Block body applies mint only inside [apply_supply_delta] after txs. *)
Lemma apply_block_body_supply_evolution :
  forall (s : ReplayState) (b : BlockAbs) (s' : ReplayState),
    apply_block_body s b = Some s' ->
    supply_evolution_eq s s' b.
Proof.
  intros s b s' H.
  unfold apply_block_body in H.
  destruct (apply_txs s (blk_txs b) 0) as [s_tx|] eqn:Ht; [|discriminate H].
  apply option_Some_inj in H. subst s'.
  set (cm' :=
    {| cm_height := hdr_height (blk_header b);
       cm_chainwork := hdr_chainwork (blk_header b);
       cm_tip_hash := hash32_placeholder |}).
  assert (Hev := supply_evolution_after_chain_delta s_tx cm' b).
  assert (Hsc := apply_txs_preserves_scalars s (blk_txs b) 0 s_tx Ht).
  destruct Hsc as [Ho Hr].
  assert (Hev' := supply_evolution_tx_base s s_tx b
    (apply_supply_delta (replay_set_chain s_tx cm') b) Ho Hr Hev).
  unfold apply_supply_delta in Hev'.
  apply push_history_supply_evolution. exact Hev'.
Qed.

(** REQUIRED-6: block connect changes supply only by protocol mint deltas. *)
Theorem apply_block_supply_correct :
  forall (s : ReplayState) (b : BlockAbs) (s' : ReplayState),
    apply_block s b = Some s' ->
    supply_evolution_eq s s' b.
Proof.
  intros s b s' H.
  unfold apply_block in H.
  destruct (valid_block_connect s b) eqn:Hc; [|discriminate H].
  apply apply_block_body_supply_evolution. exact H.
Qed.

(** REQUIRED-3 (+ single-step corollary). *)
Theorem supply_deterministic :
  forall (s : ReplayState) (b : BlockAbs) (s1 s2 : ReplayState),
    apply_block s b = Some s1 ->
    apply_block s b = Some s2 ->
    total_supply s1 = total_supply s2.
Proof.
  intros s b s1 s2 H1 H2.
  assert (E : s1 = s2) by (eapply apply_block_deterministic; eauto).
  subst s2. reflexivity.
Qed.

Theorem valid_chain_supply_deterministic :
  forall (s0 : ReplayState) (blocks : list BlockAbs) (s1 s2 : ReplayState),
    valid_block_sequence s0 blocks = true ->
    fold_apply_blocks s0 blocks = Some s1 ->
    fold_apply_blocks s0 blocks = Some s2 ->
    total_supply s1 = total_supply s2.
Proof.
  intros s0 blocks s1 s2 _ H1 H2.
  congruence.
Qed.

(** REQUIRED-4: supply never decreases (authorized mint ≥ 0). *)
Theorem supply_monotonic :
  forall (s : ReplayState) (b : BlockAbs) (s' : ReplayState),
    apply_block s b = Some s' ->
    (total_supply s <= total_supply s')%Z.
Proof.
  intros s b s' H.
  assert (Heq := apply_block_supply_correct s b s' H).
  unfold supply_evolution_eq in Heq.
  unfold total_supply in Heq |- *.
  rewrite Heq.
  pose proof (protocol_mint_total_delta_non_negative s b) as Hmint.
  lia.
Qed.

(** Cumulative authorized mint along an executed block list. *)
Fixpoint fold_protocol_mint_from (s : ReplayState) (blocks : list BlockAbs) : Z :=
  match blocks with
  | nil => 0%Z
  | b :: rest =>
      match apply_block s b with
      | None => 0%Z
      | Some s' =>
          (protocol_mint_total_delta s b + fold_protocol_mint_from s' rest)%Z
      end
  end.

Lemma total_supply_add_mint :
  forall (s s' : ReplayState) (b : BlockAbs),
    supply_evolution_eq s s' b ->
    total_supply s' = (total_supply s + protocol_mint_total_delta s b)%Z.
Proof.
  intros s s' b H. unfold supply_evolution_eq, total_supply in *.
  lia.
Qed.

Theorem valid_sequence_supply_accumulation :
  forall (s0 : ReplayState) (blocks : list BlockAbs) (s_end : ReplayState),
    valid_block_sequence s0 blocks = true ->
    fold_apply_blocks s0 blocks = Some s_end ->
    total_supply s_end =
      (total_supply s0 + fold_protocol_mint_from s0 blocks)%Z.
Proof.
  intros s0 blocks s_end Hseq Hfold.
  revert s0 Hseq Hfold.
  induction blocks as [|b rest IH]; intros s0 Hseq Hfold; simpl in *.
  - inversion Hfold. subst s_end. simpl. lia.
  - simpl in Hseq, Hfold.
    destruct (valid_block_connect s0 b) eqn:Hc; [|discriminate Hseq].
    destruct (apply_block_body s0 b) as [s1|] eqn:Hb; [|discriminate Hseq].
    destruct (valid_block_sequence s1 rest) eqn:Hrest; [|discriminate Hseq].
    assert (Hat : apply_block s0 b = Some s1).
    { unfold apply_block. rewrite Hc. exact Hb. }
    rewrite Hat in Hfold. simpl in Hfold.
    destruct (fold_apply_blocks s1 rest) eqn:Hf; [|discriminate Hfold].
    inversion Hfold. subst s_end.
    assert (Hev := apply_block_body_supply_evolution s0 b s1 Hb).
    assert (IHrest := IH s1 Hrest Hf).
    assert (Hstep := total_supply_add_mint s0 s1 b Hev).
    rewrite IHrest, Hstep.
    simpl fold_protocol_mint_from. rewrite Hat. lia.
Qed.

(** REQUIRED-5: rollback restores snapshot scalar supplies.

    Stated operationally: [k = 0] is identity on supply; [k > 0] reloads
    [state_of_snapshot] whose ORU/RES scalars match the rolled-back snapshot. *)
Theorem rollback_preserves_supply :
  forall (s : ReplayState) (k : nat) (s' : ReplayState),
    rollback s k = Some s' ->
    match k with
    | O => total_supply s' = total_supply s
    | S k' =>
        exists (snap : ReplaySnapshot),
          rollback_from_history (rs_history s) k = Some snap /\
          s' = state_of_snapshot snap (skipn k (rs_history s)) /\
          total_supply s' = supply_snapshot snap
    end.
Proof.
  intros s k s' Hrb.
  unfold rollback in Hrb.
  destruct k as [|k'].
  - simpl in Hrb. apply option_Some_inj in Hrb. subst s'. reflexivity.
  - simpl in Hrb.
    destruct (rollback_from_history (rs_history s) (S k')) as [snap|] eqn:Hhist;
      [|discriminate Hrb].
    inversion Hrb. subst s'.
    exists snap. simpl skipn. repeat split; eauto.
Qed.

(** Corollary: any snapshot witness for the same rollback depth agrees on supply. *)
Theorem rollback_snapshot_supply_unique :
  forall (s : ReplayState) (k : nat) (s' : ReplayState)
         (snap snap' : ReplaySnapshot),
    rollback s k = Some s' ->
    rollback_from_history (rs_history s) k = Some snap ->
    rollback_from_history (rs_history s) k = Some snap' ->
    supply_snapshot snap = supply_snapshot snap'.
Proof.
  intros s k s' snap snap' Hrb Hsnap Hsnap'.
  assert (Heq : snap = snap') by (eapply rollback_from_history_unique; eauto).
  subst snap'. reflexivity.
Qed.

(** Supply never decreases across a replayed block suffix. *)
Lemma fold_apply_blocks_supply_monotonic :
  forall (blocks : list BlockAbs) (s s' : ReplayState),
    fold_apply_blocks s blocks = Some s' ->
    (total_supply s <= total_supply s')%Z.
Proof.
  induction blocks as [|b rest IH]; intros s s' Hfold; simpl in Hfold.
  - inversion Hfold. subst s'. lia.
  - destruct (apply_block s b) as [s1|] eqn:Hb; [|discriminate Hfold].
    pose proof (supply_monotonic s b s1 Hb) as Hstep.
    pose proof (IH s1 s' Hfold) as Hrest.
    lia.
Qed.

(** ----------------------------------------------------------------------
    SECURITY-001-F / F-4.

    Both theorems below previously concluded [total_supply s_end =
    total_supply s_end] — a reflexivity that asserts nothing. They now state
    the property the names promise: the tip supply reached by
    rollback-then-replay agrees with the tip supply reached by replaying the
    candidate directly, and supply does not decrease across the replayed
    suffix.
    ---------------------------------------------------------------------- *)

(** Reorg + suffix replay reaches the same tip supply (PHASE-4C commutativity). *)
Theorem reorg_replay_supply_equal :
  forall (s : ReplayState) (k : nat) (c : ChainCandidate)
         (blocks_after : list BlockAbs) (s_end : ReplayState),
    rollback s k = Some (cc_state c) ->
    fold_apply_blocks (cc_state c) blocks_after = Some s_end ->
    forall (s_end' s_reorg_tip : ReplayState),
      reorg_to_candidate s k c = Some s_end' ->
      fold_apply_blocks s_end' blocks_after = Some s_reorg_tip ->
      total_supply s_reorg_tip = total_supply s_end.
Proof.
  intros s k c blocks_after s_end Hrb Hfold s_end' s_reorg_tip Hreorg Hfold'.
  unfold reorg_to_candidate in Hreorg. rewrite Hrb in Hreorg.
  apply option_Some_inj in Hreorg. subst s_end'.
  rewrite Hfold in Hfold'. apply option_Some_inj in Hfold'. subst s_reorg_tip.
  reflexivity.
Qed.

Theorem reorg_replay_preserves_winning_supply :
  forall (s : ReplayState) (k : nat) (c : ChainCandidate)
         (blocks_after : list BlockAbs) (s_end : ReplayState),
    rollback s k = Some (cc_state c) ->
    fold_apply_blocks (cc_state c) blocks_after = Some s_end ->
    exists (s_end' : ReplayState),
      reorg_to_candidate s k c = Some s_end' /\
      fold_apply_blocks s_end' blocks_after = Some s_end /\
      total_supply s_end' = total_supply (cc_state c) /\
      (total_supply s_end' <= total_supply s_end)%Z.
Proof.
  intros s k c blocks_after s_end Hrb Hfold.
  exists (cc_state c).
  split; [| split; [| split]].
  - unfold reorg_to_candidate. rewrite Hrb. reflexivity.
  - exact Hfold.
  - reflexivity.
  - exact (fold_apply_blocks_supply_monotonic blocks_after (cc_state c) s_end Hfold).
Qed.

Theorem genesis_supply_zero :
  total_supply init_replay_state = 0%Z.
Proof.
  unfold total_supply, init_replay_state. reflexivity.
Qed.

(** * SECURITY-001-F G-2 — ORU issuance is bounded by the schedule

    G-2 recorded that the model placed no upper bound on [cm_oru_subsidy] and that
    [valid_block_connect] never inspected it, so a block could mint arbitrary ORU. Production
    always rejected that (`bad-cb-amount`, `core/src/validation.cpp`); the defect was in the
    model. [valid_block_connect] now carries [cb_oru_subsidy_ok] and the consequence is proved
    here.

    The finding's stated target was [total_oru_supply chain <= ORU_MAX_SUPPLY]. That statement is
    false for RIGOR and cannot be proved: the subsidy halves only three times and then pays 125
    ORU per block forever, so ORU has tail emission and no constant cap — a deliberate divergence
    from Bitcoin, asserted in `rigor_consensus_lock.h` and commented as "unlimited cumulative ORU"
    in [GetBlockSubsidy]. The correct statement, and the one that actually rules out the failure
    mode G-2 was worried about, is that issuance is bounded per block, hence linear in chain
    length rather than attacker-controlled. *)

Lemma apply_block_body_oru_evolution :
  forall (s : ReplayState) (b : BlockAbs) (s' : ReplayState),
    apply_block_body s b = Some s' ->
    rs_oru_supply s' = (rs_oru_supply s + cb_oru_mint_delta s b)%Z.
Proof.
  intros s b s' H.
  unfold apply_block_body in H.
  destruct (apply_txs s (blk_txs b) 0) as [s_tx|] eqn:Ht; [|discriminate H].
  apply option_Some_inj in H. subst s'.
  destruct (apply_txs_preserves_scalars s (blk_txs b) 0 s_tx Ht) as [Ho _].
  unfold push_history, apply_supply_delta, apply_connectblock_supply_delta,
         replay_set_chain, cb_oru_mint_delta.
  simpl. rewrite Ho. reflexivity.
Qed.

(** One block adds at most one genesis-era subsidy. *)
Lemma apply_block_oru_bounded :
  forall (s : ReplayState) (b : BlockAbs) (s' : ReplayState),
    apply_block s b = Some s' ->
    (rs_oru_supply s' <= rs_oru_supply s + cb_ORU_INITIAL_SUBSIDY)%Z.
Proof.
  intros s b s' H.
  unfold apply_block in H.
  destruct (valid_block_connect s b) eqn:Hc; [|discriminate H].
  rewrite (apply_block_body_oru_evolution s b s' H).
  pose proof (valid_block_connect_bounds_oru_mint s b Hc). lia.
Qed.

(** n blocks add at most n subsidies. Attacker-controlled issuance is ruled out: the only lever
    left is producing blocks, which costs proof of work. *)
Theorem oru_supply_bounded_by_block_count :
  forall (bs : list BlockAbs) (s s' : ReplayState),
    fold_apply_blocks s bs = Some s' ->
    (rs_oru_supply s' <= rs_oru_supply s + Z.of_nat (length bs) * cb_ORU_INITIAL_SUBSIDY)%Z.
Proof.
  induction bs as [|b rest IH]; intros s s' H; simpl in H.
  - apply option_Some_inj in H. subst s'. simpl. lia.
  - destruct (apply_block s b) as [s1|] eqn:Hb; [|discriminate H].
    pose proof (apply_block_oru_bounded s b s1 Hb) as Hstep.
    pose proof (IH s1 s' H) as Hrest.
    simpl (length (b :: rest)). rewrite Nat2Z.inj_succ. lia.
Qed.

(** The cap that does exist. RES is capped and ORU is not; stating both together is what keeps
    the asymmetry from reading as an oversight.

    RFC-017 dual insurance: [max_issuance_within_cap] is the finite-sum identity;
    [valid_res_cap] is the explicit connect reject; this theorem is the redundant
    delta-clamp restatement and is retained unchanged. *)
Theorem res_mint_respects_cap :
  forall (s : ReplayState) (b : BlockAbs),
    (rs_res_supply s <= cb_MAX_RES_SUPPLY)%Z ->
    (rs_res_supply s + cb_res_mint_delta s b <= cb_MAX_RES_SUPPLY)%Z.
Proof.
  intros s b Hcap. unfold cb_res_mint_delta.
  destruct (cm_draw_won (blk_mint b)); [|lia].
  destruct (Z.leb (rs_res_supply s + cb_res_mint_amount b) cb_MAX_RES_SUPPLY) eqn:Hle.
  - apply Z.leb_le in Hle. exact Hle.
  - lia.
Qed.

(** Successful connect already implies the pre-state is under the cap (the
    reject path), so the post-state is under the cap by the clamp. *)
Theorem apply_block_res_within_cap :
  forall (s : ReplayState) (b : BlockAbs) (s' : ReplayState),
    apply_block s b = Some s' ->
    (rs_res_supply s' <= cb_MAX_RES_SUPPLY)%Z.
Proof.
  intros s b s' H.
  unfold apply_block in H.
  destruct (valid_block_connect s b) eqn:Hc; [|discriminate H].
  pose proof (valid_block_connect_res_cap s b Hc) as Hcapb.
  pose proof (valid_res_cap_implies_pre_le_cap s b Hcapb) as Hpre.
  pose proof (res_mint_respects_cap s b Hpre) as Hpost.
  unfold apply_block_body in H.
  destruct (apply_txs s (blk_txs b) 0) as [s_tx|] eqn:Ht; [|discriminate H].
  apply option_Some_inj in H. subst s'.
  destruct (apply_txs_preserves_scalars s (blk_txs b) 0 s_tx Ht) as [_ Hr].
  unfold push_history, apply_supply_delta, apply_connectblock_supply_delta,
         replay_set_chain. simpl.
  rewrite Hr. exact Hpost.
Qed.

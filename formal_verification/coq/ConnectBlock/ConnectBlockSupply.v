(** ConnectBlock/ConnectBlockSupply.v
    PHASE-4E: bridge ConnectBlock scaffold semantics to replay supply proofs. *)
From Coq Require Import ZArith.ZArith.
From Coq Require Import Lia.
Require Import Rigor.State.
Require Import Rigor.ConnectBlock.
Require Import Rigor.ReplayReorgCore.ReplayState.
Require Import Rigor.ReplayReorgCore.ConnectBlockMint.
Require Import Rigor.ReplayReorgCore.StateTransition.
Require Import Rigor.ReplayReorgCore.ReorgRules.
Require Import Rigor.ReplayReorgCore.ReplayDeterminism.
Require Import Rigor.ReplayReorgCore.SupplyConservation.

(** Constants align with [ConnectBlock.v] scaffold. *)
Lemma cb_res_mint_amount_eq : cb_RES_MINT_AMOUNT = RES_MINT_AMOUNT.
Proof. reflexivity. Qed.

Lemma cb_max_res_supply_eq : cb_MAX_RES_SUPPLY = MAX_RES_SUPPLY.
Proof. reflexivity. Qed.

(** Project replay scalars into the PHASE-3 [State] slice. *)
Definition replay_to_scaffold_state (s : ReplayState) : State :=
  {| ORU_supply := rs_oru_supply s;
     RES_supply := rs_res_supply s;
     current_height := Z.of_nat (cm_height (rs_chain s));
     last_hash := 0%Z |}.

Definition scaffold_res_delta (st : State) (draw_won : bool) : Z :=
  if draw_won then
    if Z.leb (RES_supply st + RES_MINT_AMOUNT) MAX_RES_SUPPLY then
      RES_MINT_AMOUNT
    else
      0%Z
  else
    0%Z.

Definition scaffold_apply_res_mint (st : State) (draw_won : bool) : State :=
  {| ORU_supply := ORU_supply st;
     RES_supply := (RES_supply st + scaffold_res_delta st draw_won)%Z;
     current_height := current_height st;
     last_hash := last_hash st |}.

Lemma replay_to_scaffold_res_supply (s : ReplayState) :
  RES_supply (replay_to_scaffold_state s) = rs_res_supply s.
Proof. reflexivity. Qed.

Lemma scaffold_res_delta_eq_cb (s : ReplayState) (b : BlockAbs) :
  scaffold_res_delta (replay_to_scaffold_state s) (cm_draw_won (blk_mint b)) =
    cb_res_mint_delta s b.
Proof.
  intros. unfold scaffold_res_delta, cb_res_mint_delta.
  destruct (cm_draw_won (blk_mint b)); [|reflexivity].
  rewrite replay_to_scaffold_res_supply.
  unfold cb_RES_MINT_AMOUNT, cb_MAX_RES_SUPPLY, RES_MINT_AMOUNT, MAX_RES_SUPPLY.
  reflexivity.
Qed.

(** Synthetic block carrying ConnectBlock mint authorization. *)
Definition block_with_mint (hdr : BlockHeaderAbs) (txs : list TxAbs)
  (spec : ConnectMintSpec) : BlockAbs :=
  {| blk_header := hdr; blk_txs := txs; blk_mint := spec |}.

(** ConnectBlock scaffold RES path matches replay [cb_res_mint_delta]. *)
Lemma connectblock_scaffold_res_matches_replay :
  forall (s : ReplayState) (st : State) (b : BlockAbs),
    st = replay_to_scaffold_state s ->
    RES_supply (scaffold_apply_res_mint st (cm_draw_won (blk_mint b))) =
      (rs_res_supply s + cb_res_mint_delta s b)%Z.
Proof.
  intros s st b Heq.
  unfold scaffold_apply_res_mint.
  rewrite Heq.
  rewrite replay_to_scaffold_res_supply.
  rewrite scaffold_res_delta_eq_cb. reflexivity.
Qed.

(** PHASE-4E: [apply_block] changes supply only via ConnectBlock mint rules. *)
Theorem apply_block_supply_correct :
  forall (s : ReplayState) (b : BlockAbs) (s' : ReplayState),
    apply_block s b = Some s' ->
    supply_evolution_eq s s' b.
Proof.
  exact SupplyConservation.apply_block_supply_correct.
Qed.

(** Supply never decreases on successful [apply_block]. *)
Theorem supply_monotonic_cb :
  forall (s : ReplayState) (b : BlockAbs) (s' : ReplayState),
    apply_block s b = Some s' ->
    (total_supply s <= total_supply s')%Z.
Proof.
  exact SupplyConservation.supply_monotonic.
Qed.

(** Rollback + winning-chain replay preserve tip supply.

    SECURITY-001-F / F-4: the final conjunct previously read
    [total_supply s_end = total_supply s_end], a reflexivity that asserts
    nothing. It now mirrors the strengthened
    [SupplyConservation.reorg_replay_preserves_winning_supply]. *)
Theorem reorg_preserves_supply :
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
  exact SupplyConservation.reorg_replay_preserves_winning_supply.
Qed.

Theorem supply_deterministic_cb :
  forall (s : ReplayState) (b : BlockAbs) (s1 s2 : ReplayState),
    apply_block s b = Some s1 ->
    apply_block s b = Some s2 ->
    total_supply s1 = total_supply s2.
Proof.
  exact SupplyConservation.supply_deterministic.
Qed.

(** Default-mint blocks recover PHASE-4D zero-delta behavior. *)
Theorem default_mint_apply_block_preserves_supply :
  forall (s : ReplayState) (hdr : BlockHeaderAbs) (txs : list TxAbs)
         (s' : ReplayState),
    apply_block s (block_with_mint hdr txs default_connect_mint) = Some s' ->
    total_supply s' = total_supply s.
Proof.
  intros s hdr txs s' H.
  set (b := block_with_mint hdr txs default_connect_mint).
  assert (Heq := apply_block_supply_correct s b s' H).
  assert (Hz := default_mint_zero_delta s hdr txs).
  unfold supply_evolution_eq, total_supply in Heq |- *.
  unfold b, block_with_mint in Heq. rewrite Hz in Heq. lia.
Qed.

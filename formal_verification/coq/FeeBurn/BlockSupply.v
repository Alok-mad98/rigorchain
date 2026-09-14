(** FeeBurn/BlockSupply.v
    RFC-018 circulating ORU supply vs the RFC-002 ledger scalar.

    Circulating (UTXO) view, after the miner claims the complementary fee
    share and coinbase pays the subsidy:

      Δ circulating_ORU = subsidy − destroyed_fee − converge

    When there is no Convergence burn in the block (this RFC's typical
    transfer-only case):

      Δ circulating_ORU = coinbase subsidy − destroyed fee

    C++ [m_chain_oru_total] continues to track the RFC-002 ledger scalar
    [S' = S + subsidy − converge] and does *not* subtract fee burns
    (RFC-018 §5). Circulating UTXO ORU is then ≤ that scalar.

    Not a C++ [ConnectBlock] refinement. *)
From Coq Require Import ZArith.ZArith.
From Coq Require Import Lia.
From Coq Require Import List.
Import ListNotations.
Require Import Rigor.FeeBurn.Accounting.
Require Import Rigor.FeeBurn.AssetDomain.
Require Import Rigor.FeeBurn.ReplaySafety.

Local Open Scope Z_scope.

(** * Block-level deltas *)

Definition ledger_oru_delta (subsidy converge : Z) : Z :=
  subsidy - converge.

Definition circulating_oru_delta (subsidy converge burn : Z) : Z :=
  subsidy - converge - burn.

Lemma circulating_delta_is_subsidy_minus_burn :
  forall subsidy burn,
    circulating_oru_delta subsidy 0 burn = subsidy - burn.
Proof. intros. unfold circulating_oru_delta. lia. Qed.

Lemma circulating_le_ledger :
  forall subsidy converge burn,
    0 <= burn ->
    circulating_oru_delta subsidy converge burn
      <= ledger_oru_delta subsidy converge.
Proof. intros. unfold circulating_oru_delta, ledger_oru_delta. lia. Qed.

Lemma coinbase_cap_is_fees_minus_burn_plus_subsidy :
  forall fees burn subsidy,
    coinbase_oru_cap fees burn subsidy = (fees - burn) + subsidy.
Proof. intros. reflexivity. Qed.

(** * Apply a block: fee burns then subsidy (converge optional) *)

Definition apply_block_circulating
  (s : FeeBurnState) (txs : list FeeTx) (subsidy converge : Z)
  : option FeeBurnState :=
  match apply_txs s txs with
  | None => None
  | Some s1 =>
      Some {| fbs_oru := fbs_oru s1 + subsidy - converge;
              fbs_res := fbs_res s1;
              fbs_burn := fbs_burn s1;
              fbs_miner := fbs_miner s1 |}
  end.

Lemma apply_block_circulating_res_unchanged :
  forall s txs subsidy converge s',
    apply_block_circulating s txs subsidy converge = Some s' ->
    fbs_res s' = fbs_res s.
Proof.
  intros s txs subsidy converge s' H.
  unfold apply_block_circulating in H.
  destruct (apply_txs s txs) as [s1|] eqn:Ha; [|discriminate H].
  inversion H; subst s'; simpl.
  eapply apply_txs_res_unchanged; eassumption.
Qed.

Lemma apply_block_circulating_supply :
  forall s txs subsidy converge s',
    apply_block_circulating s txs subsidy converge = Some s' ->
    fbs_oru s' = fbs_oru s + subsidy - converge - txs_burn txs
    /\ fbs_res s' = fbs_res s
    /\ fbs_burn s' = fbs_burn s + txs_burn txs.
Proof.
  intros s txs subsidy converge s' H.
  unfold apply_block_circulating in H.
  destruct (apply_txs s txs) as [s1|] eqn:Ha; [|discriminate H].
  destruct (apply_txs_burn_acc txs s s1 Ha) as [Hb [Ho [_ Hr]]].
  inversion H; subst s'; simpl.
  repeat split; lia.
Qed.

(** Property 2: ORU circulating change is subsidy − destroyed fee
    (no Convergence in the block). *)
Theorem oru_circulating_change_is_subsidy_minus_burn :
  forall s txs subsidy s',
    apply_block_circulating s txs subsidy 0 = Some s' ->
    fbs_oru s' - fbs_oru s = subsidy - txs_burn txs.
Proof.
  intros s txs subsidy s' H.
  destruct (apply_block_circulating_supply s txs subsidy 0 s' H) as [Ho _].
  lia.
Qed.

Theorem oru_circulating_change_with_converge :
  forall s txs subsidy converge s',
    apply_block_circulating s txs subsidy converge = Some s' ->
    fbs_oru s' - fbs_oru s =
      circulating_oru_delta subsidy converge (txs_burn txs).
Proof.
  intros s txs subsidy converge s' H.
  destruct (apply_block_circulating_supply s txs subsidy converge s' H)
    as [Ho _].
  unfold circulating_oru_delta. lia.
Qed.

Lemma apply_block_circulating_deterministic :
  forall s txs subsidy converge s1 s2,
    apply_block_circulating s txs subsidy converge = Some s1 ->
    apply_block_circulating s txs subsidy converge = Some s2 ->
    s1 = s2.
Proof.
  intros s txs subsidy converge s1 s2 H1 H2.
  rewrite H1 in H2. inversion H2. reflexivity.
Qed.

Lemma apply_block_circulating_replay :
  forall s txs subsidy converge s1 s2,
    apply_block_circulating s txs subsidy converge = Some s1 ->
    apply_block_circulating s txs subsidy converge = Some s2 ->
    fbs_burn s1 = fbs_burn s2
    /\ fbs_oru s1 = fbs_oru s2
    /\ fbs_res s1 = fbs_res s2.
Proof.
  intros s txs subsidy converge s1 s2 H1 H2.
  pose proof (apply_block_circulating_deterministic
                s txs subsidy converge s1 s2 H1 H2) as Heq.
  subst s2. repeat split; reflexivity.
Qed.

(** Rollback of the fee-tx list still restores pre-block circulating
    scalars; subsidy is not part of the fee snapshot. Connect then
    rollback of the fee state is identity ([ReplaySafety]). *)
Lemma circulating_rollback_restores_pre_fee_state :
  forall s txs s' snap,
    apply_block s txs = Some (s', snap) ->
    rollback s' snap = s.
Proof.
  intros. eapply rollback_restores_fee_burn_state; eassumption.
Qed.

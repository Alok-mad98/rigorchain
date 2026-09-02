(** fee-burn/Properties.v
    RFC-018 named properties (model theorems).

    1. Fee-burn determinism
    2. ORU circulating supply conservation
    3. RES isolation
    4. Reorg safety
    5. Replay safety
    6. Precision consistency

    Not a C++ refinement. [equivalence_claimed = false]. *)
From Coq Require Import ZArith.ZArith.
From Coq Require Import Lia.
From Coq Require Import List.
Import ListNotations.
Require Import Rigor.FeeBurn.Accounting.
Require Import Rigor.FeeBurn.AssetDomain.
Require Import Rigor.FeeBurn.ReplaySafety.
Require Import Rigor.FeeBurn.Precision.
Require Import Rigor.FeeBurn.BlockSupply.
Require Import Rigor.FeeBurn.CirculatingSemantics.

Local Open Scope Z_scope.

(** * 1. Fee burn determinism

    Same transaction (fee, class) in any block context yields the same
    ORU burn amount. Height / nBits are unused: RFC-018 is genesis-active. *)
Theorem fee_burn_determinism :
  forall (fee : Z) (c : TxFeeClass) (h1 h2 : nat) (n1 n2 : Z),
    fee_burn_in_context fee c h1 n1 = fee_burn_in_context fee c h2 n2.
Proof. exact fee_burn_deterministic. Qed.

Theorem same_transaction_same_block_same_burn :
  forall fee c h n,
    fee_burn_in_context fee c h n = calculate_fee_burn fee c.
Proof. intros. apply fee_burn_function_of_fee_and_class. Qed.

Theorem burn_share_is_a_function_of_fee :
  forall fee,
    oru_res_transfer_fee_burn_share fee =
      if fee <=? 0 then 0
      else (fee * ORU_RES_TRANSFER_FEE_BURN_NUM)
             / ORU_RES_TRANSFER_FEE_BURN_DEN.
Proof. reflexivity. Qed.

(** * 2. Supply conservation

    Circulating ORU change = coinbase subsidy − destroyed fee
    (no Convergence in the block). *)
Theorem supply_conservation :
  forall s txs subsidy s',
    apply_block_circulating s txs subsidy 0 = Some s' ->
    fbs_oru s' - fbs_oru s = subsidy - txs_burn txs.
Proof. exact oru_circulating_change_is_subsidy_minus_burn. Qed.

Theorem fee_splits_into_burn_and_miner :
  forall fee,
    ORU_fee fee = ORU_burn fee + ORU_miner_fee fee.
Proof. exact fee_conservation. Qed.

(** * 3. RES isolation

    RES-transfer fee burn cannot mint RES, burn RES, or alter the RES cap. *)
Theorem res_isolation :
  forall s fee s',
    apply_fee_tx s TxFeeResTransfer fee = Some s' ->
    as_res s' = as_res s
    /\ as_res s' - as_res s = 0
    /\ RES_MAX_SUPPLY = 21000000 * RES_COIN.
Proof. exact res_transfer_fee_burn_cannot_touch_res. Qed.

Theorem res_isolation_block :
  forall s txs subsidy converge s',
    apply_block_circulating s txs subsidy converge = Some s' ->
    fbs_res s' = fbs_res s.
Proof. exact apply_block_circulating_res_unchanged. Qed.

(** * 4. Reorg safety

    Disconnect restores ORU supply and RES balances. The new branch's
    burn replaces the old branch's; A+B is not accumulated. *)
Theorem reorg_safety :
  forall s s' txs snap,
    apply_block s txs = Some (s', snap) ->
    fbs_oru (rollback s' snap) = fbs_oru s
    /\ fbs_res (rollback s' snap) = fbs_res s.
Proof.
  intros s s' txs snap H.
  destruct (rollback_restores_oru_res_burn s s' txs snap H)
    as [Ho [Hr _]].
  split; [exact Ho | exact Hr].
Qed.

Theorem reorg_safety_new_branch_burn :
  forall s sA sB txsA txsB snapA,
    apply_block s txsA = Some (sA, snapA) ->
    apply_txs (rollback sA snapA) txsB = Some sB ->
    fbs_burn sB = fbs_burn s + txs_burn txsB
    /\ fbs_res sB = fbs_res s.
Proof.
  intros s sA sB txsA txsB snapA HA HB.
  pose proof (reorg_burn_equals_new_branch s sA sB txsA txsB snapA HA HB)
    as Hb.
  pose proof (rollback_restores_fee_burn_state s sA txsA snapA HA) as Hr.
  rewrite Hr in HB.
  pose proof (apply_txs_res_unchanged txsB s sB HB) as Hs.
  split; [exact Hb | exact Hs].
Qed.

(** * 5. Replay safety

    The same block body, replayed from the same pre-state, burns the
    same ORU amount. *)
Theorem replay_safety :
  forall s txs s1 snap1 s2 snap2,
    apply_block s txs = Some (s1, snap1) ->
    apply_block s txs = Some (s2, snap2) ->
    fbs_burn s1 = fbs_burn s2
    /\ s1 = s2.
Proof.
  intros s txs s1 snap1 s2 snap2 H1 H2.
  destruct (replay_block_same_burn s txs s1 snap1 s2 snap2 H1 H2)
    as [Hb [_ Hs]].
  split; [exact Hb | exact Hs].
Qed.

Theorem replay_safety_circulating :
  forall s txs subsidy converge s1 s2,
    apply_block_circulating s txs subsidy converge = Some s1 ->
    apply_block_circulating s txs subsidy converge = Some s2 ->
    fbs_burn s1 = fbs_burn s2
    /\ fbs_oru s1 = fbs_oru s2
    /\ fbs_res s1 = fbs_res s2.
Proof. exact apply_block_circulating_replay. Qed.

(** * 6. Precision consistency *)
Theorem precision_consistency :
  RES_COIN = 1000000
  /\ RES_ATOMIC_UNIT = 1
  /\ RES_DISPLAY_DECIMALS = 6%nat
  /\ RES_COIN <> 100
  /\ res_display_parts RES_COIN = (1, 0)
  /\ res_display_parts (RES_COIN / 10) = (0, 100000).
Proof.
  split. apply res_coin_is_1e6.
  split. apply res_atomic_unit_is_one.
  split. apply res_display_decimals_is_six.
  split. apply res_coin_is_not_100.
  split. apply display_one_res.
  apply display_tenth_res.
Qed.

(** * 7. Supply-layer semantic boundaries

    Named theorems live in [CirculatingSemantics.v] (imported above):
    [fee_burn_cannot_enter_circulating_supply],
    [cvg2_locked_outputs_excluded_from_circulating],
    [wallet_balance_matches_spendable_utxo_model].

    Model checks only. Not C++ UTXO / wallet equivalence. *)
Check fee_burn_cannot_enter_circulating_supply.
Check cvg2_locked_outputs_excluded_from_circulating.
Check wallet_balance_matches_spendable_utxo_model.

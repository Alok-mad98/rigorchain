(** FeeBurn/AssetDomain.v
    RFC-018 asset-domain separation.

    Transaction fees are ORU-denominated (POOL-F / [nFeesRes == 0]).
    A RES transfer therefore does not modify RES supply through the
    fee path. Circulating ORU, after the miner claims the complementary
    share, decreases by the burn amount only.

    The C++ ledger scalar [m_chain_oru_total] continues to track
    [S' = S + subsidy − converge] (RFC-002) and does *not* subtract
    fee burns; circulating UTXO ORU is then ≤ that scalar. This module
    models the circulating (UTXO-net-of-claim) view.

    Not a C++ refinement. *)
From Coq Require Import ZArith.ZArith.
From Coq Require Import Lia.
From Coq Require Import Bool.
From Coq Require Import List.
Import ListNotations.
Require Import Rigor.FeeBurn.Accounting.
Require Import Rigor.FeeBurn.Precision.

Local Open Scope Z_scope.

(** Circulating supplies after fee split + miner claim of the complementary
    share. RES is untouched by fee accounting. *)
Record AssetSupply : Set := {
  as_oru : Z;
  as_res : Z
}.

Definition apply_fee_tx
  (s : AssetSupply) (c : TxFeeClass) (fee : Z) : option AssetSupply :=
  if fee <? 0 then None
  else
    Some {| as_oru := as_oru s - calculate_fee_burn fee c;
            as_res := as_res s |}.

Lemma res_supply_after_tx :
  forall (s : AssetSupply) (c : TxFeeClass) (fee : Z) (s' : AssetSupply),
    apply_fee_tx s c fee = Some s' ->
    as_res s' = as_res s.
Proof.
  intros s c fee s' H.
  unfold apply_fee_tx in H.
  destruct (fee <? 0); [discriminate H|].
  inversion H. reflexivity.
Qed.

Lemma oru_supply_after_tx :
  forall (s : AssetSupply) (c : TxFeeClass) (fee : Z) (s' : AssetSupply),
    apply_fee_tx s c fee = Some s' ->
    as_oru s' = as_oru s - calculate_fee_burn fee c.
Proof.
  intros s c fee s' H.
  unfold apply_fee_tx in H.
  destruct (fee <? 0); [discriminate H|].
  inversion H. reflexivity.
Qed.

Lemma res_transfer_does_not_modify_res_supply :
  forall (s : AssetSupply) (fee : Z) (s' : AssetSupply),
    apply_fee_tx s TxFeeResTransfer fee = Some s' ->
    as_res s' = as_res s.
Proof.
  intros. eapply res_supply_after_tx; eassumption.
Qed.

Lemma oru_only_does_not_burn :
  forall (s : AssetSupply) (fee : Z) (s' : AssetSupply),
    0 <= fee ->
    apply_fee_tx s TxFeeORUOnly fee = Some s' ->
    as_oru s' = as_oru s /\ as_res s' = as_res s.
Proof.
  intros s fee s' Hf H.
  pose proof (oru_supply_after_tx s TxFeeORUOnly fee s' H) as Ho.
  pose proof (res_supply_after_tx s TxFeeORUOnly fee s' H) as Hr.
  unfold calculate_fee_burn, is_res_transfer in Ho; simpl in Ho.
  rewrite Z.sub_0_r in Ho.
  split; [exact Ho | exact Hr].
Qed.

Lemma res_transfer_oru_decreases_by_burn :
  forall (s : AssetSupply) (fee : Z) (s' : AssetSupply),
    apply_fee_tx s TxFeeResTransfer fee = Some s' ->
    as_oru s' = as_oru s - ORU_burn fee.
Proof.
  intros s fee s' H.
  apply oru_supply_after_tx in H.
  unfold calculate_fee_burn, is_res_transfer, ORU_burn in *.
  exact H.
Qed.

Lemma apply_fee_tx_rejects_negative :
  forall s c fee, fee < 0 -> apply_fee_tx s c fee = None.
Proof.
  intros s c fee H.
  unfold apply_fee_tx.
  replace (fee <? 0) with true by (symmetry; apply Z.ltb_lt; exact H).
  reflexivity.
Qed.

Lemma apply_fee_tx_accepts_nonneg :
  forall s c fee,
    0 <= fee ->
    exists s', apply_fee_tx s c fee = Some s'.
Proof.
  intros s c fee H.
  unfold apply_fee_tx.
  replace (fee <? 0) with false by (symmetry; apply Z.ltb_ge; exact H).
  eexists. reflexivity.
Qed.

(** Conserved RES transfer: [res_in = res_out] plus an ORU fee. The fee
    path still does not touch RES; this records the joint statement. *)
Record ResTransferView : Set := {
  rtv_res_in : Z;
  rtv_res_out : Z;
  rtv_oru_fee : Z
}.

Definition valid_res_transfer_fee (t : ResTransferView) : bool :=
  (0 <=? rtv_oru_fee t)
  && (0 <=? rtv_res_in t)
  && (rtv_res_out t =? rtv_res_in t).

Lemma valid_res_transfer_preserves_res :
  forall (s : AssetSupply) (t : ResTransferView) (s' : AssetSupply),
    valid_res_transfer_fee t = true ->
    apply_fee_tx s TxFeeResTransfer (rtv_oru_fee t) = Some s' ->
    as_res s' = as_res s
    /\ rtv_res_out t = rtv_res_in t.
Proof.
  intros s t s' Hv Happ.
  unfold valid_res_transfer_fee in Hv.
  apply andb_true_iff in Hv as [_ Heq].
  apply Z.eqb_eq in Heq.
  split.
  - eapply res_supply_after_tx; eassumption.
  - exact Heq.
Qed.

Lemma valid_res_transfer_oru_net_is_burn :
  forall (s : AssetSupply) (t : ResTransferView) (s' : AssetSupply),
    valid_res_transfer_fee t = true ->
    apply_fee_tx s TxFeeResTransfer (rtv_oru_fee t) = Some s' ->
    as_oru s' = as_oru s - ORU_burn (rtv_oru_fee t)
    /\ as_res s' = as_res s.
Proof.
  intros s t s' Hv Happ.
  split.
  - eapply res_transfer_oru_decreases_by_burn; eassumption.
  - eapply res_supply_after_tx; eassumption.
Qed.

(** Mint / converge classes also leave RES supply untouched on the fee
    path (mint RES is a different channel, modelled elsewhere). *)
Lemma fee_path_never_moves_res :
  forall s c fee s',
    apply_fee_tx s c fee = Some s' ->
    as_res s' = as_res s.
Proof.
  intros. eapply res_supply_after_tx; eassumption.
Qed.

(** * RES isolation (cannot mint / burn / alter cap via the fee path) *)

Lemma fee_path_does_not_mint_res :
  forall s c fee s',
    apply_fee_tx s c fee = Some s' ->
    as_res s' - as_res s = 0.
Proof.
  intros s c fee s' H.
  pose proof (fee_path_never_moves_res s c fee s' H).
  lia.
Qed.

Lemma fee_path_does_not_burn_res :
  forall s c fee s',
    apply_fee_tx s c fee = Some s' ->
    as_res s' = as_res s.
Proof.
  intros. eapply fee_path_never_moves_res; eassumption.
Qed.

(** The RES cap is a global constant. Fee accounting has no cap field
    and does not rewrite [RES_MAX_SUPPLY]. *)
Lemma fee_path_does_not_alter_res_cap :
  forall (s : AssetSupply) (c : TxFeeClass) (fee : Z) (s' : AssetSupply),
    apply_fee_tx s c fee = Some s' ->
    RES_MAX_SUPPLY = 419430400000000.
Proof.
  intros. unfold RES_MAX_SUPPLY. reflexivity.
Qed.

Lemma fee_path_preserves_res_cap_bound :
  forall s c fee s',
    apply_fee_tx s c fee = Some s' ->
    (as_res s <= RES_MAX_SUPPLY <-> as_res s' <= RES_MAX_SUPPLY).
Proof.
  intros s c fee s' H.
  pose proof (fee_path_never_moves_res s c fee s' H) as Hr.
  rewrite Hr.
  split; intro; assumption.
Qed.

Lemma res_transfer_fee_burn_cannot_touch_res :
  forall s fee s',
    apply_fee_tx s TxFeeResTransfer fee = Some s' ->
    as_res s' = as_res s
    /\ as_res s' - as_res s = 0
    /\ RES_MAX_SUPPLY = 419430400000000.
Proof.
  intros s fee s' H.
  split; [|split].
  - eapply fee_path_does_not_burn_res; eassumption.
  - eapply fee_path_does_not_mint_res; eassumption.
  - eapply fee_path_does_not_alter_res_cap; eassumption.
Qed.

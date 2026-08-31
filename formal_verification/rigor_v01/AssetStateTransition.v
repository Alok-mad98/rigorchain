(** RIGOR v0.1 — Asset state transition (P0-023 Proofs 1 and 4).

    Unique path last step: after classification and Ed25519 authorization,
    a created/spent output is admitted into (or removed from) the UTXO
    pools. This is a *model* of CheckTransaction / CheckTxInputs pooling,
    not a refinement of [validation.cpp] ConnectBlock.

    Proof 1: [classify = AssetInvalid] ⇒ create rejected.
    Proof 4: dual-pool identity
      oru' = oru + subsidy - burn
      res' = res + valid_mint
    Ordinary transfers do not mint. Burn is ORU (converge / fees).

    Stronger dual-pool theorems remain in
    [Rigor.ReplayReorgCore.DualPoolConservation] (apply_block model).
    No [Axiom] / [Admitted]. *)
From Coq Require Import List.
From Coq Require Import Bool.Bool.
From Coq Require Import ZArith.ZArith.
Import ListNotations.
Require Import RigorV01.AssetClassification.
Require Import RigorV01.ScriptAssetBoundary.
Require Import RigorV01.Ed25519Ownership.
Require Import RigorV01.ResMintCorrectness.

(** * Create / spend gates *)

Definition create_rejected (spk : list byte) (v : Z) : bool :=
  negb (created_output_admitted spk v).

Theorem invalid_create_is_rejected :
  forall (spk : list byte) (v : Z),
    classify spk = AssetInvalid ->
    create_rejected spk v = true.
Proof.
  intros spk v H.
  unfold create_rejected.
  rewrite (invalid_cannot_enter_asset_domain spk v H).
  reflexivity.
Qed.

Definition spend_rejected (o : OwnershipPredicate) : bool :=
  negb (is_spendable_own o).

Theorem invalid_own_cannot_be_spent :
  spend_rejected OwnInvalid = true.
Proof. reflexivity. Qed.

Theorem none_own_cannot_be_spent :
  spend_rejected OwnNone = true.
Proof. reflexivity. Qed.

Theorem converge_own_cannot_be_spent :
  forall h, spend_rejected (OwnConverge h) = true.
Proof. intros. reflexivity. Qed.

(** * Proof 4 — supply identity (model)

    [valid_mint] is 0 or the scheduled W(h) after owner/draw authorization
    (that authorization is *not* this module; see ResMintObligation).
    [oru_burn] includes converge burns and fee destruction. *)
Record AssetPools := {
  pool_oru : Z;
  pool_res : Z
}.

Definition apply_block_pools (p : AssetPools)
    (oru_subsidy oru_burn valid_mint : Z) : AssetPools :=
  {| pool_oru := (pool_oru p + oru_subsidy - oru_burn)%Z;
     pool_res := (pool_res p + valid_mint)%Z |}.

Definition total_supply (p : AssetPools) : Z :=
  (pool_oru p + pool_res p)%Z.

Theorem res_supply_step :
  forall (p : AssetPools) (sub burn mint : Z),
    pool_res (apply_block_pools p sub burn mint) =
    (pool_res p + mint)%Z.
Proof. intros. reflexivity. Qed.

Theorem oru_supply_step :
  forall (p : AssetPools) (sub burn mint : Z),
    pool_oru (apply_block_pools p sub burn mint) =
    (pool_oru p + sub - burn)%Z.
Proof. intros. reflexivity. Qed.

Theorem total_supply_conservation :
  forall (p : AssetPools) (oru_subsidy oru_burn valid_mint : Z),
    total_supply (apply_block_pools p oru_subsidy oru_burn valid_mint) =
    (total_supply p + oru_subsidy + valid_mint - oru_burn)%Z.
Proof.
  intros p sub burn mint.
  unfold total_supply, apply_block_pools. simpl. ring.
Qed.

(** Zero mint and zero subsidy: only burns reduce ORU; RES is unchanged. *)
Theorem no_mint_no_subsidy_is_burn_only :
  forall (p : AssetPools) (burn : Z),
    pool_res (apply_block_pools p 0 burn 0) = pool_res p /\
    pool_oru (apply_block_pools p 0 burn 0) = (pool_oru p - burn)%Z.
Proof. intros. split; unfold apply_block_pools; simpl; ring. Qed.

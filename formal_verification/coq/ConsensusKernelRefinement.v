(** ConsensusKernelRefinement.v — P056 kernel-boundary refinement.

    Proves the C++-shaped Kernel boundary (Coq model of the production
    control flow) equals the extracted / canonical Coq Kernel.

    Scope: issuance, classify, ResolveConvergenceDraw, ledger Δ, RES
    coinbase expectation.

    Not proved: ConnectBlock, UTXO mutation, DB, PoW, Transaction Kernel,
    miner, wallet, P2P.

    Production heights satisfy [0 <= h]. C++ integer division truncates
    toward zero; Coq [Z.div] floors. Those differ for [h < 0]. This file
    does not change either formula.

      refinement_proved                    = false  (kernel-wide flag; issuance models now match RFC-020)
      equivalence_proved                   = false  (not the node)
      extracted_kernel_production          = false
      compcert_binary_verified_kernel      = false

    No [Admitted] / [Axiom] / [Parameter] / [Extract Constant]. *)

From Coq Require Import ZArith.ZArith.
From Coq Require Import Lia.
From Coq Require Import List.
From Coq Require Import Bool.Bool.
From Coq Require Import Arith.PeanoNat.
Import ListNotations.

From Rigor Require Import ResSchedule.
From Rigor Require Import RigorBytes.
From Rigor Require Import Sigma.
From Rigor Require Import SigmaPoW01.
Require Import Rigor.legacy_account_model.Types.
Require Import Rigor.legacy_account_model.Selection.
Require Import Rigor.Kernel.KernelSelection.
From Rigor.FeeBurn Require Import BlockSupply.
From RigorV01 Require Import IssuanceHeightBoundary.
From RigorV01 Require Import CanonicalIssuance.
From RigorV01 Require Import AssetClassification.

Local Open Scope Z_scope.

(** * Honesty *)

Definition refinement_proved : bool := false.
Definition equivalence_proved : bool := false.
Definition extracted_kernel_production : bool := false.
Definition extracted_kernel_linked : bool := extracted_kernel_production.
Definition compcert_binary_verified_kernel : bool := false.
Definition connectblock_refinement_proved : bool := false.
Definition utxo_refinement_proved : bool := false.
Definition db_refinement_proved : bool := false.
Definition pow_refinement_proved : bool := false.
Definition transaction_kernel_refinement_proved : bool := false.

Theorem kernel_boundary_refinement_proved : refinement_proved = false.
Proof. reflexivity. Qed.

Theorem node_equivalence_not_proved : equivalence_proved = false.
Proof. reflexivity. Qed.

Theorem extracted_kernel_not_production : extracted_kernel_production = false.
Proof. reflexivity. Qed.

Theorem extracted_kernel_not_linked : extracted_kernel_linked = false.
Proof. reflexivity. Qed.

Theorem compcert_binary_not_verified : compcert_binary_verified_kernel = false.
Proof. reflexivity. Qed.

Theorem connectblock_not_proved : connectblock_refinement_proved = false.
Proof. reflexivity. Qed.

(** * Issuance *)

Record cpp_issuance_params := {
  cpp_base : Z;
  cpp_interval : Z;
  cpp_max_shifts : Z
}.

(** Must match Consensus::ORU_FLAT_BLOCK_SUBSIDY.
    cpp_interval is the observational calendar (RES_REWARD_EPOCH), not a mint argument.
    cpp_max_shifts = 0: RFC-020 has no halvings.
    FrozenKernelParams does not apply to regtest. *)
Definition FrozenKernelParams : cpp_issuance_params :=
  {| cpp_base := 12800000000;
     cpp_interval := 210000;
     cpp_max_shifts := 0 |}.

Definition ORU_GENESIS_BLOCK_SUBSIDY_Z : Z := 0.
Definition ORU_FLAT_BLOCK_SUBSIDY_Z : Z := cpp_base FrozenKernelParams.
Definition ORU_MIN_BLOCK_SUBSIDY_Z : Z := ORU_FLAT_BLOCK_SUBSIDY_Z.
Definition ORU_SUBSIDY_CAP_HEIGHT : Z := 0.

Definition kernel_params_frozen (p : cpp_issuance_params) : Prop :=
  p = FrozenKernelParams.

Definition cpp_RigorIssuanceEpoch (h : Z) : Z :=
  if h <? 0 then 0 else h / 210000.

Definition cpp_GetBlockSubsidy (h : Z) (p : cpp_issuance_params) : Z :=
  if h <=? 0 then ORU_GENESIS_BLOCK_SUBSIDY_Z
  else cpp_base p.

Definition cpp_ResDestroyCostForEpoch (k : Z) : Z :=
  res_destroy_cost_at_height (k * RES_REWARD_EPOCH).

Definition cpp_ResDestroyCostAtHeight (h : Z) : Z :=
  res_destroy_cost_at_height h.

Record issuance_pack := {
  ip_epoch : Z;
  ip_oru : Z;
  ip_res_schedule : Z
}.

Definition cpp_issuance (h : Z) (p : cpp_issuance_params) : issuance_pack :=
  {| ip_epoch := cpp_RigorIssuanceEpoch h;
     ip_oru := cpp_GetBlockSubsidy h p;
     ip_res_schedule := cpp_ResDestroyCostAtHeight h |}.

Definition coq_issuance (h : Z) (p : cpp_issuance_params) : issuance_pack :=
  {| ip_epoch := issuance_epoch h;
     ip_oru := oru_subsidy h;
     ip_res_schedule := res_destroy_cost_at_height h |}.

Definition compute_issuance (h : Z) : issuance_pack :=
  coq_issuance h FrozenKernelParams.

Lemma cpp_epoch_is_coq :
  forall h, cpp_RigorIssuanceEpoch h = issuance_epoch h.
Proof.
  intros h. unfold cpp_RigorIssuanceEpoch, issuance_epoch, HALVING_INTERVAL.
  reflexivity.
Qed.

Lemma cpp_oru_is_coq :
  forall h,
    cpp_GetBlockSubsidy h FrozenKernelParams = oru_subsidy h.
Proof.
  intros h.
  unfold cpp_GetBlockSubsidy, oru_subsidy, FrozenKernelParams, ORU_FLAT_SATS.
  simpl. reflexivity.
Qed.

Lemma cpp_res_epoch_eq_coq :
  forall k,
    cpp_ResDestroyCostForEpoch k = res_destroy_cost_at_height (k * RES_REWARD_EPOCH).
Proof. intros k. reflexivity. Qed.

Lemma cpp_res_schedule_is_coq :
  forall h, cpp_ResDestroyCostAtHeight h = res_destroy_cost_at_height h.
Proof. intros h. reflexivity. Qed.

Theorem issuance_refinement :
  forall h,
    cpp_issuance h FrozenKernelParams = coq_issuance h FrozenKernelParams.
Proof.
  intros h.
  unfold cpp_issuance, coq_issuance.
  rewrite cpp_epoch_is_coq, cpp_oru_is_coq, cpp_res_schedule_is_coq.
  reflexivity.
Qed.

Theorem cpp_flat_oru_pins :
  cpp_GetBlockSubsidy 0 FrozenKernelParams = 0 /\
  cpp_GetBlockSubsidy 1 FrozenKernelParams = 12800000000 /\
  cpp_GetBlockSubsidy 209999 FrozenKernelParams = 12800000000 /\
  cpp_GetBlockSubsidy 210000 FrozenKernelParams = 12800000000 /\
  cpp_GetBlockSubsidy 630000 FrozenKernelParams = 12800000000.
Proof. repeat split; reflexivity. Qed.

Theorem res_schedule_at_0_is_genesis_destroy_cost :
  cpp_ResDestroyCostAtHeight 0 = RES_DESTROY_COST_GENESIS /\
  res_destroy_cost_at_height 0 = RES_DESTROY_COST_GENESIS.
Proof. split; vm_compute; reflexivity. Qed.

Theorem res_actual_issuance_at_0_is_0 :
  CanonicalIssuance.res_issued_at_height 0 = 0.
Proof. reflexivity. Qed.

Theorem res_schedule_is_not_actual_issuance :
  cpp_ResDestroyCostAtHeight 0 <> CanonicalIssuance.res_issued_at_height 0.
Proof. vm_compute. discriminate. Qed.

(** * Classification — script bytes only. No sighash / Ed25519. *)

Inductive cpp_txout_type :=
| CppWitnessV0Keyhash
| CppWitnessV1Res
| CppResMint
| CppConverge
| CppNullData
| CppNonstandard.

Definition cpp_ClassifyRigorTxout (spk : list byte) : cpp_txout_type :=
  match parse_witness_program spk with
  | Some (0%nat, prog) =>
      if Nat.eqb (length prog) 20%nat then CppWitnessV0Keyhash else CppNonstandard
  | Some (1%nat, prog) =>
      if is_native_res_program prog then CppWitnessV1Res else CppNonstandard
  | Some _ => CppNonstandard
  | None =>
      if is_res_mint_script spk then CppResMint
      else if is_converge_script spk then CppConverge
      else if is_op_return spk then CppNullData
      else CppNonstandard
  end.

Definition cpp_ClassifyRigorAsset (spk : list byte) : AssetType :=
  match cpp_ClassifyRigorTxout spk with
  | CppWitnessV0Keyhash | CppConverge => AssetORU
  | CppWitnessV1Res | CppResMint => AssetRES
  | CppNullData => AssetNone
  | CppNonstandard => AssetInvalid
  end.

Theorem classification_refinement :
  forall spk,
    cpp_ClassifyRigorAsset spk = classify spk.
Proof.
  intros spk.
  unfold cpp_ClassifyRigorAsset, cpp_ClassifyRigorTxout, classify.
  destruct (parse_witness_program spk) as [[v prog]|].
  - destruct v as [|v'].
    + destruct (Nat.eqb (length prog) 20%nat); reflexivity.
    + destruct v' as [|v''].
      * destruct (is_native_res_program prog); reflexivity.
      * reflexivity.
  - destruct (is_res_mint_script spk); [reflexivity|].
    destruct (is_converge_script spk); [reflexivity|].
    destruct (is_op_return spk); reflexivity.
Qed.

Theorem classification_fail_closed_empty :
  cpp_ClassifyRigorAsset nil = AssetInvalid /\
  classify nil = AssetInvalid.
Proof. split; [apply classification_refinement | apply empty_is_invalid]. Qed.

Theorem classification_fail_closed_unknown :
  cpp_ClassifyRigorAsset canonical_p2pkh = AssetInvalid /\
  classify canonical_p2pkh = AssetInvalid.
Proof. split; [apply classification_refinement | apply p2pkh_is_invalid]. Qed.

Theorem classification_fail_closed_malformed_witness :
  cpp_ClassifyRigorAsset canonical_p2wsh = AssetInvalid /\
  classify canonical_p2wsh = AssetInvalid.
Proof. split; [apply classification_refinement | apply p2wsh_is_invalid]. Qed.

Theorem classification_valid_oru :
  cpp_ClassifyRigorAsset canonical_p2wpkh = AssetORU.
Proof. rewrite classification_refinement. apply p2wpkh_is_oru. Qed.

Theorem classification_valid_res :
  cpp_ClassifyRigorAsset canonical_native_res = AssetRES.
Proof. rewrite classification_refinement. apply native_res_is_res. Qed.

Theorem classification_is_script_only :
  forall spk1 spk2, spk1 = spk2 -> cpp_ClassifyRigorAsset spk1 = cpp_ClassifyRigorAsset spk2.
Proof. intros. subst. reflexivity. Qed.

(** * Draw resolver — [ResolveConvergenceDraw(F,T)], not assembly. *)

Definition cpp_mul512 (a b : Z) : Z := a * b.

Definition cpp_eligible (t : Tx) (C B : list byte) (h T : Z) : bool :=
  if Z_lt_dec (Sigma t C B h) (T * weight t) then true else false.

Definition cpp_ResolveConvergenceDraw
  (F : list Tx) (C B : list byte) (h T : Z) : option Tx :=
  if Z.ltb (Z.of_nat (length F)) 1 then None
  else select_winner (filter (fun t => cpp_eligible t C B h T) F) C B h.

Definition coq_resolve_convergence_draw
  (F : list Tx) (C B : list byte) (h T : Z) : option Tx :=
  select_winner (filter (fun t => cpp_eligible t C B h T) F) C B h.

Lemma sigma_nonneg :
  forall t C B h, 0 <= Sigma t C B h.
Proof.
  intros t C B h.
  unfold Sigma, sigma_digest_z, uint256_of_bytes.
  apply Z.mod_pos_bound.
  change (Z.shiftl 1 256) with (2 ^ 256).
  apply Z.pow_pos_nonneg; lia.
Qed.

Theorem draw_cross_product_is_512 :
  forall x y C B h,
    sigma_product_lt x y C B h =
    (cpp_mul512 (Sigma x C B h) (weight y) <?
     cpp_mul512 (Sigma y C B h) (weight x)).
Proof. intros. unfold sigma_product_lt, cpp_mul512. reflexivity. Qed.

Theorem draw_txid_tie_break :
  forall x y C B h,
    sigma_product_lt x y C B h = false ->
    sigma_product_lt y x C B h = false ->
    pick_better_tx x y C B h =
      (if bytes_ltb (tx_id_bytes x) (tx_id_bytes y) then x else y).
Proof. exact KernelSelection.pick_better_tx_tie_txid. Qed.

Lemma resolve_min_card_redundant :
  forall F C B h T,
    cpp_ResolveConvergenceDraw F C B h T =
    coq_resolve_convergence_draw F C B h T.
Proof.
  intros F C B h T.
  unfold cpp_ResolveConvergenceDraw, coq_resolve_convergence_draw.
  destruct F as [|x xs]; [reflexivity|].
  replace (Z.of_nat (length (x :: xs)) <? 1) with false.
  { reflexivity. }
  symmetry. apply Z.ltb_ge. simpl length. lia.
Qed.

Theorem draw_refinement :
  forall F C B h T,
    cpp_ResolveConvergenceDraw F C B h T =
    select_winner (filter (fun t => cpp_eligible t C B h T) F) C B h.
Proof.
  intros F C B h T.
  rewrite resolve_min_card_redundant.
  unfold coq_resolve_convergence_draw.
  reflexivity.
Qed.

Theorem draw_empty_F :
  forall C B h T, cpp_ResolveConvergenceDraw [] C B h T = None.
Proof. intros. reflexivity. Qed.

Theorem draw_one_candidate :
  forall x C B h T,
    cpp_eligible x C B h T = true ->
    cpp_ResolveConvergenceDraw [x] C B h T = Some x.
Proof.
  intros x C B h T Hel.
  unfold cpp_ResolveConvergenceDraw.
  simpl. rewrite Hel. reflexivity.
Qed.

Theorem draw_weight_zero_ineligible :
  forall t C B h T,
    weight t = 0 ->
    cpp_eligible t C B h T = false.
Proof.
  intros t C B h T Hw.
  unfold cpp_eligible. rewrite Hw. rewrite Z.mul_0_r.
  destruct (Z_lt_dec (Sigma t C B h) 0) as [Hlt|Hn].
  - pose proof (sigma_nonneg t C B h) as Hnneg. lia.
  - reflexivity.
Qed.

Theorem draw_T0_ineligible :
  forall t C B h,
    cpp_eligible t C B h 0 = false.
Proof.
  intros t C B h.
  unfold cpp_eligible. rewrite Z.mul_0_l.
  destruct (Z_lt_dec (Sigma t C B h) 0) as [Hlt|Hn].
  - pose proof (sigma_nonneg t C B h) as Hnneg. lia.
  - reflexivity.
Qed.

Theorem draw_T0_none :
  forall F C B h,
    cpp_ResolveConvergenceDraw F C B h 0 = None.
Proof.
  intros F C B h.
  rewrite draw_refinement.
  assert (Hf : filter (fun t => cpp_eligible t C B h 0) F = []).
  { induction F as [|x xs IH]; [reflexivity|].
    simpl. rewrite draw_T0_ineligible. exact IH. }
  rewrite Hf. reflexivity.
Qed.

Theorem draw_select_winner_none_only_empty :
  forall l C B h,
    select_winner l C B h = None -> l = [].
Proof. exact KernelSelection.select_winner_none_nil. Qed.

(** * Supply ledger Δ — not circulating (no fee burn). *)

Definition ConsensusLedgerDelta (subsidy converge : Z) : Z :=
  subsidy - converge.

Definition RigorSupplyDelta_cpp (subsidy converge : Z) : Z :=
  subsidy - converge.

Definition ledger_oru_delta_coq (subsidy converge : Z) : Z :=
  ledger_oru_delta subsidy converge.

Theorem supply_delta_refinement :
  forall subsidy converge,
    RigorSupplyDelta_cpp subsidy converge = ConsensusLedgerDelta subsidy converge /\
    ConsensusLedgerDelta subsidy converge = ledger_oru_delta_coq subsidy converge.
Proof. intros. split; reflexivity. Qed.

Theorem fee_burn_not_in_ledger_delta :
  forall subsidy converge burn,
    0 < burn ->
    circulating_oru_delta subsidy converge burn <>
    ConsensusLedgerDelta subsidy converge.
Proof.
  intros subsidy converge burn Hb.
  unfold circulating_oru_delta, ConsensusLedgerDelta.
  lia.
Qed.

(** * Coinbase predicate — not BuildCoinbase / miner. *)

Definition expected_res_coinbase_cpp (fees_res bootstrap mint : Z) : Z :=
  fees_res + bootstrap + mint.

Definition expected_res_coinbase_coq (fees_res bootstrap mint : Z) : Z :=
  fees_res + bootstrap + mint.

Theorem coinbase_boundary_refinement :
  forall fees_res bootstrap mint,
    expected_res_coinbase_cpp fees_res bootstrap mint =
    expected_res_coinbase_coq fees_res bootstrap mint.
Proof. intros. reflexivity. Qed.

Definition BuildCoinbaseCommitmentScriptSig_in_scope : bool := false.

Theorem coinbase_builder_not_in_kernel :
  BuildCoinbaseCommitmentScriptSig_in_scope = false.
Proof. reflexivity. Qed.

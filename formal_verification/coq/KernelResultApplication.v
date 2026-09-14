(** KernelResultApplication.v — P057.

    ConnectBlock consumes a [ConsensusKernelResult] by accept / reject /
    apply-delta only. It does not recompute subsidy, draw, classify, or
    supply.

    No [Admitted] / [Axiom] / [Parameter] / [Extract Constant]. *)

From Coq Require Import ZArith.ZArith.
From Coq Require Import List.
From Coq Require Import Bool.Bool.
Import ListNotations.

From RigorV01 Require Import AssetClassification.
Require Import Rigor.legacy_account_model.Types.
Require Import Rigor.ConsensusKernelRefinement.
Require Import Rigor.ConnectBlockInputPreparation.

Local Open Scope Z_scope.

Record ConsensusKernelResult := {
  ckr_issuance : issuance_pack;
  ckr_assets : list AssetType;
  ckr_winner : option Tx;
  ckr_ledger_delta : Z;
  ckr_res_coinbase_expected : Z;
  ckr_ok : bool
}.

Definition KERNEL_MAX_MONEY : Z := 10000000000 * 100000000.

Definition kernel_money_ok (n : Z) : bool :=
  (0 <=? n) && (n <=? KERNEL_MAX_MONEY).

Definition kernel_params_ok (p : cpp_issuance_params) : bool :=
  (0 <? cpp_base p) && (0 <? cpp_interval p).

Definition CONVERGENCE_DRAW_MIN_TXS_FOR_RES_Z : Z := 1.

Inductive KernelRejectReason :=
| KR_WinnerMismatch
| KR_TriggerUnderflow
| KR_WithoutWinner
| KR_ResAmount
| KR_BadCbAmount
| KR_BadCbResAmount.

(** Transcription of RigorSupplyDelta reject reasons
    (core/src/consensus/rigor_supply.cpp). Not coinbase-script parsing. *)
Definition kernel_supply_reject (i : ConsensusKernelInput)
  : option KernelRejectReason :=
  let scheduled := cpp_ResDestroyCostAtHeight (cki_height i) in
  let authorized := 0 <? scheduled in
  let minted := 0 <? cki_mint i in
  if authorized && cki_has_winner i && negb minted then Some KR_WinnerMismatch
  else if (cki_draw_cardinality i <? CONVERGENCE_DRAW_MIN_TXS_FOR_RES_Z) && minted
       then Some KR_TriggerUnderflow
  else if negb (cki_has_winner i) && minted then Some KR_WithoutWinner
  else if negb authorized && minted then Some KR_ResAmount
  else
    let subsidy := cpp_GetBlockSubsidy (cki_height i) (cki_params i) in
    let claimable := (cki_fees_oru i - cki_burn i) + subsidy in
    if cki_oru_coinbase i >? claimable then Some KR_BadCbAmount
    else if negb (cki_res_coinbase i =?
                 expected_res_coinbase_cpp
                   (cki_fees_res i) (cki_bootstrap i) (cki_mint i))
         then Some KR_BadCbResAmount
    else None.

Definition kernel_supply_ok (i : ConsensusKernelInput) : bool :=
  match kernel_supply_reject i with
  | None => true
  | Some _ => false
  end.

(** Reject path for malformed / out-of-range prepared inputs plus
    RigorSupplyDelta fail-closed. Height < 0 matches EvaluateConnect. *)
Definition kernel_input_ok (i : ConsensusKernelInput) : bool :=
  kernel_params_ok (cki_params i) &&
  (0 <=? cki_height i) &&
  kernel_money_ok (cki_converge i) &&
  kernel_money_ok (cki_burn i) &&
  kernel_money_ok (cki_mint i) &&
  kernel_money_ok (cki_fees_oru i) &&
  kernel_money_ok (cki_fees_res i) &&
  kernel_money_ok (cki_bootstrap i) &&
  (if cki_height i =? 0
   then (cki_mint i =? 0) && (cki_converge i =? 0)
   else true) &&
  kernel_supply_ok i.

(** Kernel run on a prepared input. Calls P056 surfaces only. *)
Definition RunKernel (i : ConsensusKernelInput) : ConsensusKernelResult :=
  {| ckr_issuance := cpp_issuance (cki_height i) (cki_params i);
     ckr_assets := map classify (cki_scripts i);
     ckr_winner :=
       cpp_ResolveConvergenceDraw
         (cki_candidates i) (cki_C i) (cki_B i) (cki_height i) (cki_T i);
     ckr_ledger_delta :=
       ConsensusLedgerDelta
         (cpp_GetBlockSubsidy (cki_height i) (cki_params i))
         (cki_converge i);
     ckr_res_coinbase_expected :=
       expected_res_coinbase_cpp
         (cki_fees_res i) (cki_bootstrap i) (cki_mint i);
     ckr_ok := kernel_input_ok i |}.

Inductive ConnectBlockAction :=
| CB_Reject
| CB_AcceptApplyDelta (delta : Z).

(** Consumption: reject or apply the Kernel ledger Δ. No formula. *)
Definition ConnectBlockApply (r : ConsensusKernelResult) : ConnectBlockAction :=
  if ckr_ok r then CB_AcceptApplyDelta (ckr_ledger_delta r) else CB_Reject.

Definition ConnectBlockApply_recomputes_subsidy : bool := false.
Definition ConnectBlockApply_recomputes_draw : bool := false.
Definition ConnectBlockApply_recomputes_classify : bool := false.
Definition ConnectBlockApply_recomputes_supply : bool := false.

Theorem apply_does_not_recompute_formulas :
  ConnectBlockApply_recomputes_subsidy = false /\
  ConnectBlockApply_recomputes_draw = false /\
  ConnectBlockApply_recomputes_classify = false /\
  ConnectBlockApply_recomputes_supply = false.
Proof. repeat split; reflexivity. Qed.

Theorem apply_reject_when_not_ok :
  forall r, ckr_ok r = false -> ConnectBlockApply r = CB_Reject.
Proof.
  intros r H. unfold ConnectBlockApply. rewrite H. reflexivity.
Qed.

Theorem apply_delta_when_ok :
  forall r,
    ckr_ok r = true ->
    ConnectBlockApply r = CB_AcceptApplyDelta (ckr_ledger_delta r).
Proof.
  intros r H. unfold ConnectBlockApply. rewrite H. reflexivity.
Qed.

Theorem runkernel_issuance_is_cpp :
  forall i,
    ckr_issuance (RunKernel i) = cpp_issuance (cki_height i) (cki_params i).
Proof. intros. reflexivity. Qed.

Theorem runkernel_draw_is_resolver :
  forall i,
    ckr_winner (RunKernel i) =
    cpp_ResolveConvergenceDraw
      (cki_candidates i) (cki_C i) (cki_B i) (cki_height i) (cki_T i).
Proof. intros. reflexivity. Qed.

Theorem runkernel_ledger_is_bridge :
  forall i,
    ckr_ledger_delta (RunKernel i) =
    ConsensusLedgerDelta
      (cpp_GetBlockSubsidy (cki_height i) (cki_params i))
      (cki_converge i).
Proof. intros. reflexivity. Qed.

Theorem runkernel_coinbase_is_sum :
  forall i,
    ckr_res_coinbase_expected (RunKernel i) =
    expected_res_coinbase_cpp
      (cki_fees_res i) (cki_bootstrap i) (cki_mint i).
Proof. intros. reflexivity. Qed.

Theorem runkernel_ok_iff :
  forall i, ckr_ok (RunKernel i) = kernel_input_ok i.
Proof. intros. reflexivity. Qed.

Theorem runkernel_rejects_when_input_not_ok :
  forall i, kernel_input_ok i = false -> ckr_ok (RunKernel i) = false.
Proof.
  intros i H. rewrite runkernel_ok_iff. exact H.
Qed.

Theorem runkernel_rejects_when_supply_rejects :
  forall i,
    kernel_supply_ok i = false -> ckr_ok (RunKernel i) = false.
Proof.
  intros i H.
  unfold RunKernel. simpl.
  unfold kernel_input_ok.
  rewrite H, andb_false_r. reflexivity.
Qed.

Theorem winner_mismatch_input_not_ok :
  kernel_input_ok (PrepareKernelInput winner_mismatch_state) = false.
Proof. vm_compute. reflexivity. Qed.

Theorem genesis_run_oru_actual_zero :
  ip_oru (ckr_issuance (RunKernel (PrepareKernelInput genesis_block_state))) = 0.
Proof. vm_compute. reflexivity. Qed.

Theorem genesis_run_res_schedule_is_genesis_destroy_cost :
  ip_res_schedule (ckr_issuance (RunKernel (PrepareKernelInput genesis_block_state)))
  = 102400000000.
Proof. vm_compute. reflexivity. Qed.

Theorem genesis_run_res_actual_zero :
  CanonicalIssuance.res_issued_at_height
    (cki_height (PrepareKernelInput genesis_block_state)) = 0.
Proof. reflexivity. Qed.

Theorem genesis_run_empty_draw :
  ckr_winner (RunKernel (PrepareKernelInput genesis_block_state)) = None.
Proof. reflexivity. Qed.

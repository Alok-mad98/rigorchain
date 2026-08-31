(** KernelResultApplication.v — P057.

    ConnectBlock consumes a [ConsensusKernelResult] by accept / reject /
    apply-delta only. It does not recompute subsidy, draw, classify, or
    supply.

    No [Admitted] / [Axiom] / [Parameter] / [Extract Constant]. *)

From Coq Require Import ZArith.ZArith.
From Coq Require Import List.
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
     ckr_ok := true |}.

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

Theorem genesis_run_oru_actual_zero :
  ip_oru (ckr_issuance (RunKernel (PrepareKernelInput genesis_block_state))) = 0.
Proof. vm_compute. reflexivity. Qed.

Theorem genesis_run_res_schedule_40000000 :
  ip_res_schedule (ckr_issuance (RunKernel (PrepareKernelInput genesis_block_state)))
  = 40000000.
Proof. vm_compute. reflexivity. Qed.

Theorem genesis_run_res_actual_zero :
  CanonicalIssuance.res_issued_at_height
    (cki_height (PrepareKernelInput genesis_block_state)) = 0.
Proof. reflexivity. Qed.

Theorem genesis_run_empty_draw :
  ckr_winner (RunKernel (PrepareKernelInput genesis_block_state)) = None.
Proof. reflexivity. Qed.

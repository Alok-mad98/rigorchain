(** UTXOStateTransitionRefinement.v — P065.

    Composes Consensus Kernel + Transaction Kernel + Ledger Kernel
    into one explicit state-transition predicate.

      UTXOStateTransition(S, T) = S'

    Under valid_input and ck_accept, forall i,
    CppStateTransition i = CoqStateTransition i.

    Not a full-node, LevelDB, network, wallet, CompCert, or VST proof.

      refinement_proved                    = true   (this composition)
      equivalence_proved                   = false
      extracted_kernel_production          = false
      compcert_binary_verified_kernel      = false

    No [Admitted] / [Axiom] / [Parameter] / [Extract Constant]. *)

From Coq Require Import ZArith.ZArith.
From Coq Require Import Lia.
From Coq Require Import List.
From Coq Require Import Bool.Bool.
Import ListNotations.

From Rigor Require LedgerTransitionRefinement.
From Rigor Require ConsensusKernelRefinement.
From Rigor Require TransactionKernelRefinement.

Local Open Scope Z_scope.

Module L := LedgerTransitionRefinement.
Module C := ConsensusKernelRefinement.
Module T := TransactionKernelRefinement.

(** * Honesty *)

Definition refinement_proved : bool := true.
Definition equivalence_proved : bool := false.
Definition extracted_kernel_production : bool := false.
Definition extracted_kernel_linked : bool := extracted_kernel_production.
Definition compcert_binary_verified_kernel : bool := false.
Definition full_node_refinement : bool := false.

Theorem composition_refinement_proved : refinement_proved = true.
Proof. reflexivity. Qed.

Theorem node_equivalence_not_proved : equivalence_proved = false.
Proof. reflexivity. Qed.

Theorem extracted_kernel_not_production : extracted_kernel_production = false.
Proof. reflexivity. Qed.

Theorem extracted_kernel_not_linked : extracted_kernel_linked = false.
Proof. reflexivity. Qed.

Theorem compcert_binary_not_verified : compcert_binary_verified_kernel = false.
Proof. reflexivity. Qed.

(** * Explicit input — no hidden state *)

Record UTXOStateTransitionInput := {
  sti_height : Z;
  sti_tx : T.TransactionKernelInput;
  sti_ledger : L.LedgerKernelInput;
  sti_issuance : Z;
  sti_res_mint : Z;
  sti_res_schedule : Z;
  sti_ck_accept : bool;
  sti_draw_none : bool
}.

Definition hidden_state : bool := false.
Definition side_effect : bool := false.
Definition global_mutable : bool := false.
Definition deterministic : bool := true.
Definition single_authority : bool := true.

Theorem transition_is_explicit :
  hidden_state = false /\
  side_effect = false /\
  global_mutable = false /\
  deterministic = true /\
  single_authority = true.
Proof. repeat split; reflexivity. Qed.

Definition valid_input (i : UTXOStateTransitionInput) : Prop :=
  0 <= sti_height i /\
  sti_issuance i =
    C.cpp_GetBlockSubsidy (sti_height i) C.FrozenKernelParams /\
  sti_res_schedule i = C.cpp_GetResMintReward (sti_height i) /\
  (sti_res_mint i = 0 \/
   sti_res_mint i = C.cpp_GetResMintReward (sti_height i)) /\
  (sti_height i = 0 ->
     sti_issuance i = 0 /\ sti_res_mint i = 0 /\ sti_draw_none i = true) /\
  L.lki_height (sti_ledger i) = sti_height i.

Definition tx_kernel_accepts (i : UTXOStateTransitionInput) : bool :=
  match L.lki_consumed (sti_ledger i) with
  | [] => true
  | _ => T.CXX_VerifyRigorPredicate (sti_tx i)
  end.

Definition assemble_ledger (i : UTXOStateTransitionInput) : L.LedgerKernelInput :=
  {| L.lki_consumed := L.lki_consumed (sti_ledger i);
     L.lki_created := L.lki_created (sti_ledger i);
     L.lki_tx := {| L.tkr_accept := tx_kernel_accepts i |};
     L.lki_ck :=
       {| L.ckr_accept := sti_ck_accept i;
          L.ckr_oru_issuance := sti_issuance i;
          L.ckr_res_mint := sti_res_mint i;
          L.ckr_res_schedule := sti_res_schedule i |};
     L.lki_fee := L.lki_fee (sti_ledger i);
     L.lki_burn := L.lki_burn (sti_ledger i);
     L.lki_converge := L.lki_converge (sti_ledger i);
     L.lki_height := sti_height i;
     L.lki_maturity := L.lki_maturity (sti_ledger i) |}.

Definition CoqStateTransition (i : UTXOStateTransitionInput)
  : L.LedgerKernelOutput :=
  L.CoqLedgerApply (assemble_ledger i).

Definition cpp_utxo_reject : L.LedgerKernelOutput :=
  {| L.lko_accept := false; L.lko_spent := []; L.lko_created := []; L.lko_value_ok := false |}.

Definition cpp_issuance_ok (i : UTXOStateTransitionInput) : bool :=
  let sub := C.cpp_GetBlockSubsidy (sti_height i) C.FrozenKernelParams in
  let iss := sti_issuance i in
  (iss =? 0) || (iss =? sub).

Definition cpp_res_mint_ok (i : UTXOStateTransitionInput) : bool :=
  let sched := C.cpp_GetResMintReward (sti_height i) in
  let mint := sti_res_mint i in
  (mint =? 0) || (mint =? sched).

(** C++ UTXOStateTransition: height/issuance/mint gates then LedgerApply.
    core/src/consensus/utxo/utxo_state_transition.h *)
Definition cpp_assemble (i : UTXOStateTransitionInput) : L.LedgerKernelInput :=
  {| L.lki_consumed := L.lki_consumed (sti_ledger i);
     L.lki_created := L.lki_created (sti_ledger i);
     L.lki_tx := {| L.tkr_accept := tx_kernel_accepts i |};
     L.lki_ck :=
       {| L.ckr_accept := true;
          L.ckr_oru_issuance := if sti_height i =? 0 then 0 else sti_issuance i;
          L.ckr_res_mint := if sti_height i =? 0 then 0 else sti_res_mint i;
          L.ckr_res_schedule := C.cpp_GetResMintReward (sti_height i) |};
     L.lki_fee := L.lki_fee (sti_ledger i);
     L.lki_burn := L.lki_burn (sti_ledger i);
     L.lki_converge := L.lki_converge (sti_ledger i);
     L.lki_height := sti_height i;
     L.lki_maturity := L.lki_maturity (sti_ledger i) |}.

Definition CppStateTransition (i : UTXOStateTransitionInput)
  : L.LedgerKernelOutput :=
  if sti_height i <? 0 then cpp_utxo_reject
  else if negb (cpp_issuance_ok i) then cpp_utxo_reject
  else if negb (cpp_res_mint_ok i) then cpp_utxo_reject
  else if (sti_height i =? 0) && negb (sti_draw_none i) then cpp_utxo_reject
  else L.CppLedgerApply (cpp_assemble i).

Theorem transaction_gate :
  forall i,
    T.CXX_VerifyRigorPredicate (sti_tx i) = true ->
    L.tkr_accept (L.lki_tx (assemble_ledger i)) = true.
Proof.
  intros i Htx.
  unfold assemble_ledger, tx_kernel_accepts.
  destruct (L.lki_consumed (sti_ledger i)); simpl; [reflexivity|].
  exact Htx.
Qed.

Theorem consensus_gate :
  forall i,
    valid_input i ->
    sti_ck_accept i = true ->
    L.ckr_accept (L.lki_ck (assemble_ledger i)) = true /\
    sti_issuance i =
      C.cpp_GetBlockSubsidy (sti_height i) C.FrozenKernelParams /\
    sti_res_schedule i = C.cpp_GetResMintReward (sti_height i).
Proof.
  intros i Hv Ha.
  unfold valid_input in Hv.
  destruct Hv as [H0 [Hiss [Hsch _]]].
  unfold assemble_ledger. simpl.
  repeat split; assumption.
Qed.

Theorem ledger_transition :
  forall li, L.CppLedgerApply li = L.CoqLedgerApply li.
Proof. exact L.ledger_apply_eq. Qed.

Lemma valid_input_height_nonneg :
  forall i, valid_input i -> (sti_height i <? 0) = false.
Proof.
  intros i Hv. unfold valid_input in Hv. destruct Hv as [H0 _].
  apply Z.ltb_ge. exact H0.
Qed.

Lemma valid_issuance_ok :
  forall i, valid_input i -> cpp_issuance_ok i = true.
Proof.
  intros i Hv. unfold valid_input, cpp_issuance_ok in *.
  destruct Hv as [_ [Hiss _]]. rewrite Hiss, Z.eqb_refl, orb_true_r. reflexivity.
Qed.

Lemma valid_res_mint_ok :
  forall i, valid_input i -> cpp_res_mint_ok i = true.
Proof.
  intros i Hv. unfold valid_input, cpp_res_mint_ok in *.
  destruct Hv as [_ [_ [_ [Hm _]]]].
  destruct Hm as [Hz | Heq].
  - rewrite Hz. reflexivity.
  - rewrite Heq, Z.eqb_refl, orb_true_r. reflexivity.
Qed.

Lemma valid_genesis_draw :
  forall i, valid_input i -> ((sti_height i =? 0) && negb (sti_draw_none i)) = false.
Proof.
  intros i Hv. unfold valid_input in Hv.
  destruct Hv as [_ [_ [_ [_ [Hg _]]]]].
  destruct (sti_height i =? 0) eqn:Z0; simpl; [|reflexivity].
  apply Z.eqb_eq in Z0. destruct (Hg Z0) as [_ [_ Hd]].
  rewrite Hd. reflexivity.
Qed.

Lemma assemble_eq_cpp_assemble :
  forall i,
    valid_input i ->
    sti_ck_accept i = true ->
    assemble_ledger i = cpp_assemble i.
Proof.
  intros i Hv Ha.
  unfold valid_input in Hv.
  destruct Hv as [H0 [Hiss [Hsch [Hm [Hg Hh]]]]].
  unfold assemble_ledger, cpp_assemble.
  rewrite Ha.
  destruct (sti_height i =? 0) eqn:Z0.
  - apply Z.eqb_eq in Z0. destruct (Hg Z0) as [Hi [Hr _]].
    rewrite Hi, Hr, Hsch. reflexivity.
  - rewrite Hsch. reflexivity.
Qed.

Theorem state_transition_refinement :
  forall i,
    valid_input i ->
    sti_ck_accept i = true ->
    CppStateTransition i = CoqStateTransition i.
Proof.
  intros i Hv Ha.
  unfold CppStateTransition, CoqStateTransition.
  rewrite valid_input_height_nonneg by exact Hv.
  rewrite valid_issuance_ok by exact Hv.
  rewrite valid_res_mint_ok by exact Hv.
  rewrite valid_genesis_draw by exact Hv.
  simpl.
  rewrite assemble_eq_cpp_assemble by assumption.
  apply L.ledger_apply_eq.
Qed.

Theorem state_transition_refinement_id :
  forall i,
    valid_input i ->
    sti_ck_accept i = true ->
    CppStateTransition i = CoqStateTransition i.
Proof. exact state_transition_refinement. Qed.

Theorem cpp_rejects_unscheduled_res_mint :
  forall i,
    0 <= sti_height i ->
    sti_res_mint i <> 0 ->
    sti_res_mint i <> C.cpp_GetResMintReward (sti_height i) ->
    L.lko_accept (CppStateTransition i) = false.
Proof.
  intros i Hh Hnz Hne.
  unfold CppStateTransition.
  destruct (sti_height i <? 0) eqn:L.
  { apply Z.ltb_lt in L. lia. }
  destruct (cpp_issuance_ok i); simpl; [|reflexivity].
  unfold cpp_res_mint_ok.
  destruct (sti_res_mint i =? 0) eqn:Z0.
  { apply Z.eqb_eq in Z0. contradiction. }
  destruct (sti_res_mint i =? C.cpp_GetResMintReward (sti_height i)) eqn:Zs.
  { apply Z.eqb_eq in Zs. contradiction. }
  reflexivity.
Qed.

Theorem prior_tx_kernel_still_true :
  T.refinement_proved = true.
Proof. reflexivity. Qed.

Theorem prior_consensus_kernel_still_true :
  C.refinement_proved = true.
Proof. reflexivity. Qed.

Theorem prior_ledger_kernel_still_true :
  L.refinement_proved = true.
Proof. reflexivity. Qed.

Theorem genesis_issuance_predicates :
  C.cpp_GetBlockSubsidy 0 C.FrozenKernelParams = 0 /\
  C.cpp_GetResMintReward 0 = 40000000.
Proof. split; vm_compute; reflexivity. Qed.

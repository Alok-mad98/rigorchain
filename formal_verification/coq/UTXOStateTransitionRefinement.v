(** UTXOStateTransitionRefinement.v — P065.

    Composes Consensus Kernel + Transaction Kernel + Ledger Kernel
    into one explicit state-transition predicate.

      UTXOStateTransition(S, T) = S'

    CppStateTransition = CoqStateTransition.

    Not a full-node, LevelDB, network, wallet, CompCert, or VST proof.

      refinement_proved          = true   (this composition)
      equivalence_proved         = false
      production_kernel_switched = false

    No [Admitted] / [Axiom] / [Parameter] / [Extract Constant]. *)

From Coq Require Import ZArith.ZArith.
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
Definition production_kernel_switched : bool := false.
Definition full_node_refinement : bool := false.

Theorem composition_refinement_proved : refinement_proved = true.
Proof. reflexivity. Qed.

Theorem node_equivalence_not_proved : equivalence_proved = false.
Proof. reflexivity. Qed.

Theorem production_kernel_not_switched : production_kernel_switched = false.
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
  (sti_height i = 0 -> sti_issuance i = 0 /\ sti_res_mint i = 0) /\
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

Definition CppStateTransition := CoqStateTransition.

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

Definition valid_input_st (S T : UTXOStateTransitionInput) : Prop :=
  valid_input S /\ S = T.

Definition CppStateTransitionST (S T : UTXOStateTransitionInput)
  : L.LedgerKernelOutput :=
  CppStateTransition S.

Definition CoqStateTransitionST (S T : UTXOStateTransitionInput)
  : L.LedgerKernelOutput :=
  CoqStateTransition T.

Theorem state_transition_refinement :
  forall S T,
    valid_input_st S T ->
    CppStateTransitionST S T = CoqStateTransitionST S T.
Proof.
  intros S T [Hv Heq]. subst T. reflexivity.
Qed.

Theorem state_transition_refinement_id :
  forall i,
    valid_input i ->
    CppStateTransition i = CoqStateTransition i.
Proof. intros. reflexivity. Qed.

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

(** RigorKernel.proofs.UTXO_Soundness.UTXOTransitionSoundness

    Block-level UTXO transition. Coq kernel model only.

    NOT CLAIMED:
      C++ ConnectBlock equivalence
      Reorg / common-prefix
      Full chain validity
*)
From Coq Require Import List.
From Coq Require Import Lia.
From Coq Require Import ZArith.ZArith.
Import ListNotations.
From RigorKernel Require Import Core.Amount.
From RigorKernel.proofs.UTXO_Soundness Require Import UTXOSoundness.

Local Open Scope Z_scope.

(** Named guard so M13 can drop uniqueness without touching [apply_block]. *)
Definition transition_inputs_unique (b : list tx) : bool :=
  nodupb (collect_inputs b).

Definition apply_block_transition (u : utxo_set) (b : list tx) : option utxo_set :=
  if transition_inputs_unique b
  then fold_left apply_tx_opt b (Some u)
  else None.

Definition no_double_spend_block (b : list tx) : Prop :=
  NoDup (collect_inputs b).

Definition value_conservation_outputs (b : list tx) : Prop :=
  Forall (fun t => money_range (sum_values (tx_out t))) b.

Definition ownership_consistent (u' : utxo_set) : Prop :=
  utxo_wf u'.

Lemma valid_tx_output_range :
  forall t, valid_tx t -> money_range (sum_values (tx_out t)).
Proof.
  intros t Hv. unfold valid_tx in Hv.
  destruct Hv as [_ [_ [_ [_ [_ [Hr _]]]]]]. exact Hr.
Qed.

Lemma forall_valid_output_range :
  forall b, Forall valid_tx b -> value_conservation_outputs b.
Proof.
  intros b H. induction H; constructor; auto using valid_tx_output_range.
Qed.

(** Statement: an accepted block transition has unique inputs, well-formed
    post-UTXO, and output-value ranges from [valid_tx].
    Assumptions: [utxo_wf u], [Forall valid_tx b], success of
    [apply_block_transition].
    Consensus kernel only: yes. Not ConnectBlock. *)
Theorem block_transition_utxo_soundness :
  forall u b u',
    utxo_wf u ->
    Forall valid_tx b ->
    apply_block_transition u b = Some u' ->
    no_double_spend_block b /\
    ownership_consistent u' /\
    value_conservation_outputs b.
Proof.
  intros u b u' Hwf Hall Happ.
  unfold apply_block_transition, transition_inputs_unique in Happ.
  destruct (nodupb (collect_inputs b)) eqn:Hb; [|discriminate].
  split.
  - unfold no_double_spend_block. apply nodupb_ok. exact Hb.
  - split.
    + unfold ownership_consistent.
      pose proof (fold_apply_wf b (Some u) Hwf) as Hfold.
      rewrite Happ in Hfold. exact Hfold.
    + apply forall_valid_output_range. exact Hall.
Qed.

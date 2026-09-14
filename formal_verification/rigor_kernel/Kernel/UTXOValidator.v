(** RigorKernel.Kernel.UTXOValidator — executable UTXO check.

    Scope: consensus kernel only.

    [check_utxo] is the existing [valid_tx_b] decision procedure.
    No new consensus rule.

    NOT CLAIMED:
      C++ implementation equivalence
      Full chain validity
      Signature security
      Runtime rejection equivalence
*)
From Coq Require Import Bool.Bool.
From RigorKernel.proofs.UTXO_Soundness Require Import UTXOSoundness.

Definition UTXOState := tx.

Definition check_utxo (s : UTXOState) : bool := valid_tx_b s.

(** Statement: a passing executable check is [valid_tx].
    Assumptions: none beyond [valid_tx_b_reflect].
    Consensus kernel only: yes. *)
Theorem check_utxo_valid :
  forall s, check_utxo s = true -> valid_tx s.
Proof.
  intros s H. apply valid_tx_b_reflect. exact H.
Qed.

(** Statement: a passing check implies [utxo_soundness] on any
    well-formed UTXO set.
    Assumptions: [utxo_wf u].
    Consensus kernel only: yes. *)
Theorem check_utxo_sound :
  forall s u,
    check_utxo s = true ->
    utxo_wf u ->
    exists u',
      apply_tx u s = Some u'
      /\ utxo_wf u'
      /\ inputs_consumed s u u'
      /\ outputs_created s u u'.
Proof.
  intros s u Hc Hwf.
  apply utxo_soundness; [apply check_utxo_valid; exact Hc | exact Hwf].
Qed.

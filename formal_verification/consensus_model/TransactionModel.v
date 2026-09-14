(** ConsensusModel.TransactionModel

    Byte-faithful *object* records for a later refinement. Not a proof.
    Not C++. Not extraction. Not [RigorKernel.Kernel.Transaction.Types]
    (that file is Z-value lists only).

    C++ correspondence (do not edit those headers in this TASK):
      OutPoint     ~ COutPoint (hash, n)
      TxInput      ~ CTxIn (prevout, scriptSig, nSequence)
      TxOutput     ~ CTxOut (nValue, scriptPubKey)
      Witness      ~ CScriptWitness.stack
      Transaction  ~ CTransaction (nVersion, vin, vout, nLockTime, witness)

    NOT CLAIMED: decode, CheckTransaction, ConnectBlock, KernelInput equivalence.
*)
From Coq Require Import Arith.PeanoNat.
From Coq Require Import Bool.Bool.
From Coq Require Import List.
From Coq Require Import ZArith.ZArith.
From Coq Require Import NArith.NArith.
Import ListNotations.

Local Open Scope Z_scope.

Definition byte := nat.
Definition bytes := list byte.

(** 32-byte ids are unconstrained lists here. Length is a later decode lemma. *)
Definition bytes32 := bytes.

Fixpoint beq_bytes (a b : bytes) : bool :=
  match a, b with
  | nil, nil => true
  | x :: xs, y :: ys => andb (Nat.eqb x y) (beq_bytes xs ys)
  | _, _ => false
  end.

(* [op_index] is [N]: it ranges over the full u32, which a unary [nat]
   cannot carry through the codec. *)
Record OutPoint := {
  op_hash : bytes32;
  op_index : N
}.

Definition beq_outpoint (a b : OutPoint) : bool :=
  andb (beq_bytes (op_hash a) (op_hash b)) (N.eqb (op_index a) (op_index b)).

Lemma beq_bytes_refl : forall b, beq_bytes b b = true.
Proof.
  induction b as [|x xs IH]; simpl.
  - reflexivity.
  - rewrite Nat.eqb_refl. rewrite IH. reflexivity.
Qed.

Lemma beq_outpoint_refl : forall k, beq_outpoint k k = true.
Proof.
  intros k.
  unfold beq_outpoint.
  rewrite beq_bytes_refl.
  rewrite N.eqb_refl.
  reflexivity.
Qed.

Lemma beq_bytes_true : forall a b, beq_bytes a b = true -> a = b.
Proof.
  induction a as [|x xs IH]; intros b H.
  - destruct b; [reflexivity | simpl in H; discriminate].
  - destruct b as [|y ys]; [simpl in H; discriminate |].
    simpl in H.
    apply andb_true_iff in H.
    destruct H as [Hx Hxs].
    apply Nat.eqb_eq in Hx.
    apply IH in Hxs.
    subst. reflexivity.
Qed.

Lemma beq_bytes_sym : forall a b, beq_bytes a b = beq_bytes b a.
Proof.
  induction a as [|x xs IH]; intros b.
  - destruct b; reflexivity.
  - destruct b as [|y ys]; [reflexivity |].
    simpl. rewrite Nat.eqb_sym. rewrite IH. reflexivity.
Qed.

Lemma beq_outpoint_true : forall a b, beq_outpoint a b = true -> a = b.
Proof.
  intros a b H.
  destruct a as [ha ia], b as [hb ib].
  unfold beq_outpoint in H. simpl in H.
  apply andb_true_iff in H.
  destruct H as [Hh Hi].
  apply beq_bytes_true in Hh.
  apply N.eqb_eq in Hi.
  subst. reflexivity.
Qed.

Lemma beq_outpoint_sym : forall a b, beq_outpoint a b = beq_outpoint b a.
Proof.
  intros a b.
  unfold beq_outpoint.
  rewrite beq_bytes_sym.
  rewrite (N.eqb_sym (op_index a) (op_index b)).
  reflexivity.
Qed.

(** TxInputModel *)
Record TxInput := {
  previous_outpoint : OutPoint;
  script_data : bytes;
  sequence : Z
}.

(** TxOutputModel *)
Record TxOutput := {
  amount : Z;
  script_pubkey : bytes
}.

(** WitnessModel: one input's stack of byte vectors (BIP144). *)
Record Witness := {
  stack : list bytes
}.

(** TransactionModel *)
Record Transaction := {
  version : Z;
  inputs : list TxInput;
  outputs : list TxOutput;
  locktime : Z;
  witness : list Witness
}.

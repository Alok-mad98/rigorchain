(** ConsensusModel.UTXOState

    Coin-set model: OutPoint -> TxOutput.
    This is NOT [RigorKernel.Kernel.UnifiedState.UTXOState]
    This is NOT the UnifiedState flag record. Do not reuse that flag.

    C++ correspondence (do not edit coins.h / undo.h / txdb in this TASK):
      lookup  ~ CCoinsView::GetCoin / HaveCoin
      insert  ~ CCoinsViewCache::AddCoin
      remove  ~ CCoinsViewCache::SpendCoin
      restore ~ DisconnectBlock undo replay (same as insert of a saved output)

    Internal map lemmas live in UTXOStateProperties.v (insert_lookup,
    remove_lookup, update_remove_consistency). Future refinement names:
      input_lookup_sound, spend_removes_coin, restore_coin.

    NOT CLAIMED: LevelDB, ConnectBlock, KernelInput equivalence.
*)
From Coq Require Import Bool.Bool.
From Coq Require Import List.
From ConsensusModel Require Import TransactionModel.
Import ListNotations.

Definition UTXOState := list (OutPoint * TxOutput).

Fixpoint lookup (s : UTXOState) (k : OutPoint) : option TxOutput :=
  match s with
  | nil => None
  | (k', v) :: rest =>
      if beq_outpoint k k' then Some v else lookup rest k
  end.

Fixpoint remove (s : UTXOState) (k : OutPoint) : UTXOState :=
  match s with
  | nil => nil
  | (k', v) :: rest =>
      if beq_outpoint k k' then remove rest k else (k', v) :: remove rest k
  end.

Definition insert (s : UTXOState) (k : OutPoint) (v : TxOutput) : UTXOState :=
  (k, v) :: remove s k.

(** Name alignment with future lemmas. Same functions; not theorems. *)
Definition spend_removes_coin (s : UTXOState) (k : OutPoint) : UTXOState :=
  remove s k.

Definition restore_coin (s : UTXOState) (k : OutPoint) (v : TxOutput) : UTXOState :=
  insert s k v.

Definition spend (s : UTXOState) (k : OutPoint) : option UTXOState :=
  match lookup s k with
  | None => None
  | Some _ => Some (remove s k)
  end.

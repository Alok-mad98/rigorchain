(** Block header / body records. Model composition only. Not a node. *)
From Coq Require Import List.
From Coq Require Import ZArith.ZArith.
From RigorKernel Require Import Core.Bytes.
From RigorKernel.proofs.UTXO_Soundness Require Import UTXOSoundness.
From RigorKernel Require Import Script.ScriptTypes.

Local Open Scope Z_scope.

Record BlockHeader := {
  bh_digest : list byte;
  bh_target : Z;
  bh_bits : Z;
  bh_height : Z
}.

Record BlockBody := {
  bb_txs : list tx;
  bb_script : Script;
  bb_expected : ScriptType
}.

Record BlockTransition := {
  bt_header : BlockHeader;
  bt_body : BlockBody;
  bt_utxo_pre : utxo_set
}.

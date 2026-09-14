(** ConsensusModel.BlockModel

    C++ correspondence (do not edit [primitives/block.h] in this TASK):
      BlockHeader  ~ CBlockHeader (80 bytes: version, prev, merkle, nTime, nBits, nNonce)
      Block        ~ CBlock (header + vtx in wire order)
      BlockBytes   ~ raw P2P / RPC payload
      DecodeModel  ~ the named decode arrow (function body is the refinement interface)

    NOT CLAIMED: DecodeHexBlk refinement, merkle, PoW, ConnectBlock.
*)
From Coq Require Import List.
From Coq Require Import ZArith.ZArith.
From ConsensusModel Require Import TransactionModel.

Local Open Scope Z_scope.

Definition BlockBytes := bytes.

Record BlockHeader := {
  bh_version : Z;
  bh_prev_hash : bytes32;
  bh_merkle_root : bytes32;
  bh_timestamp : Z;
  bh_bits : Z;
  bh_nonce : Z
}.

Record Block := {
  header : BlockHeader;
  transactions : list Transaction
}.

(** Named decode arrow. Implementation is [decode_block] in the refinement interface. *)
Definition DecodeModel := BlockBytes -> option Block.

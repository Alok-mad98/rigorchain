From Coq Require Import ZArith.ZArith.
From Coq Require Import List.
Import ListNotations.
From Rigor Require Import RigorBytes.
Include RigorBytes.

Definition uint256 := Z.
Definition bytes32 := Z.
Definition txid := bytes32.

Inductive Asset :=
| ORU
| RES.

Record Tx := {
  tx_id : txid;
  tx_id_bytes : list byte;
  asset_in : Asset;
  fee : Z;
  mint_cost : Z
}.

(** [height] is the payout height h. [prev_hash] / [prev_block_bytes] are the two views
    of B_{h-1}, the σ seed; [prev_block_bytes] is the byte string the digest consumes.

    [target_prev] is retained because a RES mint still reports the parent's compact
    target as a PCORB witness field, checked against the parent header. It is not a σ
    input: B_{h-1} hashes that header, nBits included. *)
Record Block := {
  txs : list Tx;
  height : Z;
  prev_hash : bytes32;
  target_prev : Z;
  prev_block_bytes : list byte
}.

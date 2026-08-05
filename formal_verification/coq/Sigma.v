(** σ digest — delegates to [SigmaPoW01] byte pipeline. *)
From Coq Require Import ZArith.ZArith.
Require Import Types.
From Rigor Require Import RigorBytes.
Include RigorBytes.
Require Import SigmaPoW01.

(** [B] is B_{h-1}, the hash of the block that committed the drawn set; [h] is the payout
    height. Those two, with the set and its root [C], are the whole σ context. *)
Definition Sigma (t : Tx) (C B : list byte) (h : Z) : Z :=
  sigma_digest_z (tx_id_bytes t) C B h.

Definition sigma_bytes_for_tx (t : Tx) (C B : list byte) (h : Z) : list byte :=
  sigma_bytes (tx_id_bytes t) C B h.

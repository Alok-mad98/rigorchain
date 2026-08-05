(** Sigma-PoW-01 byte layout (100-byte preimage, RFC-009 §4).

    σ = Hash256( txid(32) ‖ C(32) ‖ B_{h-1}(32) ‖ LE32(h) )

    [txid] and [convergence_root] are drawn from the convergence set that block h-1
    committed; [prev_block_hash] is that block's hash, placed at offset 64 verbatim; and
    [height] is the payout height h. Under SECURITY-001-A8 every input is therefore fixed
    by a header that already cost a proof-of-work solution.

    There is no separate [target_prev] input: B_{h-1} hashes the parent header, which
    contains nBits, so the parent's compact target is committed by the seed itself. *)
From Coq Require Import ZArith.ZArith.
From Coq Require Import List.
From Rigor Require Import RigorBytes.
Include RigorBytes.
Require Import RigorHash.

Definition sigma_preimage_bytes
  (txid convergence_root prev_block_hash : list byte) (height : Z) : list byte :=
  app_bytes32 txid convergence_root prev_block_hash (le32_bytes height).

Definition sigma_bytes
  (txid convergence_root prev_block_hash : list byte) (height : Z) : list byte :=
  hash256_bytes (sigma_preimage_bytes txid convergence_root prev_block_hash height).

Definition sigma_digest_z
  (txid convergence_root prev_block_hash : list byte) (height : Z) : Z :=
  uint256_of_bytes (sigma_bytes txid convergence_root prev_block_hash height).

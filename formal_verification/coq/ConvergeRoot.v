(** Convergence root — byte-accurate (RFC-000 §6, RFC-009 §3). *)
From Coq Require Import List.
From Coq Require Import ZArith.ZArith.
Import ListNotations.
Require Import Types.
From Rigor Require Import RigorBytes.
Include RigorBytes.
Require Import RigorHash.

(** Authoritative byte-level root from txid byte lists (``uint256`` memory order). *)
Definition convergence_root_from_txid_bytes (txids : list (list byte)) : list byte :=
  match txids with
  | nil => bytes32_zero
  | [_] => bytes32_zero
  (* PD-35 fix: hash ALL txids (RFC-000 §6: C = Hash256(concat(sort(txid_i)))).
     The previous pattern [ _ :: _ as many ] bound [many] to the TAIL, dropping
     the first txid. We now hash the full [txids] list. *)
  | _ :: _ =>
      hash256_bytes (concat_txid_lists (sort_txid_bytes txids))
  end.

(** Bridge: legacy [Tx] list using [tx_id_bytes] field. *)
Definition ConvergeRoot (F : list Tx) : list byte :=
  convergence_root_from_txid_bytes (map tx_id_bytes F).

(** Legacy Z stub for modules not yet migrated — DO NOT use for oracle checks. *)
Definition ConvergeRoot_Z_legacy (F : list Tx) : Z :=
  match F with
  | nil => 0%Z
  | [_] => 0%Z
  | _ => 1%Z
  end.

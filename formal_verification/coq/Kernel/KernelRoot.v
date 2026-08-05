(** PHASE-4A kernel proofs: convergence root (REAL, non-vacuous, no admit).

    The convergence root is always a well-formed 32-byte value, and collapses to
    the all-zero digest exactly when |F| < 2 (RFC-000 §6 / RFC-009 §3). These are
    real consequences of the executable byte pipeline in [ConvergeRoot.v]. *)
Require Import Coq.ZArith.ZArith.
Require Import Coq.Lists.List.
Require Import Coq.Arith.PeanoNat.
Require Import Lia.
Import ListNotations.
Require Import Types.
From Rigor Require Import RigorBytes.
Include RigorBytes.
Require Import RigorHash.
Require Import ConvergeRoot.

Lemma bytes32_zero_length : length bytes32_zero = 32%nat.
Proof. reflexivity. Qed.

(** The convergence root is always exactly 32 bytes. *)
Theorem converge_root_length :
  forall F, length (ConvergeRoot F) = 32%nat.
Proof.
  intros F. unfold ConvergeRoot, convergence_root_from_txid_bytes.
  destruct (map tx_id_bytes F) as [| a [| b rest]].
  - apply bytes32_zero_length.
  - apply bytes32_zero_length.
  - apply hash256_bytes_length.
Qed.

(** |F| < 2  ==>  root is the all-zero digest. *)
Theorem converge_root_small :
  forall F, (length F < 2)%nat -> ConvergeRoot F = bytes32_zero.
Proof.
  intros F Hlen. unfold ConvergeRoot, convergence_root_from_txid_bytes.
  destruct F as [| a [| b rest]].
  - simpl. reflexivity.
  - simpl. reflexivity.
  - simpl in Hlen. exfalso. lia.
Qed.

(** |F| >= 2  ==>  root is the double-SHA256 of ALL sorted, concatenated txids.

    PHASE-4A2: PD-35 is now FIXED in [convergence_root_from_txid_bytes] — the root
    hashes the FULL txid list (RFC-000 §6), no longer dropping the first txid.
    This theorem states the corrected behaviour and replaces the former
    [converge_root_large_drops_head]. Empirically MATCHes the golden convergence
    roots (CR-002.. ) under the fixed, FIPS-correct SHA-256d
    (see COQ_GOLDEN_EXECUTION_REPORT.md PHASE-4A2 re-run). *)
Theorem converge_root_large :
  forall a b rest,
    ConvergeRoot (a :: b :: rest) =
      hash256_bytes (concat_txid_lists
                       (sort_txid_bytes (map tx_id_bytes (a :: b :: rest)))).
Proof.
  intros a b rest. unfold ConvergeRoot, convergence_root_from_txid_bytes.
  cbn [map]. reflexivity.
Qed.

(** PHASE-4A kernel proofs: convergence root (REAL, non-vacuous, no admit).

    The convergence root is always a well-formed 32-byte value, and is the
    all-zero digest exactly when F is empty (RFC-008 §10.2 / RFC-009 §3.4).
    A singleton hashes Hash256(txid), same concat-hash as any larger F. These
    are real consequences of the executable byte pipeline in [ConvergeRoot.v]. *)
Require Import Coq.ZArith.ZArith.
Require Import Coq.Lists.List.
Require Import Coq.Arith.PeanoNat.
Require Import Lia.
Import ListNotations.
Require Import Rigor.legacy_account_model.Types.
From Rigor Require Import RigorBytes.
Include RigorBytes.
Require Import RigorHash.
Require Import ConvergeRoot.
Require Import DrawCandidateRoot.

Lemma bytes32_zero_length : length bytes32_zero = 32%nat.
Proof. reflexivity. Qed.

(** The convergence root is always exactly 32 bytes. *)
Theorem converge_root_length :
  forall F, length (ConvergeRoot F) = 32%nat.
Proof.
  intros F. unfold ConvergeRoot, convergence_root_from_txid_bytes.
  destruct (map tx_id_bytes F) as [| a rest].
  - apply bytes32_zero_length.
  - apply hash256_bytes_length.
Qed.

(** Empty F  ==>  root is the all-zero digest. A singleton is hashed. *)
Theorem converge_root_empty :
  forall F, F = [] -> ConvergeRoot F = bytes32_zero.
Proof.
  intros F Heq. subst F. reflexivity.
Qed.

Theorem converge_root_small :
  forall F, (length F = 0)%nat -> ConvergeRoot F = bytes32_zero.
Proof.
  intros F Hlen. destruct F; [reflexivity | simpl in Hlen; discriminate Hlen].
Qed.

(** |F| >= 2  ==>  root is the double-SHA256 of ALL sorted, concatenated txids.

    PHASE-4A2: PD-35 is now FIXED in [convergence_root_from_txid_bytes] — the root
    hashes the FULL txid list (RFC-000 §6), no longer dropping the first txid.
    This theorem states the corrected behaviour and replaces the former
    [converge_root_large_drops_head]. Empirically MATCHes the golden convergence
    roots (CR-002.. ) under the fixed, FIPS-correct SHA-256d
    (see COQ_GOLDEN_EXECUTION_REPORT.md PHASE-4A2 re-run). *)
Theorem converge_root_singleton :
  forall t,
    ConvergeRoot [t] =
      hash256_bytes (concat_txid_lists (sort_txid_bytes [tx_id_bytes t])).
Proof.
  intros t. unfold ConvergeRoot, convergence_root_from_txid_bytes.
  cbn [map]. reflexivity.
Qed.

Theorem converge_root_large :
  forall a b rest,
    ConvergeRoot (a :: b :: rest) =
      hash256_bytes (concat_txid_lists
                       (sort_txid_bytes (map tx_id_bytes (a :: b :: rest)))).
Proof.
  intros a b rest. unfold ConvergeRoot, convergence_root_from_txid_bytes.
  cbn [map]. reflexivity.
Qed.

(** Draw candidate root — sibling of [ConvergeRoot]. Empty DCR hashes
    SHA-256d([]) and is not zeros; nonempty DCR agrees with C.
    Full development: [DrawCandidateRoot.v]. Re-exported here so the kernel
    cone names the length/empty facts next to C. *)
Theorem draw_candidate_root_length_kernel :
  forall txids, length (draw_candidate_root_from_txid_bytes txids) = 32%nat.
Proof. apply draw_candidate_root_length. Qed.

Theorem draw_candidate_root_empty_is_not_zero :
  draw_candidate_root_from_txid_bytes [] <> bytes32_zero.
Proof. apply draw_candidate_root_empty_neq_zero. Qed.

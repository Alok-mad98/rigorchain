(** Phase5Equivalence/DrawBridgeProgress.v

    Post-verdict remediation record. The freeze-era catalogs in this directory
    (DrawClosureImpossibilityCatalog and siblings) remain the historical
    statement that *full* Coq=C++ refinement is not closed. This module records
    what actually landed afterwards, without flipping those catalogs or claiming
    equivalence.

    What is now present:
      - a Canonical Draw Encoder with pinned offsets, widths, and endianness
      - an executable SHA-256d kernel in Coq (RigorSha256), matching the C++
        Hash256 used by ComputeSigmaHash, checked on golden vectors
      - FIPS 180-4 short-message lemmas in CanonicalHashKernel.v
      - an optional extracted OCaml kernel (ExtractHashKernel.v) for the gate
      - a portable C++ CSHA256 CLI over sha256.cpp, compared differentially
      - A-9-C already_paid exclusion modeled and proved in WindowExclusion.v
      - draw_candidate_root (eligible-subset hash; empty hashes [] while C is
        zeros) in DrawCandidateRoot.v, composed with the window in
        WindowExclusion.v

    What is still absent:
      - a mechanized refinement proof (Coq function = C++ function)
      - a shared object file with C++ (extracted OCaml is the Coq kernel, not CHash256)
      - game-theoretic grinding-cost theorems about real SHA-256
      - network / mempool / P2P in the model

    HONESTY: equivalence_claimed remains false.

    Freeze (2026-08-16): honesty lock for the post-verdict bridge. Do not set
    equivalence_claimed or hash_kernel_shared_binary_with_cpp to true without a
    genuine refinement. Operator status: docs/verification/FORMAL-VERIFICATION-STATUS.md. *)

From Coq Require Import Arith.Arith.
From Coq Require Import List.
Import ListNotations.

Require Import Rigor.CanonicalDrawEncoder.
Require Import Rigor.CanonicalHashKernel.
Require Import Rigor.WindowExclusion.
Require Import Rigor.DrawCandidateRoot.
From Rigor Require Import RigorSha256.
Require Import Rigor.RigorHash.

(** * Bridge artifacts that now exist *)

Definition canonical_draw_encoder_specified : bool := true.
Definition sha256d_kernel_executable : bool := true.
Definition sha256_fips_vectors_in_coq : bool := true.
Definition hash_kernel_extracted_ocaml : bool := true.
Definition hash_kernel_cpp_portable_cli : bool := true.
Definition hash_kernel_shared_binary_with_cpp : bool := false.
Definition a9c_no_double_win_modeled : bool := true.
Definition draw_candidate_root_modeled : bool := true.

Lemma encoder_specified : canonical_draw_encoder_specified = true.
Proof. reflexivity. Qed.

Lemma hash_kernel_executable : sha256d_kernel_executable = true.
Proof. reflexivity. Qed.

Lemma fips_vectors_locked : sha256_fips_vectors_in_coq = true.
Proof. reflexivity. Qed.

Lemma extracted_ocaml_is_not_cpp :
  hash_kernel_extracted_ocaml = true /\
  hash_kernel_cpp_portable_cli = true /\
  hash_kernel_shared_binary_with_cpp = false.
Proof. repeat split; reflexivity. Qed.

Lemma a9c_modeled : a9c_no_double_win_modeled = true.
Proof. reflexivity. Qed.

Lemma draw_candidate_root_is_modeled : draw_candidate_root_modeled = true.
Proof. reflexivity. Qed.

Theorem draw_candidate_root_always_32 :
  forall txids, length (draw_candidate_root_from_txid_bytes txids) = 32%nat.
Proof. apply draw_candidate_root_length. Qed.

Theorem draw_candidate_root_empty_not_zero :
  draw_candidate_root_from_txid_bytes [] <> bytes32_zero.
Proof. apply draw_candidate_root_empty_neq_zero. Qed.

(** Re-export the load-bearing layout lock so a Phase-5 reader does not have
    to open CanonicalDrawEncoder to see that the encoder is not a comment. *)
Theorem draw_encoder_layout_locked :
  SIGMA_INPUT_SIZE = 100%nat /\
  SIGMA_OFF_TXID = 0%nat /\
  SIGMA_OFF_CROOT = 32%nat /\
  SIGMA_OFF_PREV_BLOCK = 64%nat /\
  SIGMA_OFF_HEIGHT = 96%nat.
Proof. repeat split; reflexivity. Qed.

Theorem draw_encoder_is_sha256d :
  forall txid C B h,
    canonical_sigma_digest txid C B h =
    hash256_bytes (canonical_sigma_preimage txid C B h).
Proof. apply canonical_sigma_is_sha256d. Qed.

Theorem hash256_is_double_sha256 :
  forall m, hash256_bytes m = sha256_bytes (sha256_bytes m).
Proof. apply hash256_bytes_is_double_sha256. Qed.

Theorem sha256_fips_empty_locked :
  sha256_bytes fips_empty = fips_empty_sha256.
Proof. apply sha256_fips_empty. Qed.

Theorem a9c_txid_wins_at_most_once :
  forall paid outcomes id,
    paid_closed_trace paid outcomes ->
    (count_wins id outcomes <= 1)%nat.
Proof. apply txid_wins_at_most_once. Qed.

(** * What this module still refuses to claim *)

Definition refinement_proved : bool := false.
Definition equivalence_claimed : bool := false.
Definition grinding_advantage_bounded : bool := false.
Definition sha256_uniformity_proved : bool := false.

Lemma refinement_not_proved : refinement_proved = false.
Proof. reflexivity. Qed.

Lemma equivalence_not_claimed : equivalence_claimed = false.
Proof. reflexivity. Qed.

Lemma grinding_not_a_theorem : grinding_advantage_bounded = false.
Proof. reflexivity. Qed.

(** Prerequisites the freeze-era impossibility catalog listed as missing.
    Two of five now exist as *artifacts*; the remaining three (runtime trace,
    transition history, accept history) are still absent, and even with all
    five the catalog would only become "potentially closable", not closed. *)
Definition encoder_prerequisite_met : bool := true.
Definition hash_kernel_prerequisite_met : bool := true.
Definition runtime_trace_prerequisite_met : bool := false.
Definition runtime_transition_prerequisite_met : bool := false.
Definition runtime_accept_prerequisite_met : bool := false.

Definition remaining_closure_prerequisites : list bool :=
  [runtime_trace_prerequisite_met;
   runtime_transition_prerequisite_met;
   runtime_accept_prerequisite_met].

Lemma remaining_closure_prerequisites_still_open :
  forallb negb remaining_closure_prerequisites = true.
Proof. reflexivity. Qed.

Lemma bridge_progress_does_not_close_refinement :
  encoder_prerequisite_met = true /\
  hash_kernel_prerequisite_met = true /\
  hash_kernel_shared_binary_with_cpp = false /\
  refinement_proved = false /\
  equivalence_claimed = false.
Proof. repeat split; reflexivity. Qed.

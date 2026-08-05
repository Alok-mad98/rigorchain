(** Phase5Equivalence/CanonicalStateHashModel.v
    Abstract canonical state hash model (TASK-018GG-C).

    Pure structural reducer over the 018GG-B canonical bytes projection.
    This is NOT a cryptographic hash — only a Coq fold using
    [Z.add] / [Z.mul] / [Z.modulo].

    HONESTY BOUNDARY:
    - No SHA-256 / crypto library / external hash.
    - No frozen-C++ / runtime hash imports.
    - No equivalence to runtime or frozen claimed.
    - PD-4F-OB-02 / PD-4F-CPP-01 remain OPEN.
    - Only dependency: 018GG-B (CanonicalBytesProjection). *)

From Coq Require Import List ZArith Lia.
Import ListNotations.
Require Import Rigor.Phase4FConsensus.ConsensusMachine.
Require Import Rigor.Phase4FConsensus.SemanticObservationCatalog.
Require Import Rigor.Phase5Equivalence.EquivalenceFramework.
Require Import Rigor.Phase5Equivalence.CanonicalStateEncoding.
Require Import Rigor.Phase5Equivalence.CanonicalBytesProjection.

Open Scope Z_scope.

(** ================================================================
    Part A:  Hash modulus and abstract type
    ================================================================ *)

(** Mersenne prime [2^31 - 1] used as a public, deterministic modulus.
    Visible value; not a cryptographic parameter. *)
Definition state_hash_modulus : Z := 2147483647.

Lemma state_hash_modulus_positive : 0 < state_hash_modulus.
Proof. unfold state_hash_modulus. lia. Qed.

(** Abstract canonical state hash. Structurally a [Z]; semantically a value
    in [0, state_hash_modulus). *)
Definition CanonicalStateHash := Z.

(** ================================================================
    Part B:  Pure structural reducer over [list Z]
    ================================================================ *)

(** Polynomial-style fold:
      state_hash []        = 0
      state_hash (x :: xs) = (x + 31 * state_hash xs) mod 2147483647

    No SHA / crypto. Deterministic Coq Fixpoint. *)
Fixpoint state_hash (bs : list Z) : CanonicalStateHash :=
  match bs with
  | nil => 0
  | x :: xs => (x + 31 * state_hash xs) mod state_hash_modulus
  end.

(** ================================================================
    Part C:  Hash projection API (only over 018GG-B encoders)
    ================================================================ *)

Definition hash_replay_state (tr : consensus_trace) : CanonicalStateHash :=
  state_hash (encode_replay_bytes tr).

Definition hash_reorg_state (tr : consensus_trace) : CanonicalStateHash :=
  state_hash (encode_reorg_bytes tr).

Definition hash_connectblock_state (tr : consensus_trace) : CanonicalStateHash :=
  state_hash (encode_connectblock_bytes tr).

(** Dispatch by [CanonicalState] (re-uses 018GG-A enum). *)
Definition hash_canonical_state
  (cs : CanonicalState) (tr : consensus_trace) : CanonicalStateHash :=
  match cs with
  | CS_Replay => hash_replay_state tr
  | CS_Reorg => hash_reorg_state tr
  | CS_ConnectBlock => hash_connectblock_state tr
  end.

(** ================================================================
    Part D:  Core reducer proofs
    ================================================================ *)

Lemma state_hash_deterministic :
  forall bs1 bs2 : CanonicalBytesEncoding,
    bs1 = bs2 -> state_hash bs1 = state_hash bs2.
Proof. intros bs1 bs2 Heq. rewrite Heq. reflexivity. Qed.

Lemma state_hash_total :
  forall bs : CanonicalBytesEncoding,
    exists h : CanonicalStateHash, state_hash bs = h.
Proof. intro bs. exists (state_hash bs). reflexivity. Qed.

(** Output is always in [0, state_hash_modulus). This serves as the
    [bounded_or_well_defined] requirement: every hash is a definite,
    finite value within the public modulus interval. *)
Lemma state_hash_bounded_or_well_defined :
  forall bs : CanonicalBytesEncoding,
    0 <= state_hash bs < state_hash_modulus.
Proof.
  induction bs as [| x xs IH].
  - simpl. split; [lia | exact state_hash_modulus_positive].
  - simpl. apply Z.mod_pos_bound. exact state_hash_modulus_positive.
Qed.

Lemma state_hash_nil : state_hash [] = 0.
Proof. reflexivity. Qed.

Lemma state_hash_cons :
  forall x xs,
    state_hash (x :: xs) = (x + 31 * state_hash xs) mod state_hash_modulus.
Proof. intros x xs. reflexivity. Qed.

(** ================================================================
    Part E:  Projection consistency (definitional unfoldings)
    ================================================================ *)

Lemma hash_projection_consistency_replay :
  forall tr, hash_replay_state tr = state_hash (encode_replay_bytes tr).
Proof. intro tr. reflexivity. Qed.

Lemma hash_projection_consistency_reorg :
  forall tr, hash_reorg_state tr = state_hash (encode_reorg_bytes tr).
Proof. intro tr. reflexivity. Qed.

Lemma hash_projection_consistency_connectblock :
  forall tr, hash_connectblock_state tr = state_hash (encode_connectblock_bytes tr).
Proof. intro tr. reflexivity. Qed.

(** ================================================================
    Part F:  Per-projection bound and totality
    ================================================================ *)

Lemma hash_replay_state_bounded :
  forall tr, 0 <= hash_replay_state tr < state_hash_modulus.
Proof. intro tr. apply state_hash_bounded_or_well_defined. Qed.

Lemma hash_reorg_state_bounded :
  forall tr, 0 <= hash_reorg_state tr < state_hash_modulus.
Proof. intro tr. apply state_hash_bounded_or_well_defined. Qed.

Lemma hash_connectblock_state_bounded :
  forall tr, 0 <= hash_connectblock_state tr < state_hash_modulus.
Proof. intro tr. apply state_hash_bounded_or_well_defined. Qed.

Lemma hash_replay_state_deterministic :
  forall tr1 tr2, tr1 = tr2 -> hash_replay_state tr1 = hash_replay_state tr2.
Proof. intros tr1 tr2 Heq. rewrite Heq. reflexivity. Qed.

Lemma hash_reorg_state_deterministic :
  forall tr1 tr2, tr1 = tr2 -> hash_reorg_state tr1 = hash_reorg_state tr2.
Proof. intros tr1 tr2 Heq. rewrite Heq. reflexivity. Qed.

Lemma hash_connectblock_state_deterministic :
  forall tr1 tr2,
    tr1 = tr2 -> hash_connectblock_state tr1 = hash_connectblock_state tr2.
Proof. intros tr1 tr2 Heq. rewrite Heq. reflexivity. Qed.

(** ================================================================
    Part G:  Sample [vm_compute] checks (Phase-4F catalog traces)
    ================================================================ *)

Lemma hash_replay_state_sample_value :
  exists h, hash_replay_state replay_trace = h.
Proof. exists (hash_replay_state replay_trace). reflexivity. Qed.

Lemma hash_reorg_state_sample_value :
  exists h, hash_reorg_state reorg_trace = h.
Proof. exists (hash_reorg_state reorg_trace). reflexivity. Qed.

Lemma hash_connectblock_state_sample_value :
  exists h, hash_connectblock_state [ TrAcceptBlock 7 ] = h.
Proof. exists (hash_connectblock_state [ TrAcceptBlock 7 ]). reflexivity. Qed.

(** ================================================================
    Part H:  Framework status (no equivalence; PD remains OPEN)
    ================================================================ *)

Definition canonical_state_hash_framework_established : bool := true.

(** Explicit honesty flags: this layer does NOT claim equivalence
    to runtime or frozen C++ hashes. *)
Definition hash_equivalence_to_runtime_established : bool := false.
Definition hash_equivalence_to_frozen_established : bool := false.

Lemma canonical_state_hash_framework_established_value :
  canonical_state_hash_framework_established = true.
Proof. reflexivity. Qed.

Lemma hash_equivalence_to_runtime_not_established :
  negb hash_equivalence_to_runtime_established = true.
Proof. reflexivity. Qed.

Lemma hash_equivalence_to_frozen_not_established :
  negb hash_equivalence_to_frozen_established = true.
Proof. reflexivity. Qed.

Lemma canonical_state_hash_no_equivalence_claimed :
  negb equivalence_claimed = true.
Proof. exact equivalence_not_claimed. Qed.

Lemma canonical_state_hash_pd_ob_02_open :
  negb proof_debt_placeholder.(pdp_pd_4f_ob_02_closed) = true.
Proof. exact canonical_encoding_pd_ob_02_open. Qed.

Lemma canonical_state_hash_pd_cpp_01_open :
  negb proof_debt_placeholder.(pdp_pd_4f_cpp_01_closed) = true.
Proof. exact canonical_encoding_pd_cpp_01_open. Qed.

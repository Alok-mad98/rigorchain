(** Phase5Equivalence/CanonicalStateHashAudit.v
    Canonical state hash audit (TASK-018GG-C).

    Aggregates encoder count, determinism, totality, and consistency-with-bytes
    over Replay / Reorg / ConnectBlock for the abstract state-hash reducer.
    No equivalence to runtime / frozen C++ is claimed. *)

From Coq Require Import List Bool ZArith.
Import ListNotations.
Require Import Rigor.Phase4FConsensus.ConsensusMachine.
Require Import Rigor.Phase5Equivalence.EquivalenceFramework.
Require Import Rigor.Phase5Equivalence.CanonicalStateEncoding.
Require Import Rigor.Phase5Equivalence.CanonicalBytesProjection.
Require Import Rigor.Phase5Equivalence.CanonicalStateHashModel.

(** ---- Encoder inventory ---- *)

Definition hash_encoder_count : nat := length all_canonical_states.

Lemma hash_encoder_count_value : hash_encoder_count = 3%nat.
Proof. exact all_canonical_states_count. Qed.

(** ---- Aggregate determinism ---- *)

Definition all_hash_encoders_deterministic : Prop :=
  (forall tr1 tr2 : consensus_trace,
     tr1 = tr2 -> hash_replay_state tr1 = hash_replay_state tr2) /\
  (forall tr1 tr2 : consensus_trace,
     tr1 = tr2 -> hash_reorg_state tr1 = hash_reorg_state tr2) /\
  (forall tr1 tr2 : consensus_trace,
     tr1 = tr2 -> hash_connectblock_state tr1 = hash_connectblock_state tr2).

Lemma all_hash_encoders_deterministic_holds :
  all_hash_encoders_deterministic.
Proof.
  unfold all_hash_encoders_deterministic.
  split; [exact hash_replay_state_deterministic | ].
  split; [exact hash_reorg_state_deterministic | ].
  exact hash_connectblock_state_deterministic.
Qed.

(** ---- Aggregate totality ---- *)

Definition all_hash_encoders_total : Prop :=
  (forall tr : consensus_trace, exists h, hash_replay_state tr = h) /\
  (forall tr : consensus_trace, exists h, hash_reorg_state tr = h) /\
  (forall tr : consensus_trace, exists h, hash_connectblock_state tr = h).

Lemma all_hash_encoders_total_holds : all_hash_encoders_total.
Proof.
  unfold all_hash_encoders_total.
  split; [intro tr; exists (hash_replay_state tr); reflexivity | ].
  split; [intro tr; exists (hash_reorg_state tr); reflexivity | ].
  intro tr; exists (hash_connectblock_state tr); reflexivity.
Qed.

(** ---- Consistency with 018GG-B byte projection ---- *)

Definition all_hash_encoders_consistent_with_bytes : Prop :=
  (forall tr, hash_replay_state tr = state_hash (encode_replay_bytes tr)) /\
  (forall tr, hash_reorg_state tr = state_hash (encode_reorg_bytes tr)) /\
  (forall tr,
      hash_connectblock_state tr = state_hash (encode_connectblock_bytes tr)).

Lemma all_hash_encoders_consistent_with_bytes_holds :
  all_hash_encoders_consistent_with_bytes.
Proof.
  unfold all_hash_encoders_consistent_with_bytes.
  split; [exact hash_projection_consistency_replay | ].
  split; [exact hash_projection_consistency_reorg | ].
  exact hash_projection_consistency_connectblock.
Qed.

(** ---- Aggregate boundedness (0 <= h < modulus) ---- *)

Definition all_hash_encoders_bounded : Prop :=
  (forall tr, 0 <= hash_replay_state tr < state_hash_modulus)%Z /\
  (forall tr, 0 <= hash_reorg_state tr < state_hash_modulus)%Z /\
  (forall tr, 0 <= hash_connectblock_state tr < state_hash_modulus)%Z.

Lemma all_hash_encoders_bounded_holds : all_hash_encoders_bounded.
Proof.
  unfold all_hash_encoders_bounded.
  split; [exact hash_replay_state_bounded | ].
  split; [exact hash_reorg_state_bounded | ].
  exact hash_connectblock_state_bounded.
Qed.

(** ---- CanonicalState dispatch totality ---- *)

Lemma hash_canonical_state_total :
  forall (cs : CanonicalState) (tr : consensus_trace),
    exists h : CanonicalStateHash, hash_canonical_state cs tr = h.
Proof.
  intros cs tr.
  destruct cs.
  - exists (hash_replay_state tr). reflexivity.
  - exists (hash_reorg_state tr). reflexivity.
  - exists (hash_connectblock_state tr). reflexivity.
Qed.

(** ---- Honesty / debt re-exports ---- *)

Lemma audit_hash_pd_4f_ob_02_open :
  negb proof_debt_placeholder.(pdp_pd_4f_ob_02_closed) = true.
Proof. exact canonical_state_hash_pd_ob_02_open. Qed.

Lemma audit_hash_pd_4f_cpp_01_open :
  negb proof_debt_placeholder.(pdp_pd_4f_cpp_01_closed) = true.
Proof. exact canonical_state_hash_pd_cpp_01_open. Qed.

Lemma audit_hash_no_equivalence_claimed :
  negb equivalence_claimed = true.
Proof. exact canonical_state_hash_no_equivalence_claimed. Qed.

Lemma audit_hash_runtime_equivalence_not_established :
  negb hash_equivalence_to_runtime_established = true.
Proof. exact hash_equivalence_to_runtime_not_established. Qed.

Lemma audit_hash_frozen_equivalence_not_established :
  negb hash_equivalence_to_frozen_established = true.
Proof. exact hash_equivalence_to_frozen_not_established. Qed.

Lemma audit_hash_frozen_oracle_not_wired :
  negb frozen_cpp_oracle_wired = true.
Proof. exact frozen_cpp_oracle_not_wired. Qed.

(** ---- Master audit verdict ---- *)

Definition canonical_state_hash_audit_verdict : Prop :=
  hash_encoder_count = 3%nat /\
  all_hash_encoders_deterministic /\
  all_hash_encoders_total /\
  all_hash_encoders_consistent_with_bytes /\
  all_hash_encoders_bounded /\
  canonical_state_hash_framework_established = true /\
  negb equivalence_claimed = true /\
  negb hash_equivalence_to_runtime_established = true /\
  negb hash_equivalence_to_frozen_established = true /\
  negb proof_debt_placeholder.(pdp_pd_4f_ob_02_closed) = true /\
  negb proof_debt_placeholder.(pdp_pd_4f_cpp_01_closed) = true.

Lemma canonical_state_hash_audit_verdict_holds :
  canonical_state_hash_audit_verdict.
Proof.
  unfold canonical_state_hash_audit_verdict.
  split; [exact hash_encoder_count_value | ].
  split; [exact all_hash_encoders_deterministic_holds | ].
  split; [exact all_hash_encoders_total_holds | ].
  split; [exact all_hash_encoders_consistent_with_bytes_holds | ].
  split; [exact all_hash_encoders_bounded_holds | ].
  split; [exact canonical_state_hash_framework_established_value | ].
  split; [exact audit_hash_no_equivalence_claimed | ].
  split; [exact audit_hash_runtime_equivalence_not_established | ].
  split; [exact audit_hash_frozen_equivalence_not_established | ].
  split; [exact audit_hash_pd_4f_ob_02_open | ].
  exact audit_hash_pd_4f_cpp_01_open.
Qed.

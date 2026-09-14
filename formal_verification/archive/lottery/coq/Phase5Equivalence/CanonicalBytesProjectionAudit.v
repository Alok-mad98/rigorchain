(** Phase5Equivalence/CanonicalBytesProjectionAudit.v
    Canonical bytes projection audit (TASK-018GG-B).

    Aggregates encoder count, determinism, totality and non-emptiness for the
    Replay / Reorg / ConnectBlock byte encoders built in 018GG-B.
    No equivalence is claimed; PD-4F-OB-02 / PD-4F-CPP-01 remain OPEN. *)

From Coq Require Import List Bool ZArith.
Import ListNotations.
Require Import Rigor.Phase4FConsensus.ConsensusMachine.
Require Import Rigor.Phase5Equivalence.EquivalenceFramework.
Require Import Rigor.Phase5Equivalence.CanonicalStateEncoding.
Require Import Rigor.Phase5Equivalence.CanonicalBytesProjection.

(** ---- Encoder inventory ---- *)

Definition canonical_bytes_encoder_count : nat := length all_canonical_states.

Lemma canonical_bytes_encoder_count_value :
  canonical_bytes_encoder_count = 3%nat.
Proof. exact all_canonical_states_count. Qed.

(** ---- Aggregate determinism ---- *)

Definition all_bytes_encoders_deterministic : Prop :=
  (forall tr1 tr2 : consensus_trace,
     tr1 = tr2 -> encode_replay_bytes tr1 = encode_replay_bytes tr2) /\
  (forall tr1 tr2 : consensus_trace,
     tr1 = tr2 -> encode_reorg_bytes tr1 = encode_reorg_bytes tr2) /\
  (forall tr1 tr2 : consensus_trace,
     tr1 = tr2 -> encode_connectblock_bytes tr1 = encode_connectblock_bytes tr2).

Lemma all_bytes_encoders_deterministic_holds :
  all_bytes_encoders_deterministic.
Proof.
  repeat split; intros; eauto using replay_bytes_encoder_deterministic,
    reorg_bytes_encoder_deterministic, connectblock_bytes_encoder_deterministic.
Qed.

(** ---- Aggregate totality ---- *)

Definition all_bytes_encoders_total : Prop :=
  (forall tr : consensus_trace,
     exists bytes, encode_replay_bytes tr = bytes) /\
  (forall tr : consensus_trace,
     exists bytes, encode_reorg_bytes tr = bytes) /\
  (forall tr : consensus_trace,
     exists bytes, encode_connectblock_bytes tr = bytes).

Lemma all_bytes_encoders_total_holds : all_bytes_encoders_total.
Proof.
  repeat split; intro tr; eauto using replay_bytes_encoder_total,
    reorg_bytes_encoder_total, connectblock_bytes_encoder_total.
Qed.

(** ---- Aggregate non-emptiness ---- *)

Definition all_bytes_encoders_nonempty : Prop :=
  (forall tr : consensus_trace, encode_replay_bytes tr <> nil) /\
  (forall tr : consensus_trace, encode_reorg_bytes tr <> nil) /\
  (forall tr : consensus_trace, encode_connectblock_bytes tr <> nil).

Lemma all_bytes_encoders_nonempty_holds : all_bytes_encoders_nonempty.
Proof.
  repeat split; intros; eauto using replay_bytes_encoder_nonempty,
    reorg_bytes_encoder_nonempty, connectblock_bytes_encoder_nonempty.
Qed.

(** ---- CanonicalState dispatch totality / non-emptiness ---- *)

Lemma encode_canonical_bytes_total :
  forall (cs : CanonicalState) (tr : consensus_trace),
    exists bytes : CanonicalBytesEncoding, encode_canonical_bytes cs tr = bytes.
Proof.
  intros cs tr.
  destruct cs.
  - exists (encode_replay_bytes tr). reflexivity.
  - exists (encode_reorg_bytes tr). reflexivity.
  - exists (encode_connectblock_bytes tr). reflexivity.
Qed.

Lemma encode_canonical_bytes_nonempty :
  forall (cs : CanonicalState) (tr : consensus_trace),
    encode_canonical_bytes cs tr <> nil.
Proof.
  intros cs tr.
  destruct cs; eauto using replay_bytes_encoder_nonempty,
    reorg_bytes_encoder_nonempty, connectblock_bytes_encoder_nonempty.
Qed.

(** ---- Fixed-width audit (each encoder has a known length) ---- *)

Definition all_bytes_encoders_fixed_width : Prop :=
  (forall tr, length (encode_replay_bytes tr) = 32%nat) /\
  (forall tr, length (encode_reorg_bytes tr) = 40%nat) /\
  (forall tr, length (encode_connectblock_bytes tr) = 32%nat).

Lemma all_bytes_encoders_fixed_width_holds :
  all_bytes_encoders_fixed_width.
Proof.
  repeat split; intros; eauto using replay_bytes_encoder_length,
    reorg_bytes_encoder_length, connectblock_bytes_encoder_length.
Qed.

(** ---- Honesty / debt re-exports ---- *)

Lemma audit_bytes_pd_4f_ob_02_open :
  negb proof_debt_placeholder.(pdp_pd_4f_ob_02_closed) = true.
Proof. exact canonical_bytes_pd_ob_02_open. Qed.

Lemma audit_bytes_pd_4f_cpp_01_open :
  negb proof_debt_placeholder.(pdp_pd_4f_cpp_01_closed) = true.
Proof. exact canonical_bytes_pd_cpp_01_open. Qed.

Lemma audit_bytes_no_equivalence_claimed :
  negb equivalence_claimed = true.
Proof. exact canonical_bytes_no_equivalence_claimed. Qed.

Lemma audit_bytes_frozen_oracle_not_wired :
  negb frozen_cpp_oracle_wired = true.
Proof. exact frozen_cpp_oracle_not_wired. Qed.

(** ---- Master audit verdict ---- *)

Definition canonical_bytes_projection_audit_verdict : Prop :=
  canonical_bytes_encoder_count = 3%nat /\
  all_bytes_encoders_deterministic /\
  all_bytes_encoders_total /\
  all_bytes_encoders_nonempty /\
  all_bytes_encoders_fixed_width /\
  canonical_bytes_framework_established = true /\
  negb equivalence_claimed = true /\
  negb proof_debt_placeholder.(pdp_pd_4f_ob_02_closed) = true /\
  negb proof_debt_placeholder.(pdp_pd_4f_cpp_01_closed) = true.

Lemma canonical_bytes_projection_audit_verdict_holds :
  canonical_bytes_projection_audit_verdict.
Proof.
  unfold canonical_bytes_projection_audit_verdict.
  split; [exact canonical_bytes_encoder_count_value | ].
  split; [exact all_bytes_encoders_deterministic_holds | ].
  split; [exact all_bytes_encoders_total_holds | ].
  split; [exact all_bytes_encoders_nonempty_holds | ].
  split; [exact all_bytes_encoders_fixed_width_holds | ].
  split; [exact canonical_bytes_framework_established_value | ].
  split; [exact audit_bytes_no_equivalence_claimed | ].
  split; [exact audit_bytes_pd_4f_ob_02_open | ].
  exact audit_bytes_pd_4f_cpp_01_open.
Qed.

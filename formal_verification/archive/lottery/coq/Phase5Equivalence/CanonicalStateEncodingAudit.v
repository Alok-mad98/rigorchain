(** Phase5Equivalence/CanonicalStateEncodingAudit.v
    Canonical state encoding audit (TASK-018GG-A).

    Aggregates encoder count, determinism, totality, and non-emptiness over
    Replay / Reorg / ConnectBlock. Proof debts PD-4F-OB-02 and PD-4F-CPP-01
    remain OPEN; no equivalence is claimed. *)

From Coq Require Import List Bool Arith.
Import ListNotations.
Require Import Rigor.Phase4FConsensus.ConsensusMachine.
Require Import Rigor.Phase5Equivalence.EquivalenceFramework.
Require Import Rigor.Phase5Equivalence.CanonicalStateEncoding.

(** ---- Encoder inventory ---- *)

Definition canonical_encoder_count : nat := length all_canonical_states.

Lemma canonical_encoder_count_value : canonical_encoder_count = 3%nat.
Proof. exact all_canonical_states_count. Qed.

(** ---- Aggregate determinism ---- *)

Definition all_encoders_deterministic : Prop :=
  (forall tr1 tr2 : consensus_trace,
     tr1 = tr2 -> encode_replay_state tr1 = encode_replay_state tr2) /\
  (forall tr1 tr2 : consensus_trace,
     tr1 = tr2 -> encode_reorg_state tr1 = encode_reorg_state tr2) /\
  (forall tr1 tr2 : consensus_trace,
     tr1 = tr2 -> encode_connectblock_state tr1 = encode_connectblock_state tr2).

Lemma all_encoders_deterministic_holds : all_encoders_deterministic.
Proof.
  repeat split; intros; eauto using replay_encoder_deterministic,
    reorg_encoder_deterministic, connectblock_encoder_deterministic.
Qed.

(** ---- Aggregate totality ---- *)

Definition all_encoders_total : Prop :=
  (forall tr : consensus_trace,
     exists enc, encode_replay_state tr = enc) /\
  (forall tr : consensus_trace,
     exists enc, encode_reorg_state tr = enc) /\
  (forall tr : consensus_trace,
     exists enc, encode_connectblock_state tr = enc).

Lemma all_encoders_total_holds : all_encoders_total.
Proof.
  repeat split; intro tr; eauto using replay_encoder_total,
    reorg_encoder_total, connectblock_encoder_total.
Qed.

(** ---- Aggregate non-emptiness ---- *)

Definition all_encoders_nonempty : Prop :=
  (forall tr : consensus_trace, encode_replay_state tr <> nil) /\
  (forall tr : consensus_trace, encode_reorg_state tr <> nil) /\
  (forall tr : consensus_trace, encode_connectblock_state tr <> nil).

Lemma all_encoders_nonempty_holds : all_encoders_nonempty.
Proof.
  repeat split; intros; eauto using replay_encoder_nonempty,
    reorg_encoder_nonempty, connectblock_encoder_nonempty.
Qed.

(** ---- Canonical-state dispatch totality ---- *)

Lemma encode_canonical_state_total :
  forall (cs : CanonicalState) (tr : consensus_trace),
    exists enc : canonical_encoding, encode_canonical_state cs tr = enc.
Proof.
  intros cs tr.
  destruct cs.
  - exists (encode_replay_state tr). simpl. reflexivity.
  - exists (encode_reorg_state tr). simpl. reflexivity.
  - exists (encode_connectblock_state tr). simpl. reflexivity.
Qed.

Lemma encode_canonical_state_nonempty :
  forall (cs : CanonicalState) (tr : consensus_trace),
    encode_canonical_state cs tr <> nil.
Proof.
  intros cs tr.
  destruct cs; eauto using replay_encoder_nonempty,
    reorg_encoder_nonempty, connectblock_encoder_nonempty.
Qed.

(** ---- Proof debts and honesty (re-export) ---- *)

Lemma audit_pd_4f_ob_02_open :
  negb proof_debt_placeholder.(pdp_pd_4f_ob_02_closed) = true.
Proof. exact canonical_encoding_pd_ob_02_open. Qed.

Lemma audit_pd_4f_cpp_01_open :
  negb proof_debt_placeholder.(pdp_pd_4f_cpp_01_closed) = true.
Proof. exact canonical_encoding_pd_cpp_01_open. Qed.

Lemma audit_no_equivalence_claimed :
  negb equivalence_claimed = true.
Proof. exact canonical_encoding_no_equivalence_claimed. Qed.

Lemma audit_frozen_oracle_not_wired :
  negb frozen_cpp_oracle_wired = true.
Proof. exact frozen_cpp_oracle_not_wired. Qed.

(** ---- Master audit verdict ---- *)

Definition canonical_state_encoding_audit_verdict : Prop :=
  canonical_encoder_count = 3%nat /\
  all_encoders_deterministic /\
  all_encoders_total /\
  all_encoders_nonempty /\
  canonical_encoding_framework_established = true /\
  negb equivalence_claimed = true /\
  negb proof_debt_placeholder.(pdp_pd_4f_ob_02_closed) = true /\
  negb proof_debt_placeholder.(pdp_pd_4f_cpp_01_closed) = true.

Lemma canonical_state_encoding_audit_verdict_holds :
  canonical_state_encoding_audit_verdict.
Proof.
  unfold canonical_state_encoding_audit_verdict.
  split; [exact canonical_encoder_count_value | ].
  split; [exact all_encoders_deterministic_holds | ].
  split; [exact all_encoders_total_holds | ].
  split; [exact all_encoders_nonempty_holds | ].
  split; [exact canonical_encoding_framework_established_value | ].
  split; [exact audit_no_equivalence_claimed | ].
  split; [exact audit_pd_4f_ob_02_open | ].
  exact audit_pd_4f_cpp_01_open.
Qed.

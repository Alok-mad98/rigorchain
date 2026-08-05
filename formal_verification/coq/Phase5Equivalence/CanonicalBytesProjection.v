(** Phase5Equivalence/CanonicalBytesProjection.v
    Canonical bytes projection (TASK-018GG-B).

    Lifts the 018GG-A canonical state encoding ([list Z] of trace_height /
    transition counts / accept counts / rollback depth / last block id) into a
    fixed-width byte projection ([list Z] where each entry is a Z in [0..255]).

    HONESTY BOUNDARY:
    - No SHA-256 / hash kernel; the byte projection is pure functional Z arithmetic.
    - No runtime / frozen payload imports.
    - No equivalence claimed; PD-4F-OB-02 / PD-4F-CPP-01 remain OPEN.
    - Reuses ONLY the canonical fields from 018GG-A. *)

From Coq Require Import List ZArith Bool Arith.
Import ListNotations.
Require Import Rigor.Phase4FConsensus.ConsensusMachine.
Require Import Rigor.Phase4FConsensus.SemanticObservationCatalog.
Require Import Rigor.Phase5Equivalence.EquivalenceFramework.
Require Import Rigor.Phase5Equivalence.CanonicalStateEncoding.

Open Scope Z_scope.

(** ================================================================
    Part A:  Bytes vocabulary
    ================================================================ *)

(** A canonical bytes encoding is a list of Z values intended to live in
    [0..255]. The structural projection here keeps the type as [list Z];
    no SHA / hash kernel is invoked. *)
Definition CanonicalBytesEncoding := list Z.

(** ================================================================
    Part B:  Z -> fixed-width little-endian byte list
    ================================================================ *)

(** Decompose a Z into [n] little-endian bytes (each in [0..255]). *)
Fixpoint le_bytes_n (z : Z) (n : nat) : list Z :=
  match n with
  | O => []
  | S k => (z mod 256) :: le_bytes_n (z / 256) k
  end.

Lemma le_bytes_n_length :
  forall n z, length (le_bytes_n z n) = n.
Proof.
  induction n; intro z; simpl; auto.
Qed.

Lemma le_bytes_n_succ :
  forall n z, le_bytes_n z (S n) = (z mod 256) :: le_bytes_n (z / 256) n.
Proof. intros n z. reflexivity. Qed.

(** Project one Z field into 8 little-endian bytes (fixed width). *)
Definition project_z_to_bytes (z : Z) : list Z := le_bytes_n z 8.

Lemma project_z_to_bytes_length :
  forall z, length (project_z_to_bytes z) = 8%nat.
Proof. intro z. apply le_bytes_n_length. Qed.

Lemma project_z_to_bytes_nonempty :
  forall z, project_z_to_bytes z <> nil.
Proof.
  intro z. unfold project_z_to_bytes. simpl. discriminate.
Qed.

(** ================================================================
    Part C:  canonical_encoding -> bytes
    ================================================================ *)

Definition canonical_to_bytes (enc : canonical_encoding) : CanonicalBytesEncoding :=
  flat_map project_z_to_bytes enc.

Lemma canonical_to_bytes_cons :
  forall h t,
    canonical_to_bytes (h :: t) = project_z_to_bytes h ++ canonical_to_bytes t.
Proof. intros h t. reflexivity. Qed.

Lemma canonical_to_bytes_length :
  forall enc, length (canonical_to_bytes enc) = (length enc * 8)%nat.
Proof.
  induction enc as [| h t IH]; [reflexivity|].
  rewrite canonical_to_bytes_cons, app_length.
  rewrite project_z_to_bytes_length, IH. simpl. reflexivity.
Qed.

Lemma canonical_to_bytes_nonempty :
  forall enc, enc <> nil -> canonical_to_bytes enc <> nil.
Proof.
  intros enc Hne.
  destruct enc as [| h t]; [contradiction|].
  rewrite canonical_to_bytes_cons.
  intro Habs.
  apply (f_equal (@length Z)) in Habs.
  rewrite app_length, project_z_to_bytes_length in Habs.
  simpl in Habs. discriminate Habs.
Qed.

Lemma canonical_to_bytes_deterministic :
  forall enc1 enc2 : canonical_encoding,
    enc1 = enc2 -> canonical_to_bytes enc1 = canonical_to_bytes enc2.
Proof. intros enc1 enc2 Heq. rewrite Heq. reflexivity. Qed.

(** ================================================================
    Part D:  Per-component bytes encoders (over consensus_trace)
    ================================================================ *)

Definition encode_replay_bytes (tr : consensus_trace) : CanonicalBytesEncoding :=
  canonical_to_bytes (encode_replay_state tr).

Definition encode_reorg_bytes (tr : consensus_trace) : CanonicalBytesEncoding :=
  canonical_to_bytes (encode_reorg_state tr).

Definition encode_connectblock_bytes (tr : consensus_trace) : CanonicalBytesEncoding :=
  canonical_to_bytes (encode_connectblock_state tr).

(** Dispatch by [CanonicalState] (re-uses 018GG-A enum). *)
Definition encode_canonical_bytes
  (cs : CanonicalState) (tr : consensus_trace) : CanonicalBytesEncoding :=
  match cs with
  | CS_Replay => encode_replay_bytes tr
  | CS_Reorg => encode_reorg_bytes tr
  | CS_ConnectBlock => encode_connectblock_bytes tr
  end.

(** ================================================================
    Part E:  Replay bytes encoder proofs
    ================================================================ *)

Lemma replay_bytes_encoder_deterministic :
  forall tr1 tr2 : consensus_trace,
    tr1 = tr2 -> encode_replay_bytes tr1 = encode_replay_bytes tr2.
Proof. intros tr1 tr2 Heq. rewrite Heq. reflexivity. Qed.

Lemma replay_bytes_encoder_total :
  forall tr : consensus_trace,
    exists bytes : CanonicalBytesEncoding, encode_replay_bytes tr = bytes.
Proof. intro tr. exists (encode_replay_bytes tr). reflexivity. Qed.

Lemma replay_bytes_encoder_nonempty :
  forall tr : consensus_trace, encode_replay_bytes tr <> nil.
Proof.
  intro tr. unfold encode_replay_bytes.
  apply canonical_to_bytes_nonempty.
  apply replay_encoder_nonempty.
Qed.

Lemma replay_bytes_encoder_length :
  forall tr, length (encode_replay_bytes tr) = 32%nat.
Proof.
  intro tr. unfold encode_replay_bytes.
  rewrite canonical_to_bytes_length.
  rewrite replay_encoder_field_count. reflexivity.
Qed.

(** ================================================================
    Part F:  Reorg bytes encoder proofs
    ================================================================ *)

Lemma reorg_bytes_encoder_deterministic :
  forall tr1 tr2 : consensus_trace,
    tr1 = tr2 -> encode_reorg_bytes tr1 = encode_reorg_bytes tr2.
Proof. intros tr1 tr2 Heq. rewrite Heq. reflexivity. Qed.

Lemma reorg_bytes_encoder_total :
  forall tr : consensus_trace,
    exists bytes : CanonicalBytesEncoding, encode_reorg_bytes tr = bytes.
Proof. intro tr. exists (encode_reorg_bytes tr). reflexivity. Qed.

Lemma reorg_bytes_encoder_nonempty :
  forall tr : consensus_trace, encode_reorg_bytes tr <> nil.
Proof.
  intro tr. unfold encode_reorg_bytes.
  apply canonical_to_bytes_nonempty.
  apply reorg_encoder_nonempty.
Qed.

Lemma reorg_bytes_encoder_length :
  forall tr, length (encode_reorg_bytes tr) = 40%nat.
Proof.
  intro tr. unfold encode_reorg_bytes.
  rewrite canonical_to_bytes_length.
  rewrite reorg_encoder_field_count. reflexivity.
Qed.

(** ================================================================
    Part G:  ConnectBlock bytes encoder proofs
    ================================================================ *)

Lemma connectblock_bytes_encoder_deterministic :
  forall tr1 tr2 : consensus_trace,
    tr1 = tr2 -> encode_connectblock_bytes tr1 = encode_connectblock_bytes tr2.
Proof. intros tr1 tr2 Heq. rewrite Heq. reflexivity. Qed.

Lemma connectblock_bytes_encoder_total :
  forall tr : consensus_trace,
    exists bytes : CanonicalBytesEncoding, encode_connectblock_bytes tr = bytes.
Proof. intro tr. exists (encode_connectblock_bytes tr). reflexivity. Qed.

Lemma connectblock_bytes_encoder_nonempty :
  forall tr : consensus_trace, encode_connectblock_bytes tr <> nil.
Proof.
  intro tr. unfold encode_connectblock_bytes.
  apply canonical_to_bytes_nonempty.
  apply connectblock_encoder_nonempty.
Qed.

Lemma connectblock_bytes_encoder_length :
  forall tr, length (encode_connectblock_bytes tr) = 32%nat.
Proof.
  intro tr. unfold encode_connectblock_bytes.
  rewrite canonical_to_bytes_length.
  rewrite connectblock_encoder_field_count. reflexivity.
Qed.

(** ================================================================
    Part H:  Sample [vm_compute] checks (Phase-4F catalog traces)
    ================================================================ *)

(** Replay: encode [3;3;3;3] = 4 fields × 8 bytes = 32 bytes. The first byte
    of each field is the field value mod 256. *)
Lemma encode_replay_bytes_sample :
  encode_replay_bytes replay_trace =
    [ 3; 0; 0; 0; 0; 0; 0; 0;
      3; 0; 0; 0; 0; 0; 0; 0;
      3; 0; 0; 0; 0; 0; 0; 0;
      3; 0; 0; 0; 0; 0; 0; 0 ].
Proof. vm_compute. reflexivity. Qed.

(** Reorg: encode [3;6;5;2;5] = 5 fields × 8 bytes = 40 bytes. *)
Lemma encode_reorg_bytes_sample :
  encode_reorg_bytes reorg_trace =
    [ 3; 0; 0; 0; 0; 0; 0; 0;
      6; 0; 0; 0; 0; 0; 0; 0;
      5; 0; 0; 0; 0; 0; 0; 0;
      2; 0; 0; 0; 0; 0; 0; 0;
      5; 0; 0; 0; 0; 0; 0; 0 ].
Proof. vm_compute. reflexivity. Qed.

(** ConnectBlock: encode [1;1;1;7] = 4 fields × 8 bytes = 32 bytes. *)
Lemma encode_connectblock_bytes_sample :
  encode_connectblock_bytes [ TrAcceptBlock 7 ] =
    [ 1; 0; 0; 0; 0; 0; 0; 0;
      1; 0; 0; 0; 0; 0; 0; 0;
      1; 0; 0; 0; 0; 0; 0; 0;
      7; 0; 0; 0; 0; 0; 0; 0 ].
Proof. vm_compute. reflexivity. Qed.

(** ================================================================
    Part I:  Framework status (no equivalence; PD remains OPEN)
    ================================================================ *)

Definition canonical_bytes_framework_established : bool := true.

Lemma canonical_bytes_framework_established_value :
  canonical_bytes_framework_established = true.
Proof. reflexivity. Qed.

Lemma canonical_bytes_no_equivalence_claimed :
  negb equivalence_claimed = true.
Proof. exact equivalence_not_claimed. Qed.

Lemma canonical_bytes_pd_ob_02_open :
  negb proof_debt_placeholder.(pdp_pd_4f_ob_02_closed) = true.
Proof. exact canonical_encoding_pd_ob_02_open. Qed.

Lemma canonical_bytes_pd_cpp_01_open :
  negb proof_debt_placeholder.(pdp_pd_4f_cpp_01_closed) = true.
Proof. exact canonical_encoding_pd_cpp_01_open. Qed.

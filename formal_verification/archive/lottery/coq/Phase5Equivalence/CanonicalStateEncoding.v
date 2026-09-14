(** Phase5Equivalence/CanonicalStateEncoding.v
    Canonical state encoding bridge (TASK-018GG-A).

    First canonical-state bridge layer toward closing PD-4F-OB-02 and
    PD-4F-CPP-01. Encoders are purely functional and deterministic over the
    Phase-4F Coq consensus machine; they pack trace_height, transition counts,
    rollback depth, and block identifiers already present in the model.

    HONESTY BOUNDARY:
    - No SHA-256 / hash kernel.
    - No runtime calls or frozen-payload imports.
    - No equivalence claimed ([equivalence_claimed] remains false).
    - PD-4F-OB-02 and PD-4F-CPP-01 remain OPEN (scaffold only). *)

From Coq Require Import List ZArith Bool Arith.
Import ListNotations.
Require Import Rigor.Phase4FConsensus.ConsensusMachine.
Require Import Rigor.Phase4FConsensus.SemanticObservationCatalog.
Require Import Rigor.Phase4GRefinement.RefinementProjection.
Require Import Rigor.Phase5Equivalence.EquivalenceFramework.

(** ================================================================
    Part A:  Canonical state vocabulary
    ================================================================ *)

Inductive CanonicalState :=
| CS_Replay
| CS_Reorg
| CS_ConnectBlock.

Definition all_canonical_states : list CanonicalState :=
  [ CS_Replay; CS_Reorg; CS_ConnectBlock ].

Lemma all_canonical_states_count : length all_canonical_states = 3%nat.
Proof. reflexivity. Qed.

(** Canonical payload: ordered list of [Z] fields (no hash, no oracle bytes). *)
Definition canonical_encoding := list Z.

(** ================================================================
    Part B:  Trace-derived fields (Coq model only)
    ================================================================ *)

Definition trace_transition_count (tr : consensus_trace) : nat := length tr.

Fixpoint trace_accept_count (tr : consensus_trace) : nat :=
  match tr with
  | nil => O
  | TrAcceptBlock _ :: rest => S (trace_accept_count rest)
  | _ :: rest => trace_accept_count rest
  end.

Fixpoint trace_rollback_depth (tr : consensus_trace) : nat :=
  match tr with
  | nil => O
  | TrRollback k :: rest => Nat.max k (trace_rollback_depth rest)
  | _ :: rest => trace_rollback_depth rest
  end.

Fixpoint trace_last_block_id (tr : consensus_trace) : Z :=
  match tr with
  | nil => (-1)%Z
  | TrAcceptBlock bid :: rest =>
      let rest_id := trace_last_block_id rest in
      if Z.eqb rest_id (-1)%Z then Z.of_nat bid else rest_id
  | _ :: rest => trace_last_block_id rest
  end.

(** ================================================================
    Part C:  Pure functional encoders
    ================================================================ *)

(** Replay: final height + transition / accept counts + last block tag. *)
Definition encode_replay_state (tr : consensus_trace) : canonical_encoding :=
  [ trace_height tr;
    Z.of_nat (trace_transition_count tr);
    Z.of_nat (trace_accept_count tr);
    trace_last_block_id tr ].

(** Reorg: same fields plus maximum rollback depth observed in the trace. *)
Definition encode_reorg_state (tr : consensus_trace) : canonical_encoding :=
  [ trace_height tr;
    Z.of_nat (trace_transition_count tr);
    Z.of_nat (trace_accept_count tr);
    Z.of_nat (trace_rollback_depth tr);
    trace_last_block_id tr ].

(** ConnectBlock: height + counts + last accepted block identifier. *)
Definition encode_connectblock_state (tr : consensus_trace) : canonical_encoding :=
  [ trace_height tr;
    Z.of_nat (trace_transition_count tr);
    Z.of_nat (trace_accept_count tr);
    trace_last_block_id tr ].

Definition encode_canonical_state
  (cs : CanonicalState) (tr : consensus_trace) : canonical_encoding :=
  match cs with
  | CS_Replay => encode_replay_state tr
  | CS_Reorg => encode_reorg_state tr
  | CS_ConnectBlock => encode_connectblock_state tr
  end.

(** ================================================================
    Part D:  Replay encoder proofs
    ================================================================ *)

Lemma replay_encoder_deterministic :
  forall tr1 tr2 : consensus_trace,
    tr1 = tr2 -> encode_replay_state tr1 = encode_replay_state tr2.
Proof. intros tr1 tr2 Heq. rewrite Heq. reflexivity. Qed.

Lemma replay_encoder_total :
  forall tr : consensus_trace,
    exists enc : canonical_encoding, encode_replay_state tr = enc.
Proof. intro tr. exists (encode_replay_state tr). reflexivity. Qed.

Lemma replay_encoder_nonempty :
  forall tr : consensus_trace, encode_replay_state tr <> nil.
Proof.
  intro tr. intro Hnil.
  discriminate Hnil.
Qed.

Lemma replay_encoder_field_count :
  forall tr, length (encode_replay_state tr) = 4%nat.
Proof. intro tr. reflexivity. Qed.

(** ================================================================
    Part E:  Reorg encoder proofs
    ================================================================ *)

Lemma reorg_encoder_deterministic :
  forall tr1 tr2 : consensus_trace,
    tr1 = tr2 -> encode_reorg_state tr1 = encode_reorg_state tr2.
Proof. intros tr1 tr2 Heq. rewrite Heq. reflexivity. Qed.

Lemma reorg_encoder_total :
  forall tr : consensus_trace,
    exists enc : canonical_encoding, encode_reorg_state tr = enc.
Proof. intro tr. exists (encode_reorg_state tr). reflexivity. Qed.

Lemma reorg_encoder_nonempty :
  forall tr : consensus_trace, encode_reorg_state tr <> nil.
Proof.
  intro tr. intro Hnil.
  discriminate Hnil.
Qed.

Lemma reorg_encoder_field_count :
  forall tr, length (encode_reorg_state tr) = 5%nat.
Proof. intro tr. reflexivity. Qed.

(** ================================================================
    Part F:  ConnectBlock encoder proofs
    ================================================================ *)

Lemma connectblock_encoder_deterministic :
  forall tr1 tr2 : consensus_trace,
    tr1 = tr2 -> encode_connectblock_state tr1 = encode_connectblock_state tr2.
Proof. intros tr1 tr2 Heq. rewrite Heq. reflexivity. Qed.

Lemma connectblock_encoder_total :
  forall tr : consensus_trace,
    exists enc : canonical_encoding, encode_connectblock_state tr = enc.
Proof. intro tr. exists (encode_connectblock_state tr). reflexivity. Qed.

Lemma connectblock_encoder_nonempty :
  forall tr : consensus_trace, encode_connectblock_state tr <> nil.
Proof.
  intro tr. intro Hnil.
  discriminate Hnil.
Qed.

Lemma connectblock_encoder_field_count :
  forall tr, length (encode_connectblock_state tr) = 4%nat.
Proof. intro tr. reflexivity. Qed.

(** ================================================================
    Part G:  Sample encodings (Phase-4F catalog traces, no frozen imports)
    ================================================================ *)

Lemma encode_replay_sample_height :
  encode_replay_state replay_trace = [ 3%Z; 3%Z; 3%Z; 3%Z ].
Proof. vm_compute. reflexivity. Qed.

Lemma encode_reorg_sample_height :
  encode_reorg_state reorg_trace = [ 3%Z; 6%Z; 5%Z; 2%Z; 5%Z ].
Proof. vm_compute. reflexivity. Qed.

Lemma encode_connectblock_sample :
  encode_connectblock_state [ TrAcceptBlock 7 ] = [ 1%Z; 1%Z; 1%Z; 7%Z ].
Proof. vm_compute. reflexivity. Qed.

(** Encoder agrees with [trace_height] as the leading field (no hash fabrication). *)
Lemma encode_replay_leading_field_is_height :
  forall tr,
    match encode_replay_state tr with
    | h :: _ => h = trace_height tr
    | nil => True
    end.
Proof. intro tr. simpl. reflexivity. Qed.

Lemma encode_reorg_leading_field_is_height :
  forall tr,
    match encode_reorg_state tr with
    | h :: _ => h = trace_height tr
    | nil => True
    end.
Proof. intro tr. simpl. reflexivity. Qed.

Lemma encode_connectblock_leading_field_is_height :
  forall tr,
    match encode_connectblock_state tr with
    | h :: _ => h = trace_height tr
    | nil => True
    end.
Proof. intro tr. simpl. reflexivity. Qed.

(** ================================================================
    Part H:  Framework status (no equivalence, debts OPEN)
    ================================================================ *)

Definition canonical_encoding_framework_established : bool := true.

Lemma canonical_encoding_framework_established_value :
  canonical_encoding_framework_established = true.
Proof. reflexivity. Qed.

Lemma canonical_encoding_no_equivalence_claimed :
  negb equivalence_claimed = true.
Proof. exact equivalence_not_claimed. Qed.

Lemma canonical_encoding_pd_ob_02_open :
  negb proof_debt_placeholder.(pdp_pd_4f_ob_02_closed) = true.
Proof. destruct proof_debt_all_open as [? [? ?]]. exact H. Qed.

Lemma canonical_encoding_pd_cpp_01_open :
  negb proof_debt_placeholder.(pdp_pd_4f_cpp_01_closed) = true.
Proof. destruct proof_debt_all_open as [? [? ?]]. exact H0. Qed.

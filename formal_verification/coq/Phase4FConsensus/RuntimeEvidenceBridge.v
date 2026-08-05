(** Phase4FConsensus/RuntimeEvidenceBridge.v
    Frozen runtime evidence -> Oracle observation bridge (PHASE-4F-001G).

    Types the conversion from FROZEN runtime evidence (produced by
    TASK-RES-LIVE-MINT-INTEGRATION-003 / TASK-RES-SOAK-READY-004 /
    TASK-RES-MULTINODE-CONSENSUS-CLOSURE-005) into Phase-4F [OracleObservation]
    records. AUDIT ONLY: no execution, no RPC, no node access; no consensus /
    RFC / SHA256 / ConvergeRoot / ConnectBlock / mining / runtime / evidence is
    modified. The evidence is consumed as constants. *)

From Coq Require Import List ZArith.
Import ListNotations.
Require Import Rigor.Phase4FConsensus.ConsensusOracleBridge.

(** REQUIRED-1: kinds of runtime evidence the bridge can carry. *)
Inductive RuntimeEvidenceKind :=
  | REK_DrawWinner
  | REK_ResMintAccepted
  | REK_ConnectBlockAccepted
  | REK_ReorgReplay
  | REK_MultinodeConsensus.

(** Frozen acceptance status as reported by the runtime evidence. *)
Inductive RuntimeStatus :=
  | RST_Accepted
  | RST_Pass
  | RST_Rejected
  | RST_Unknown.

(** REQUIRED-1: one frozen runtime evidence record. Hashes / txids are encoded
    as their 256-bit big-endian integer value (Z). *)
Record RuntimeEvidenceRecord := {
  rer_task_id : nat;            (* 3, 4, or 5 *)
  rer_kind : RuntimeEvidenceKind;
  rer_block_hash : Z;
  rer_height : Z;
  rer_winner_txid : Z;
  rer_status : RuntimeStatus;
}.

(** Map evidence kind to the bridge component it speaks to. *)
Definition component_of_kind (k : RuntimeEvidenceKind) : BridgeComponent :=
  match k with
  | REK_DrawWinner => BC_Draw
  | REK_ResMintAccepted => BC_RESRuntime
  | REK_ConnectBlockAccepted => BC_ConnectBlock
  | REK_ReorgReplay => BC_Reorg
  | REK_MultinodeConsensus => BC_Replay
  end.

Definition status_accepted (s : RuntimeStatus) : bool :=
  match s with
  | RST_Accepted | RST_Pass => true
  | RST_Rejected | RST_Unknown => false
  end.

(** Project the evidence value into an oracle result, per component. *)
Definition result_of_record (r : RuntimeEvidenceRecord) : OracleResult :=
  match r.(rer_kind) with
  | REK_DrawWinner => OR_Winner r.(rer_winner_txid)
  | REK_ResMintAccepted => OR_Accept (status_accepted r.(rer_status))
  | REK_ConnectBlockAccepted => OR_Accept (status_accepted r.(rer_status))
  | REK_ReorgReplay => OR_Accept (status_accepted r.(rer_status))
  | REK_MultinodeConsensus => OR_Accept (status_accepted r.(rer_status))
  end.

(** REQUIRED-1: runtime evidence -> OracleObservation. Frozen evidence is always
    PRESENT (it exists); its source is the runtime, never Coq. This is a runtime
    observation, NOT a proof of equivalence with the Coq trace. *)
Definition evidence_to_observation (r : RuntimeEvidenceRecord) : OracleObservation :=
  {| oo_component := component_of_kind r.(rer_kind);
     oo_source := OS_Runtime;
     oo_present := true;
     oo_result := result_of_record r |}.

(** Every produced observation is present and runtime-sourced. *)
Lemma evidence_observation_present :
  forall r, (evidence_to_observation r).(oo_present) = true.
Proof. intro r. reflexivity. Qed.

Lemma evidence_observation_runtime_source :
  forall r, (evidence_to_observation r).(oo_source) = OS_Runtime.
Proof. intro r. reflexivity. Qed.

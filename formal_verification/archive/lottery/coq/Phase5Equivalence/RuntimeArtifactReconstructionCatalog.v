(** Phase5Equivalence/RuntimeArtifactReconstructionCatalog.v
    Runtime artifact reconstruction feasibility (TASK-018GG-H).

    For Draw / Replay / Reorg / ConnectBlock, classifies whether the frozen
    Runtime evidence artifact contains enough information to reconstruct a
    [CanonicalStateHash] (018GG-C: [state_hash] over [encode_*_bytes]).

    HONESTY BOUNDARY (CRITICAL):
    - DOES NOT assert Coq = Runtime equivalence.
    - DOES NOT modify consensus / runtime / replay / reorg / connectblock code.
    - No SHA-256 / external hash kernel introduced.
    - Runtime hash values are metadata only; never used as equality proofs.
    - PD-4F-OB-02 / PD-4F-CPP-01 remain OPEN. *)

From Coq Require Import List ZArith Bool.
Import ListNotations.
Require Import Rigor.Phase4FConsensus.RuntimeEvidenceBridge.
Require Import Rigor.Phase4FConsensus.RuntimeEvidenceCatalog.
Require Import Rigor.Phase5Equivalence.EquivalenceFramework.
Require Import Rigor.Phase5Equivalence.CanonicalStateEncoding.
Require Import Rigor.Phase5Equivalence.CanonicalBytesProjection.
Require Import Rigor.Phase5Equivalence.CanonicalStateHashModel.
Require Import Rigor.Phase5Equivalence.CanonicalHashAlignmentBridge.

(** ================================================================
    Part A:  Reconstruction status vocabulary
    ================================================================ *)

Inductive ReconstructionStatus :=
| RCS_Reconstructable
| RCS_PartiallyReconstructable
| RCS_NotReconstructable.

Definition rcs_reconstructable (s : ReconstructionStatus) : bool :=
  match s with RCS_Reconstructable => true | _ => false end.
Definition rcs_partial (s : ReconstructionStatus) : bool :=
  match s with RCS_PartiallyReconstructable => true | _ => false end.
Definition rcs_not_reconstructable (s : ReconstructionStatus) : bool :=
  match s with RCS_NotReconstructable => true | _ => false end.

(** Canonical encoding field tags required by 018GG-A (for audit listing). *)
Inductive CanonicalFieldTag :=
| CFT_TraceHeight
| CFT_TransitionCount
| CFT_AcceptCount
| CFT_RollbackDepth
| CFT_LastBlockId
| CFT_ConsensusTrace
| CFT_DrawCanonicalEncoder
| CFT_Sha256Kernel.

(** ================================================================
    Part B:  Per-component reconstruction record
    ================================================================ *)

Record RuntimeReconstructionEntry := {
  rre_component : Component;
  rre_runtime_artifact_available : bool;
  rre_runtime_task_id : nat;
  rre_runtime_height : Z;           (* height field from evidence, if any *)
  rre_fields_required : list CanonicalFieldTag;
  rre_fields_missing : list CanonicalFieldTag;
  rre_status : ReconstructionStatus;
}.

Definition mk_reconstruction_entry
  (c : Component) (avail : bool) (tid : nat) (h : Z)
  (req miss : list CanonicalFieldTag) (st : ReconstructionStatus)
  : RuntimeReconstructionEntry :=
  {| rre_component := c;
     rre_runtime_artifact_available := avail;
     rre_runtime_task_id := tid;
     rre_runtime_height := h;
     rre_fields_required := req;
     rre_fields_missing := miss;
     rre_status := st |}.

(** Fields required to build [CanonicalStateHash] for Replay / ConnectBlock. *)
Definition replay_cb_fields_required : list CanonicalFieldTag :=
  [ CFT_TraceHeight; CFT_TransitionCount; CFT_AcceptCount;
    CFT_LastBlockId; CFT_ConsensusTrace; CFT_Sha256Kernel ].

(** Reorg additionally needs rollback depth. *)
Definition reorg_fields_required : list CanonicalFieldTag :=
  CFT_RollbackDepth :: replay_cb_fields_required.

(** Draw: 018GG-A/B/C define no [hash_*] for Draw; encoder path absent. *)
Definition draw_fields_required : list CanonicalFieldTag :=
  [ CFT_DrawCanonicalEncoder; CFT_ConsensusTrace; CFT_TraceHeight;
    CFT_TransitionCount; CFT_AcceptCount; CFT_LastBlockId; CFT_Sha256Kernel ].

(** ================================================================
    Part C:  The four-component catalog
    ================================================================ *)

(** Draw — TASK-003 winner evidence present, but no canonical Draw encoder /
    hash path exists in 018GG-A/B/C; runtime [winner_txid] is a 256-bit domain
    value, not a trace-derived [CanonicalStateHash]. *)
Definition draw_reconstruction_entry : RuntimeReconstructionEntry :=
  mk_reconstruction_entry
    C_Draw true 3 (rer_height ev_003_winner)
    draw_fields_required
    draw_fields_required
    RCS_NotReconstructable.

(** Replay — TASK-005 multinode: height present; trace / counts / block-id /
    SHA kernel absent from the artifact. *)
Definition replay_reconstruction_entry : RuntimeReconstructionEntry :=
  mk_reconstruction_entry
    C_Replay true 5 (rer_height ev_005_multinode)
    replay_cb_fields_required
    [ CFT_ConsensusTrace; CFT_TransitionCount; CFT_AcceptCount;
      CFT_LastBlockId; CFT_Sha256Kernel ]
    RCS_PartiallyReconstructable.

(** Reorg — TASK-005 reorg: height present; rollback depth and trace absent. *)
Definition reorg_reconstruction_entry : RuntimeReconstructionEntry :=
  mk_reconstruction_entry
    C_Reorg true 5 (rer_height ev_005_reorg)
    reorg_fields_required
    [ CFT_ConsensusTrace; CFT_TransitionCount; CFT_AcceptCount;
      CFT_RollbackDepth; CFT_LastBlockId; CFT_Sha256Kernel ]
    RCS_PartiallyReconstructable.

(** ConnectBlock — TASK-005 connect: height present; trace fields absent. *)
Definition connectblock_reconstruction_entry : RuntimeReconstructionEntry :=
  mk_reconstruction_entry
    C_ConnectBlock true 5 (rer_height ev_005_connect)
    replay_cb_fields_required
    [ CFT_ConsensusTrace; CFT_TransitionCount; CFT_AcceptCount;
      CFT_LastBlockId; CFT_Sha256Kernel ]
    RCS_PartiallyReconstructable.

Definition runtime_reconstruction_catalog : list RuntimeReconstructionEntry :=
  [ draw_reconstruction_entry;
    replay_reconstruction_entry;
    reorg_reconstruction_entry;
    connectblock_reconstruction_entry ].

Definition runtime_reconstruction_count : nat :=
  length runtime_reconstruction_catalog.

(** ================================================================
    Part D:  Counts
    ================================================================ *)

Definition reconstruction_statuses : list ReconstructionStatus :=
  map rre_status runtime_reconstruction_catalog.

Definition reconstructable_count : nat :=
  length (filter rcs_reconstructable reconstruction_statuses).
Definition partial_count : nat :=
  length (filter rcs_partial reconstruction_statuses).
Definition not_reconstructable_count : nat :=
  length (filter rcs_not_reconstructable reconstruction_statuses).

(** ================================================================
    Part E:  Required proofs
    ================================================================ *)

Lemma runtime_reconstruction_catalog_nonempty :
  runtime_reconstruction_catalog <> nil.
Proof. discriminate. Qed.

Lemma runtime_reconstruction_total_entries :
  runtime_reconstruction_count = 4%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma reconstructable_count_value : reconstructable_count = 0%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma partial_count_value : partial_count = 3%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma not_reconstructable_count_value : not_reconstructable_count = 1%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma runtime_reconstruction_partition_complete :
  (reconstructable_count + partial_count + not_reconstructable_count)%nat
  = runtime_reconstruction_count.
Proof. vm_compute. reflexivity. Qed.

(** Every entry has a concrete status (no fourth constructor). *)
Lemma runtime_reconstruction_status_defined :
  forall e, In e runtime_reconstruction_catalog ->
    match e.(rre_status) with
    | RCS_Reconstructable => True
    | RCS_PartiallyReconstructable => True
    | RCS_NotReconstructable => True
    end.
Proof.
  intros e Hin.
  repeat (destruct Hin as [<- | Hin]; [exact I | ]).
  destruct Hin.
Qed.

(** Draw is explicitly not reconstructable. *)
Lemma draw_not_reconstructable :
  draw_reconstruction_entry.(rre_status) = RCS_NotReconstructable.
Proof. reflexivity. Qed.

(** Replay / Reorg / ConnectBlock are partially reconstructable only. *)
Lemma replay_reorg_cb_partially_reconstructable :
  replay_reconstruction_entry.(rre_status) = RCS_PartiallyReconstructable /\
  reorg_reconstruction_entry.(rre_status) = RCS_PartiallyReconstructable /\
  connectblock_reconstruction_entry.(rre_status) = RCS_PartiallyReconstructable.
Proof. repeat split; reflexivity. Qed.

(** All four runtime artifacts are marked available in the catalog. *)
Lemma all_runtime_artifacts_available :
  forall e, In e runtime_reconstruction_catalog ->
    e.(rre_runtime_artifact_available) = true.
Proof.
  intros e Hin.
  repeat (destruct Hin as [<- | Hin]; [reflexivity | ]).
  destruct Hin.
Qed.

(** ================================================================
    Part F:  Honesty / debt re-exports
    ================================================================ *)

Definition runtime_reconstruction_layer_established : bool := true.
Definition runtime_reconstruction_coq_eq_runtime : bool := false.

Lemma runtime_reconstruction_layer_established_value :
  runtime_reconstruction_layer_established = true.
Proof. reflexivity. Qed.

Lemma runtime_reconstruction_does_not_establish_equivalence :
  runtime_reconstruction_coq_eq_runtime = false /\
  equivalence_claimed = false.
Proof. split; reflexivity. Qed.

Lemma runtime_reconstruction_preserves_open_proof_debt :
  negb proof_debt_placeholder.(pdp_pd_4f_ob_02_closed) = true /\
  negb proof_debt_placeholder.(pdp_pd_4f_cpp_01_closed) = true.
Proof.
  split.
  - exact canonical_encoding_pd_ob_02_open.
  - exact canonical_encoding_pd_cpp_01_open.
Qed.

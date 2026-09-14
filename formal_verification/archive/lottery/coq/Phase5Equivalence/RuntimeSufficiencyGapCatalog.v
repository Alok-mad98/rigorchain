(** Phase5Equivalence/RuntimeSufficiencyGapCatalog.v
    Runtime sufficiency gap analysis (TASK-018GG-I).

    Classifies, per Draw / Replay / Reorg / ConnectBlock, which canonical
    fields are required, which runtime fields are actually present in frozen
    evidence, and which [GapKind] entries remain. Assigns a [GapClosureStatus]
    indicating whether gaps could theoretically close in a future proof.

    HONESTY BOUNDARY (CRITICAL):
    - DOES NOT assert Coq = Runtime equivalence.
    - No SHA-256 / external hash kernel introduced.
    - No consensus / runtime / replay / reorg / connectblock code modified.
    - PD-4F-OB-02 / PD-4F-CPP-01 remain OPEN. *)

From Coq Require Import List ZArith Bool.
Import ListNotations.
Require Import Rigor.Phase4FConsensus.RuntimeEvidenceBridge.
Require Import Rigor.Phase4FConsensus.RuntimeEvidenceCatalog.
Require Import Rigor.Phase5Equivalence.EquivalenceFramework.
Require Import Rigor.Phase5Equivalence.CanonicalStateEncoding.
Require Import Rigor.Phase5Equivalence.CanonicalHashAlignmentBridge.
Require Import Rigor.Phase5Equivalence.RuntimeArtifactReconstructionCatalog.

(** ================================================================
    Part A:  Gap vocabulary
    ================================================================ *)

Inductive GapKind :=
| GK_MissingTransitionCount
| GK_MissingAcceptCount
| GK_MissingRollbackDepth
| GK_MissingLastBlockId
| GK_MissingTraceHistory
| GK_MissingHashKernel.

Definition gap_kind_count : nat := 6%nat.

Lemma gap_kind_count_value : gap_kind_count = 6%nat.
Proof. reflexivity. Qed.

(** Whether a gap could close in principle with richer runtime evidence. *)
Inductive GapClosureStatus :=
| GCS_Closable
| GCS_PotentiallyClosable
| GCS_NonClosable.

Definition gcs_closable (s : GapClosureStatus) : bool :=
  match s with GCS_Closable => true | _ => false end.
Definition gcs_potentially (s : GapClosureStatus) : bool :=
  match s with GCS_PotentiallyClosable => true | _ => false end.
Definition gcs_nonclosable (s : GapClosureStatus) : bool :=
  match s with GCS_NonClosable => true | _ => false end.

(** Runtime field tags present in frozen evidence (metadata only). *)
Inductive RuntimeFieldTag :=
| RTF_Height
| RTF_BlockHash256
| RTF_WinnerTxid.

(** ================================================================
    Part B:  RuntimeGapRecord
    ================================================================ *)

Record RuntimeGapRecord := {
  rgr_component : Component;
  rgr_fields_required : list CanonicalFieldTag;
  rgr_runtime_available : list RuntimeFieldTag;
  rgr_gaps : list GapKind;
  rgr_closure_status : GapClosureStatus;
  rgr_coq_eq_runtime_theoretically_reachable : bool;
}.

Definition mk_gap_record
  (c : Component) (req : list CanonicalFieldTag)
  (avail : list RuntimeFieldTag) (gaps : list GapKind)
  (closure : GapClosureStatus) (reachable : bool)
  : RuntimeGapRecord :=
  {| rgr_component := c;
     rgr_fields_required := req;
     rgr_runtime_available := avail;
     rgr_gaps := gaps;
     rgr_closure_status := closure;
     rgr_coq_eq_runtime_theoretically_reachable := reachable |}.

(** Standard gap sets (aligned with 018GG-H missing-field analysis). *)
Definition replay_cb_gaps : list GapKind :=
  [ GK_MissingTraceHistory;
    GK_MissingTransitionCount;
    GK_MissingAcceptCount;
    GK_MissingLastBlockId;
    GK_MissingHashKernel ].

Definition reorg_gaps : list GapKind :=
  GK_MissingRollbackDepth :: replay_cb_gaps.

Definition draw_gaps : list GapKind :=
  [ GK_MissingTraceHistory;
    GK_MissingTransitionCount;
    GK_MissingAcceptCount;
    GK_MissingLastBlockId;
    GK_MissingHashKernel ].

(** ================================================================
    Part C:  Per-component gap records
    ================================================================ *)

(** Draw — encoder path absent in 018GG-A/B/C; gaps are non-closable from
    current runtime evidence shape (winner_txid is not canonical state). *)
Definition draw_gap_record : RuntimeGapRecord :=
  mk_gap_record
    C_Draw draw_fields_required
    [ RTF_Height; RTF_WinnerTxid; RTF_BlockHash256 ]
    draw_gaps
    GCS_NonClosable
    false.

(** Replay — height + block_hash available; trace-derived fields missing but
    could theoretically be supplied by richer runtime exports. *)
Definition replay_gap_record : RuntimeGapRecord :=
  mk_gap_record
    C_Replay replay_cb_fields_required
    [ RTF_Height; RTF_BlockHash256 ]
    replay_cb_gaps
    GCS_PotentiallyClosable
    false.

(** Reorg — same as Replay plus rollback depth gap. *)
Definition reorg_gap_record : RuntimeGapRecord :=
  mk_gap_record
    C_Reorg reorg_fields_required
    [ RTF_Height; RTF_BlockHash256 ]
    reorg_gaps
    GCS_PotentiallyClosable
    false.

(** ConnectBlock — height + block_hash available; trace fields missing. *)
Definition connectblock_gap_record : RuntimeGapRecord :=
  mk_gap_record
    C_ConnectBlock replay_cb_fields_required
    [ RTF_Height; RTF_BlockHash256 ]
    replay_cb_gaps
    GCS_PotentiallyClosable
    false.

Definition runtime_gap_catalog : list RuntimeGapRecord :=
  [ draw_gap_record;
    replay_gap_record;
    reorg_gap_record;
    connectblock_gap_record ].

Definition runtime_gap_catalog_count : nat := length runtime_gap_catalog.

(** ================================================================
    Part D:  Aggregate gap counts
    ================================================================ *)

Definition gap_count_draw : nat := length draw_gaps.
Definition gap_count_replay : nat := length replay_cb_gaps.
Definition gap_count_reorg : nat := length reorg_gaps.
Definition gap_count_connectblock : nat := length replay_cb_gaps.

Definition total_gap_count : nat :=
  (gap_count_draw + gap_count_replay + gap_count_reorg
   + gap_count_connectblock)%nat.

Lemma gap_count_draw_value : gap_count_draw = 5%nat.
Proof. reflexivity. Qed.

Lemma gap_count_replay_value : gap_count_replay = 5%nat.
Proof. reflexivity. Qed.

Lemma gap_count_reorg_value : gap_count_reorg = 6%nat.
Proof. reflexivity. Qed.

Lemma gap_count_connectblock_value : gap_count_connectblock = 5%nat.
Proof. reflexivity. Qed.

Lemma total_gap_count_value : total_gap_count = 21%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma gap_count_per_component_sum :
  (gap_count_draw + gap_count_replay + gap_count_reorg
   + gap_count_connectblock)%nat = total_gap_count.
Proof. vm_compute. reflexivity. Qed.

(** ================================================================
    Part E:  Closure-status component counts
    ================================================================ *)

Definition closure_statuses : list GapClosureStatus :=
  map rgr_closure_status runtime_gap_catalog.

Definition components_with_closable_gap : nat :=
  length (filter gcs_closable closure_statuses).
Definition components_with_potentially_closable_gap : nat :=
  length (filter gcs_potentially closure_statuses).
Definition components_with_nonclosable_gap : nat :=
  length (filter gcs_nonclosable closure_statuses).

Lemma components_with_closable_gap_value :
  components_with_closable_gap = 0%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma components_with_potentially_closable_gap_value :
  components_with_potentially_closable_gap = 3%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma components_with_nonclosable_gap_value :
  components_with_nonclosable_gap = 1%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma runtime_gap_closure_partition_complete :
  (components_with_closable_gap + components_with_potentially_closable_gap
   + components_with_nonclosable_gap)%nat
  = runtime_gap_catalog_count.
Proof. vm_compute. reflexivity. Qed.

(** No component is marked theoretically reachable for Coq = Runtime yet. *)
Lemma all_coq_eq_runtime_not_reachable :
  forall r, In r runtime_gap_catalog ->
    r.(rgr_coq_eq_runtime_theoretically_reachable) = false.
Proof.
  intros r Hin.
  repeat (destruct Hin as [<- | Hin]; [reflexivity | ]).
  destruct Hin.
Qed.

(** ================================================================
    Part F:  Required proofs
    ================================================================ *)

Lemma runtime_gap_catalog_nonempty :
  runtime_gap_catalog <> nil.
Proof. discriminate. Qed.

Lemma runtime_gap_partition_complete :
  (gap_count_draw + gap_count_replay + gap_count_reorg
   + gap_count_connectblock)%nat = total_gap_count /\
  (components_with_closable_gap + components_with_potentially_closable_gap
   + components_with_nonclosable_gap)%nat = runtime_gap_catalog_count.
Proof. split; [exact gap_count_per_component_sum | ].
       exact runtime_gap_closure_partition_complete.
Qed.

Lemma runtime_gap_status_defined :
  forall r, In r runtime_gap_catalog ->
    match r.(rgr_closure_status) with
    | GCS_Closable => True
    | GCS_PotentiallyClosable => True
    | GCS_NonClosable => True
    end.
Proof.
  intros r Hin.
  repeat (destruct Hin as [<- | Hin]; [exact I | ]).
  destruct Hin.
Qed.

(** Draw is non-closable; Replay / Reorg / ConnectBlock are potentially closable. *)
Lemma draw_gap_nonclosable :
  draw_gap_record.(rgr_closure_status) = GCS_NonClosable.
Proof. reflexivity. Qed.

Lemma replay_reorg_cb_potentially_closable :
  replay_gap_record.(rgr_closure_status) = GCS_PotentiallyClosable /\
  reorg_gap_record.(rgr_closure_status) = GCS_PotentiallyClosable /\
  connectblock_gap_record.(rgr_closure_status) = GCS_PotentiallyClosable.
Proof. repeat split; reflexivity. Qed.

(** ================================================================
    Part G:  Honesty / debt re-exports
    ================================================================ *)

Definition runtime_sufficiency_layer_established : bool := true.
Definition runtime_gap_coq_eq_runtime : bool := false.

Lemma runtime_sufficiency_layer_established_value :
  runtime_sufficiency_layer_established = true.
Proof. reflexivity. Qed.

Lemma runtime_gap_does_not_establish_equivalence :
  runtime_gap_coq_eq_runtime = false /\ equivalence_claimed = false.
Proof. split; reflexivity. Qed.

Lemma runtime_gap_preserves_open_proof_debt :
  negb proof_debt_placeholder.(pdp_pd_4f_ob_02_closed) = true /\
  negb proof_debt_placeholder.(pdp_pd_4f_cpp_01_closed) = true.
Proof.
  split.
  - exact canonical_encoding_pd_ob_02_open.
  - exact canonical_encoding_pd_cpp_01_open.
Qed.

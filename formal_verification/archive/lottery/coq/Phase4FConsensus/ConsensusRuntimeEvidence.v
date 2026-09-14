(** Phase4FConsensus/ConsensusRuntimeEvidence.v
    Runtime evidence catalog (PHASE-4F-001F).

    Enumerates the frozen-runtime evidence sources the Oracle Bridge may cite.
    This is a CATALOG ONLY: no execution, no runtime connection, no consensus or
    kernel import. Every entry is marked [ee_imported = true] because the values
    originate outside Phase-4F and are consumed as evidence, not re-derived. *)

From Coq Require Import List.
Import ListNotations.

(** REQUIRED-2: phased runtime evidence sources. *)
Inductive EvidenceSource :=
  | ES_PHASE4A
  | ES_PHASE4B
  | ES_PHASE4C
  | ES_PHASE4D
  | ES_PHASE4E.

(** REQUIRED-2: runtime RES (reserve) evidence tasks. *)
Inductive ResEvidenceTask :=
  | RES_TASK003
  | RES_TASK004
  | RES_TASK005.

(** Kind of consensus behaviour the evidence speaks to. *)
Inductive EvidenceKind :=
  | EK_Draw
  | EK_Replay
  | EK_Reorg
  | EK_Supply
  | EK_ConnectBlock
  | EK_RES.

(** Verification standing of a piece of evidence inside Phase-4F. *)
Inductive EvidenceStanding :=
  | EV_IndependentlyChecked   (* recomputed inside Phase-4F (e.g. draw selection) *)
  | EV_ImportedOnly           (* consumed as-is; not re-derived in Phase-4F *)
  | EV_Blocked.               (* not modeled at all *)

Record EvidenceEntry := {
  ee_source : EvidenceSource;
  ee_kind : EvidenceKind;
  ee_imported : bool;
  ee_standing : EvidenceStanding;
}.

(** REQUIRED-2: phased evidence catalog. Standing reflects 001E findings:
    only the draw *selection* layer is independently re-checked; everything else
    is imported or blocked. *)
Definition phase_evidence_catalog : list EvidenceEntry :=
  {| ee_source := ES_PHASE4A; ee_kind := EK_Draw;
     ee_imported := true; ee_standing := EV_IndependentlyChecked |}
  ::
  {| ee_source := ES_PHASE4B; ee_kind := EK_Replay;
     ee_imported := true; ee_standing := EV_ImportedOnly |}
  ::
  {| ee_source := ES_PHASE4B; ee_kind := EK_Reorg;
     ee_imported := true; ee_standing := EV_ImportedOnly |}
  ::
  {| ee_source := ES_PHASE4C; ee_kind := EK_RES;
     ee_imported := true; ee_standing := EV_ImportedOnly |}
  ::
  {| ee_source := ES_PHASE4D; ee_kind := EK_Supply;
     ee_imported := true; ee_standing := EV_ImportedOnly |}
  ::
  {| ee_source := ES_PHASE4E; ee_kind := EK_ConnectBlock;
     ee_imported := true; ee_standing := EV_ImportedOnly |}
  :: nil.

Record ResEvidenceEntry := {
  re_task : ResEvidenceTask;
  re_imported : bool;
  re_standing : EvidenceStanding;
}.

(** REQUIRED-2: runtime RES evidence catalog (TASK-003/004/005). *)
Definition res_evidence_catalog : list ResEvidenceEntry :=
  {| re_task := RES_TASK003; re_imported := true; re_standing := EV_ImportedOnly |}
  ::
  {| re_task := RES_TASK004; re_imported := true; re_standing := EV_ImportedOnly |}
  ::
  {| re_task := RES_TASK005; re_imported := true; re_standing := EV_ImportedOnly |}
  :: nil.

Definition phase_evidence_count : nat := length phase_evidence_catalog.
Definition res_evidence_count : nat := length res_evidence_catalog.

(** Catalog facts (no execution; counts only). *)
Lemma phase_evidence_count_value : phase_evidence_count = 6%nat.
Proof. reflexivity. Qed.

Lemma res_evidence_count_value : res_evidence_count = 3%nat.
Proof. reflexivity. Qed.

(** Every catalogued entry is imported (consumed, not re-derived in Phase-4F). *)
Lemma all_phase_evidence_imported :
  forallb ee_imported phase_evidence_catalog = true.
Proof. reflexivity. Qed.

Lemma all_res_evidence_imported :
  forallb re_imported res_evidence_catalog = true.
Proof. reflexivity. Qed.

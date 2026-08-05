(** Phase4GRefinement/Phase4FEndToEndReadiness.v
    Final PHASE-4F / Phase-4G end-to-end readiness aggregation (PHASE-4F-008V).

    Aggregates, per component (Draw / Replay / Reorg / ConnectBlock / Supply):
      - the 004Q comparison verdicts vs Frozen C++ (002O) and Live Runtime (003P)
      - the 007U refinement projection status
    into one readiness record, and tallies agreement / disagreement / missing and
    defined / partial / undefined projections.

    AUDIT / SPEC ONLY. Does NOT claim Coq = Runtime / Coq = Frozen C++ / Coq =
    Consensus. No consensus / runtime / RFC / SHA256 / ConvergeRoot / ConnectBlock /
    Replay / Reorg code is modified. *)

From Coq Require Import List ZArith.
Import ListNotations.
Require Import Rigor.Phase4FConsensus.ConsensusOracleBridge.
Require Import Rigor.Phase4FConsensus.FrozenCppAgreement.
Require Import Rigor.Phase4FConsensus.Phase4FEndToEndComparison.
Require Import Rigor.Phase4GRefinement.RefinementProjection.

(** REQUIRED-1: one readiness record per component. *)
Record ReadinessRecord := {
  rr_component : BridgeComponent;
  rr_vs_frozen : FrozenAgreementResult;
  rr_vs_runtime : FrozenAgreementResult;
  rr_projection : ProjectionStatus;
}.

Definition mk_readiness (s : E2EComponentSummary) : ReadinessRecord :=
  {| rr_component := s.(e2e_component);
     rr_vs_frozen := s.(e2e_vs_frozen);
     rr_vs_runtime := s.(e2e_vs_runtime);
     rr_projection := projection_status_of s.(e2e_component) |}.

(** Built from the 004Q per-component summary (5 components). *)
Definition readiness_records : list ReadinessRecord :=
  map mk_readiness per_component_summary.

(** All comparison verdicts (frozen + runtime) across all components. *)
Definition all_verdicts : list FrozenAgreementResult :=
  flat_map (fun r => [ r.(rr_vs_frozen); r.(rr_vs_runtime) ]) readiness_records.

Definition agreement_total : nat := length (filter far_is_agree all_verdicts).
Definition disagreement_total : nat := length (filter far_is_disagree all_verdicts).
Definition missing_total : nat := length (filter far_is_missing all_verdicts).

(** Per-component projection tallies (5 components). *)
Definition defined_total : nat :=
  length (filter (fun r => status_eqb r.(rr_projection) PS_Defined) readiness_records).
Definition partial_total : nat :=
  length (filter (fun r => status_eqb r.(rr_projection) PS_Partial) readiness_records).
Definition undefined_total : nat :=
  length (filter (fun r => status_eqb r.(rr_projection) PS_Undefined) readiness_records).

Lemma agreement_total_value : agreement_total = 0%nat.
Proof. vm_compute. reflexivity. Qed.
Lemma disagreement_total_value : disagreement_total = 8%nat.
Proof. vm_compute. reflexivity. Qed.
Lemma missing_total_value : missing_total = 2%nat.
Proof. vm_compute. reflexivity. Qed.
Lemma defined_total_value : defined_total = 1%nat.
Proof. vm_compute. reflexivity. Qed.
Lemma partial_total_value : partial_total = 3%nat.
Proof. vm_compute. reflexivity. Qed.
Lemma undefined_total_value : undefined_total = 1%nat.
Proof. vm_compute. reflexivity. Qed.

(** REQUIRED-1: the verdict tallies partition all verdicts (2 per component). *)
Lemma readiness_counts_consistent :
  agreement_total + disagreement_total + missing_total = length all_verdicts.
Proof. vm_compute. reflexivity. Qed.

(** REQUIRED-1: the readiness records cover exactly the 004Q components, and the
    projection tallies partition them. *)
Lemma total_entries_match :
  length readiness_records = length per_component_summary.
Proof. unfold readiness_records. rewrite map_length. reflexivity. Qed.

Lemma readiness_projection_partition :
  defined_total + partial_total + undefined_total = length readiness_records.
Proof. vm_compute. reflexivity. Qed.

(** Supply is aggregated as intentionally undefined. *)
Lemma supply_readiness_undefined :
  projection_status_of BC_Supply = PS_Undefined.
Proof. reflexivity. Qed.

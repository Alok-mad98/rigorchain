(** Phase4FConsensus/FrozenCppTraceAudit.v
    Coq-trace vs Frozen-C++ comparison statistics (PHASE-4F-001L).

    Instantiates the comparison over the per-component mapping and proves the
    statistics. The agreements are frozen-vs-runtime import cross-consistency; the
    lone Coq-trace-vs-frozen pair (Draw) disagrees. This does NOT establish
    Coq = Frozen C++ (PD-4F-CPP-01 / PD-4F-OB-02). AUDIT only. *)

From Coq Require Import List ZArith.
Import ListNotations.
Require Import Rigor.Phase4FConsensus.FrozenCppAgreement.
Require Import Rigor.Phase4FConsensus.FrozenCppTraceComparison.
Require Import Rigor.Phase4FConsensus.FrozenCppTraceCatalog.
Require Import Rigor.Phase4FConsensus.LiveRuntimeTraceIntegration.

Definition frozen_trace_agreement_count : nat :=
  trace_agreement_count frozen_trace_pairs.
Definition frozen_trace_disagreement_count : nat :=
  trace_disagreement_count frozen_trace_pairs.
Definition frozen_trace_missing_count : nat :=
  trace_missing_count frozen_trace_pairs.

(** Honest statistics: 3 agree (runtime cross-consistency), 1 disagree (draw
    domain), 1 missing (supply). *)
Lemma frozen_trace_agreement_count_value : frozen_trace_agreement_count = 3%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma frozen_trace_disagreement_count_value : frozen_trace_disagreement_count = 1%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma frozen_trace_missing_count_value : frozen_trace_missing_count = 1%nat.
Proof. vm_compute. reflexivity. Qed.

(** REQUIRED-3: mapping catalog is non-empty. *)
Lemma catalog_nonempty : frozen_trace_pairs <> nil.
Proof. unfold frozen_trace_pairs. discriminate. Qed.

(** REQUIRED-3: statistics are total (partition the catalog). *)
Lemma agreement_statistics_exist :
  (frozen_trace_agreement_count + frozen_trace_disagreement_count
     + frozen_trace_missing_count)%nat = length frozen_trace_pairs.
Proof. vm_compute. reflexivity. Qed.

(** 001M cross-check counts: live-projected Coq trace vs frozen C++.
    These agreements are projection identities (import cross-consistency), NOT
    Coq-vs-C++ equivalence. Draw/Replay/Reorg/ConnectBlock agree (4); Supply
    missing (1). *)
Lemma live_cross_agreement_count_value : live_agreement_count = 4%nat.
Proof. exact live_agreement_count_value. Qed.

Lemma live_cross_disagreement_count_value : live_disagreement_count = 0%nat.
Proof. exact live_disagreement_count_value. Qed.

Lemma live_cross_missing_count_value : live_missing_count = 1%nat.
Proof. exact live_missing_count_value. Qed.

Lemma live_cross_statistics_exist :
  (live_agreement_count + live_disagreement_count + live_missing_count)%nat
    = length live_frozen_pairs.
Proof. exact live_statistics_total. Qed.

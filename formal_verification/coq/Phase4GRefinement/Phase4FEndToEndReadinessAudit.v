(** Phase4GRefinement/Phase4FEndToEndReadinessAudit.v
    Final PHASE-4F / Phase-4G readiness audit (PHASE-4F-008V).

    Final consolidated totals:
      Agreement / Disagreement / Missing = 0 / 8 / 2  (10 verdicts, 5 comp x 2 sources)
      Defined / Partial / Undefined projections (per component) = 1 / 3 / 1
        (gap-derived catalog view, 8 records: 2 / 6 / 0 — see Phase4GRefinementAudit)

    *** Remaining gaps / circular dependencies / open proof debt ***
    - PD-4F-OB-02  (Coq = runtime/frozen) ............ OPEN
    - PD-4F-CPP-01 (Coq = frozen C++) ................ OPEN / PARTIAL
    - PD-4F-OB-03  (eligibility / sigma imported) .... OPEN / PARTIAL
    - Payload-domain gap: Coq model emits indices/heights; runtime/frozen emit 256-bit
      hashes. Partial projections encode heights, NOT hashes.
    - Coq observations (001N) are independently derived (no projection identity), but
      this is NOT equivalence.

    *** Equivalence is NOT established: Coq != Runtime, Coq != Frozen C++,
    Coq != Consensus are not asserted; nor is the converse proven. ***

    SPEC / AUDIT ONLY. No consensus / runtime / RFC / SHA256 / ConvergeRoot /
    ConnectBlock / Replay / Reorg code is modified. *)

From Coq Require Import List ZArith.
Import ListNotations.
Require Import Rigor.Phase4FConsensus.ConsensusOracleBridge.
Require Import Rigor.Phase4GRefinement.RefinementProjection.
Require Import Rigor.Phase4GRefinement.Phase4FEndToEndReadiness.
Require Import Rigor.Phase4GRefinement.Phase4FEndToEndReadinessCatalog.

(** Final aggregate totals (re-exported from the readiness aggregation). *)
Definition final_agreement : nat := agreement_total.
Definition final_disagreement : nat := disagreement_total.
Definition final_missing : nat := missing_total.
Definition final_defined : nat := defined_total.
Definition final_partial : nat := partial_total.
Definition final_undefined : nat := undefined_total.

Lemma final_agreement_value : final_agreement = 0%nat.
Proof. exact agreement_total_value. Qed.
Lemma final_disagreement_value : final_disagreement = 8%nat.
Proof. exact disagreement_total_value. Qed.
Lemma final_missing_value : final_missing = 2%nat.
Proof. exact missing_total_value. Qed.
Lemma final_defined_value : final_defined = 1%nat.
Proof. exact defined_total_value. Qed.
Lemma final_partial_value : final_partial = 3%nat.
Proof. exact partial_total_value. Qed.
Lemma final_undefined_value : final_undefined = 1%nat.
Proof. exact undefined_total_value. Qed.

(** Verdict totals partition all comparison verdicts. *)
Lemma final_verdict_totals_consistent :
  final_agreement + final_disagreement + final_missing = length all_verdicts.
Proof. unfold final_agreement, final_disagreement, final_missing.
       exact readiness_counts_consistent. Qed.

(** Projection totals partition the per-component readiness records. *)
Lemma final_projection_totals_consistent :
  final_defined + final_partial + final_undefined = length readiness_records.
Proof. unfold final_defined, final_partial, final_undefined.
       exact readiness_projection_partition. Qed.

(** The readiness catalog is the source-expanded view (15 = 5 components x 3 sources). *)
Lemma readiness_catalog_total : readiness_catalog_count = 15%nat.
Proof. exact readiness_catalog_count_value. Qed.

(** Supply remains the only intentionally undefined component. *)
Lemma supply_undefined_in_readiness :
  projection_status_of BC_Supply = PS_Undefined.
Proof. exact supply_readiness_undefined. Qed.

(** End-to-end readiness is PARTIAL: there are disagreements (8) and missing (2),
    so NO all-agree equivalence holds. This lemma records that the audit does not
    witness equivalence. *)
Lemma readiness_is_partial_not_equivalent :
  final_agreement <> length all_verdicts.
Proof. rewrite final_agreement_value. vm_compute. discriminate. Qed.

(** Phase4FConsensus/ConsensusDrawAudit.v
    Independent draw re-computation audit (PHASE-4F-001E).

    Re-computes the winner selection from raw registry inputs (candidate
    weight + txid + eligibility) WITHOUT reading the imported winner value, then
    compares against the PHASE-4A2 imported winner. This removes the selection
    layer's circularity. Eligibility itself remains imported kernel evidence
    (sigma threshold needs the hash kernel; out of Phase-4F scope). AUDIT ONLY:
    no consensus / RFC / kernel / runtime code is touched. *)

From Coq Require Import List ZArith.
Import ListNotations.
Require Import Rigor.Phase4FConsensus.ConsensusGoldenTrace.
Require Import Rigor.Phase4FConsensus.ConsensusGoldenVectorsData.

(** A candidate [w,t] is strictly better than the running best [bw,bt] iff it has
    a higher weight, or equal weight and a higher txid (tie-break). *)
Definition cand_better (w t bw bt : Z) : bool :=
  orb (Z.ltb bw w) (andb (Z.eqb w bw) (Z.ltb bt t)).

Fixpoint recompute_aux (cands : list DrawCandidate) (i : nat)
  (bi bw bt : Z) : Z :=
  match cands with
  | [] => bi
  | c :: rest =>
      if dc_eligible c then
        if orb (Z.eqb bi (-1)) (cand_better (dc_weight c) (dc_txid c) bw bt)
        then recompute_aux rest (S i) (Z.of_nat i) (dc_weight c) (dc_txid c)
        else recompute_aux rest (S i) bi bw bt
      else recompute_aux rest (S i) bi bw bt
  end.

(** REQUIRED-1: independent winner recomputed from registry inputs only. *)
Definition recompute_winner (cands : list DrawCandidate) : Z :=
  recompute_aux cands 0 (-1) 0 0.

Definition IndependentWinnerSource (r : DrawAuditRow) : Z :=
  recompute_winner (dar_candidates r).

Definition ImportedWinnerSource (r : DrawAuditRow) : Z :=
  dar_imported_winner r.

(** REQUIRED-1: comparison result of the two winner sources. *)
Inductive WinnerComparisonResult :=
  | WCR_Agree
  | WCR_Disagree.

Definition compare_winner_sources (imported recomputed : Z) : WinnerComparisonResult :=
  if Z.eqb imported recomputed then WCR_Agree else WCR_Disagree.

Definition winner_sources_agree (r : DrawAuditRow) : bool :=
  Z.eqb (ImportedWinnerSource r) (IndependentWinnerSource r).

Definition winner_sources_disagree (r : DrawAuditRow) : bool :=
  negb (winner_sources_agree r).

Definition row_comparison (r : DrawAuditRow) : WinnerComparisonResult :=
  compare_winner_sources (ImportedWinnerSource r) (IndependentWinnerSource r).

(** REQUIRED-2: comparison over every draw-applicable audit row. *)
Definition audit_results : list WinnerComparisonResult :=
  map row_comparison draw_audit_rows.

Definition audit_match_count : nat :=
  length (filter winner_sources_agree draw_audit_rows).

Definition audit_mismatch_count : nat :=
  length (filter winner_sources_disagree draw_audit_rows).

Definition audit_mismatched_rows : list DrawAuditRow :=
  filter winner_sources_disagree draw_audit_rows.

(** 001E verified result: independent recomputation agrees with imported winner
    on all 14 draw-applicable rows. *)
Lemma audit_match_count_value : audit_match_count = 14%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma audit_mismatch_count_value : audit_mismatch_count = 0%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma audit_no_mismatched_rows : audit_mismatched_rows = nil.
Proof. vm_compute. reflexivity. Qed.

Lemma audit_rows_total : (audit_match_count + audit_mismatch_count)%nat = draw_audit_rows_count.
Proof. vm_compute. reflexivity. Qed.

(** All audit rows agree (circularity of the selection layer is closed). *)
Theorem draw_circularity_closed :
  forall r, In r draw_audit_rows -> winner_sources_agree r = true.
Proof.
  intros r Hin.
  revert Hin.
  vm_compute.
  intros Hin.
  repeat (destruct Hin as [Heq | Hin]; [subst r; reflexivity | ]).
  contradiction.
Qed.

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

(** A candidate [(s,w,t)] is strictly better than the running best [(bs,bw,bt)] iff its sigma
    cross-product is smaller, or the cross-products are equal and its txid is smaller.

    This is the protocol rule: Selection.v [sigma_product_lt] compares [sx * wy] against
    [sy * wx] and breaks ties by [bytes_ltb] on the txid, and the C++/Python side does the same
    via [winner_cross_multiply_less] and [txid_memcmp_less]. Note the direction — LOWER sigma and
    LOWER txid win. The previous version of this file ranked by HIGHER weight and HIGHER txid,
    which is a different relation entirely; see the [dc_sigma] note in ConsensusGoldenTrace.v.

    Cross-multiplication is what keeps this exact: comparing [s/w] as a rational would need
    division, and both products fit in [Z] here with no modular wraparound, matching the uint512
    intermediate the production code uses. *)
Definition cand_better (s w t bs bw bt : Z) : bool :=
  orb (Z.ltb (s * bw) (bs * w))
      (andb (Z.eqb (s * bw) (bs * w)) (Z.ltb t bt)).

Fixpoint recompute_aux (cands : list DrawCandidate) (i : nat)
  (bi bs bw bt : Z) : Z :=
  match cands with
  | [] => bi
  | c :: rest =>
      if dc_eligible c then
        if orb (Z.eqb bi (-1))
               (cand_better (dc_sigma c) (dc_weight c) (dc_txid c) bs bw bt)
        then recompute_aux rest (S i) (Z.of_nat i)
               (dc_sigma c) (dc_weight c) (dc_txid c)
        else recompute_aux rest (S i) bi bs bw bt
      else recompute_aux rest (S i) bi bs bw bt
  end.

(** REQUIRED-1: independent winner recomputed from registry inputs only. *)
Definition recompute_winner (cands : list DrawCandidate) : Z :=
  recompute_aux cands 0 (-1) 0 0 0.

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

(** 001E verified result: independent recomputation agrees with the imported winner on all 168
    draw-applicable rows (84 RunSelection vectors x {PHASE3_WIN, PHASE4A_DRAW}).

    This was 14 rows against the 57-vector baseline. The count moved because the generated data
    file had been frozen at that baseline while the vector set grew to 324; it is 168 and not
    something in between because every RunSelection vector contributes exactly two rows. *)
Lemma audit_match_count_value : audit_match_count = 168%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma audit_mismatch_count_value : audit_mismatch_count = 0%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma audit_no_mismatched_rows : audit_mismatched_rows = nil.
Proof. vm_compute. reflexivity. Qed.

Lemma audit_rows_total : (audit_match_count + audit_mismatch_count)%nat = draw_audit_rows_count.
Proof. vm_compute. reflexivity. Qed.

(** Pin the list length before freezing the table. Downstream files must use this
    rather than [vm_compute] on [draw_audit_rows], which would re-normalize every
    embedded 256-bit sigma. *)
Lemma draw_audit_rows_length : length draw_audit_rows = draw_audit_rows_count.
Proof. vm_compute. reflexivity. Qed.

(** All audit rows agree (circularity of the selection layer is closed).

    Derived from [audit_no_mismatched_rows] rather than by [vm_compute] over every
    In-hypothesis: the latter produced intractable proof terms once the row count
    moved from 14 to 168. The computational content lives entirely in
    [audit_no_mismatched_rows], which ran the sigma-cross-product rule against all
    168 rows; this theorem only packages that fact as a ∀-statement. *)
Theorem draw_circularity_closed :
  forall r, In r draw_audit_rows -> winner_sources_agree r = true.
Proof.
  intros r Hin.
  destruct (winner_sources_agree r) eqn:Ha; [reflexivity|].
  assert (Hm : In r audit_mismatched_rows).
  { unfold audit_mismatched_rows, winner_sources_disagree.
    apply filter_In. split; [exact Hin|].
    rewrite Ha. reflexivity. }
  rewrite audit_no_mismatched_rows in Hm.
  destruct Hm.
Qed.

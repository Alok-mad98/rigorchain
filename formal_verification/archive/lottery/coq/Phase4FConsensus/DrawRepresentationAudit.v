(** Phase4FConsensus/DrawRepresentationAudit.v
    Draw representation closure audit (PHASE-4F-001I).

    For every draw entry, projects BOTH the Coq-recomputed winner index and the
    imported winner index into the txid domain and checks they denote the SAME
    txid object. This closes the index-vs-txid representation gap WITHIN the draw
    domain. It does NOT connect to the live runtime block (different scenario) or
    to C++; those remain in PD-4F-OB-01b / PD-4F-OB-02. AUDIT only.

    Match counts are derived from [winner_projection_sound] (itself from
    [draw_circularity_closed]), not by a second [vm_compute] pass over the
    Opaque [draw_audit_rows] table. The computational evidence that the selection
    rule agrees with the golden winners lives in ConsensusDrawAudit; this file
    only lifts index-agreement to txid-agreement. *)

From Coq Require Import List ZArith.
Import ListNotations.
Require Import Rigor.Phase4FConsensus.ConsensusGoldenTrace.
Require Import Rigor.Phase4FConsensus.ConsensusDrawAudit.
Require Import Rigor.Phase4FConsensus.DrawRepresentationBridge.
Require Import Rigor.Phase4FConsensus.DrawRepresentationCatalog.

Definition entry_agreement (e : DrawReprEntry) : DrawRepresentationAgreement :=
  compare_representation e.(dre_candidates) e.(dre_coq_index) e.(dre_imported_index).

Definition entry_coq_txid (e : DrawReprEntry) : Z :=
  winner_index_to_txid e.(dre_candidates) e.(dre_coq_index).

Definition entry_runtime_txid (e : DrawReprEntry) : Z :=
  winner_txid_projection e.(dre_candidates) e.(dre_imported_index).

Definition representation_results : list DrawRepresentationAgreement :=
  map entry_agreement representation_catalog.

Definition is_repr_match (a : DrawRepresentationAgreement) : bool :=
  match a with DRA_Match => true | _ => false end.

Definition representation_match_count : nat :=
  length (filter is_repr_match representation_results).

Definition representation_mismatch_count : nat :=
  length (filter (fun a => negb (is_repr_match a)) representation_results).

(** REQUIRED-4: the Coq-side winner txid equals the imported (runtime-shaped)
    winner txid for every catalog entry — the representation closure.

    Derived from [draw_circularity_closed] + [equal_index_equal_txid]. *)
Theorem winner_projection_sound :
  forall e, In e representation_catalog ->
            entry_coq_txid e = entry_runtime_txid e.
Proof.
  intros e Hin.
  unfold representation_catalog in Hin.
  apply in_map_iff in Hin.
  destruct Hin as [r [Heq Hr]].
  subst e.
  unfold entry_coq_txid, entry_runtime_txid. simpl.
  unfold IndependentWinnerSource.
  apply equal_index_equal_txid.
  pose proof (draw_circularity_closed r Hr) as Hagree.
  unfold winner_sources_agree, IndependentWinnerSource, ImportedWinnerSource in Hagree.
  apply Z.eqb_eq in Hagree.
  symmetry. exact Hagree.
Qed.

Lemma entry_agreement_is_match :
  forall e, In e representation_catalog ->
            is_repr_match (entry_agreement e) = true.
Proof.
  intros e Hin.
  unfold is_repr_match, entry_agreement, compare_representation.
  pose proof (winner_projection_sound e Hin) as Heq.
  unfold entry_coq_txid, entry_runtime_txid in Heq.
  rewrite Heq. rewrite Z.eqb_refl. reflexivity.
Qed.

Lemma filter_length_when_all :
  forall {A : Type} (p : A -> bool) (l : list A),
    (forall x, In x l -> p x = true) ->
    length (filter p l) = length l.
Proof.
  induction l as [|a t IH]; intros H; simpl; [reflexivity|].
  assert (Ha : p a = true) by (apply H; left; reflexivity).
  rewrite Ha. simpl. f_equal.
  apply IH. intros x Hx. apply H. right. exact Hx.
Qed.

(** REQUIRED-4: all 168 draw rows close in the txid domain. *)
Lemma representation_match_count_value : representation_match_count = 168%nat.
Proof.
  unfold representation_match_count, representation_results.
  assert (Hall : forall a, In a (map entry_agreement representation_catalog) ->
                           is_repr_match a = true).
  { intros a Ha. apply in_map_iff in Ha.
    destruct Ha as [e [Heq Hin]]. subst a.
    apply entry_agreement_is_match. exact Hin. }
  rewrite (filter_length_when_all is_repr_match _ Hall).
  rewrite map_length.
  exact representation_catalog_count_value.
Qed.

Lemma representation_mismatch_count_value : representation_mismatch_count = 0%nat.
Proof.
  unfold representation_mismatch_count, representation_results.
  assert (Hall : forall a, In a (map entry_agreement representation_catalog) ->
                           is_repr_match a = true).
  { intros a Ha. apply in_map_iff in Ha.
    destruct Ha as [e [Heq Hin]]. subst a.
    apply entry_agreement_is_match. exact Hin. }
  assert (Hnone : filter (fun a => negb (is_repr_match a))
                         (map entry_agreement representation_catalog) = nil).
  { induction (map entry_agreement representation_catalog) as [|a t IH];
      [reflexivity|].
    simpl.
    assert (Ha : is_repr_match a = true).
    { apply Hall. left. reflexivity. }
    rewrite Ha. simpl.
    apply IH. intros x Hx. apply Hall. right. exact Hx. }
  rewrite Hnone. reflexivity.
Qed.

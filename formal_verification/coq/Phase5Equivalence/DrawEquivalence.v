(** Phase5Equivalence/DrawEquivalence.v
    Draw equivalence proofs (TASK-018FF-B).

    Proves the Draw projection equivalence chain:

      winner_index  --[winner_index_to_txid]-->  coq_projected_txid
            |                                          |
            |  (golden: coq_idx = imported_idx)        |  (= when indices agree)
            v                                          v
      imported_index  --[winner_txid_projection]-->  runtime_shaped_txid

    Also proves frozen TASK-003 draw txid = live-runtime draw txid.

    HONESTY BOUNDARY:
    - Golden-domain projection equivalence is fully discharged (14/14).
    - Frozen = live-runtime draw txid is discharged (import cross-check).
    - Coq golden projected txid = frozen/live-runtime TASK-003 txid is NOT claimed;
      no golden row carries that txid (proved below).
    - Supply / Replay / Reorg / ConnectBlock are untouched.
    - No fabricated equivalence; no consensus / runtime / RFC code modified. *)

From Coq Require Import List ZArith Bool.
Import ListNotations.
Require Import Rigor.Phase4FConsensus.ConsensusGoldenTrace.
Require Import Rigor.Phase4FConsensus.ConsensusDrawAudit.
Require Import Rigor.Phase4FConsensus.DrawRepresentationBridge.
Require Import Rigor.Phase4FConsensus.DrawRepresentationCatalog.
Require Import Rigor.Phase4FConsensus.DrawRepresentationAudit.
Require Import Rigor.Phase4GRefinement.DrawHashProjectionTheorem.
Require Import Rigor.Phase5Equivalence.EquivalenceFramework.

(** ================================================================
    Part A:  Draw equivalence result vocabulary
    ================================================================ *)

Inductive DrawEquivKind :=
| DEK_GoldenProjection
| DEK_FrozenRuntimeIdentity
| DEK_CrossDomainBridge.

Inductive DrawEquivResult :=
| DER_Resolved
| DER_Disagreement
| DER_Missing
| DER_Partial.

Definition draw_result_resolved (r : DrawEquivResult) : bool :=
  match r with
  | DER_Resolved => true
  | _ => false
  end.

Definition draw_result_disagreement (r : DrawEquivResult) : bool :=
  match r with
  | DER_Disagreement => true
  | _ => false
  end.

Definition draw_result_missing (r : DrawEquivResult) : bool :=
  match r with
  | DER_Missing => true
  | _ => false
  end.

Definition draw_result_partial (r : DrawEquivResult) : bool :=
  match r with
  | DER_Partial => true
  | _ => false
  end.

(** [winner_txid_projection] is definitionally the same function as
    [winner_index_to_txid] — an identity wrapper on indices. *)
Definition draw_projection_uses_identity_map : bool := true.

Lemma winner_txid_projection_is_identity_map :
  forall (cands : list DrawCandidate) (idx : Z),
    winner_txid_projection cands idx = winner_index_to_txid cands idx.
Proof. intros. reflexivity. Qed.

(** ================================================================
    Part B:  Golden-domain Coq index <-> runtime-shaped txid
    ================================================================ *)

Definition draw_coq_projected_txid (e : DrawReprEntry) : Z :=
  winner_index_to_txid e.(dre_candidates) e.(dre_coq_index).

Definition draw_runtime_shaped_txid (e : DrawReprEntry) : Z :=
  winner_txid_projection e.(dre_candidates) e.(dre_imported_index).

Definition draw_golden_projection_result (e : DrawReprEntry) : DrawEquivResult :=
  if Z.eqb (draw_coq_projected_txid e) (draw_runtime_shaped_txid e)
  then DER_Resolved else DER_Disagreement.

(** REQUIRED: Coq winner index projects to the same txid as the runtime-shaped
    projection for every golden catalog entry (re-exports 001I). *)
Theorem draw_coq_runtime_shaped_equivalence :
  forall e, In e representation_catalog ->
    draw_coq_projected_txid e = draw_runtime_shaped_txid e.
Proof.
  intros e Hin.
  unfold draw_coq_projected_txid, draw_runtime_shaped_txid.
  exact (winner_projection_sound e Hin).
Qed.

Lemma draw_golden_projection_all_resolved :
  forall e, In e representation_catalog ->
    draw_golden_projection_result e = DER_Resolved.
Proof.
  intros e Hin.
  unfold draw_golden_projection_result.
  pose proof (draw_coq_runtime_shaped_equivalence e Hin) as H.
  rewrite H. simpl. rewrite Z.eqb_refl. reflexivity.
Qed.

(** Index-level agreement (001E) implies txid agreement through one projection. *)
Theorem draw_index_txid_chain :
  forall (e : DrawReprEntry),
    In e representation_catalog ->
    recompute_winner e.(dre_candidates) = e.(dre_imported_index) ->
    winner_index_to_txid e.(dre_candidates) (recompute_winner e.(dre_candidates))
      = winner_index_to_txid e.(dre_candidates) e.(dre_imported_index).
Proof.
  intros e Hin Heq.
  rewrite Heq. reflexivity.
Qed.

(** ================================================================
    Part C:  Frozen C++ <-> live runtime draw txid (TASK-003)
    ================================================================ *)

Definition draw_frozen_txid : Z := frozen_draw_txid.
Definition draw_live_runtime_txid : Z := runtime_draw_txid.

Theorem draw_frozen_live_runtime_equivalence :
  draw_frozen_txid = draw_live_runtime_txid.
Proof. exact frozen_runtime_draw_txid_equal. Qed.

(** ================================================================
    Part D:  Cross-domain bridge (golden projected txid vs TASK-003 txid)
    ================================================================ *)

Definition draw_cross_domain_result (coq_txid : Z) : DrawEquivResult :=
  if Z.eqb coq_txid draw_frozen_txid then DER_Resolved else DER_Disagreement.

(** Cross-domain check uses the imported-index projection (equal to the Coq
    recomputed projection by [winner_projection_sound]). Looking up a stored
    index avoids re-running the sigma cross-product over every golden row just
    to ask whether any winner txid collides with the TASK-003 frozen constant. *)
Definition golden_row_matches_frozen (e : DrawReprEntry) : bool :=
  Z.eqb (entry_runtime_txid e) draw_frozen_txid.

Definition any_golden_row_matches_frozen : bool :=
  existsb golden_row_matches_frozen representation_catalog.

(** Honest: no golden vector's projected winner txid equals the TASK-003 frozen
    draw txid (different candidate domains). *)
Lemma any_golden_row_matches_frozen_false : any_golden_row_matches_frozen = false.
Proof. vm_compute. reflexivity. Qed.

Lemma existsb_false_In :
  forall {A : Type} (f : A -> bool) (l : list A) (x : A),
    existsb f l = false -> In x l -> f x = false.
Proof.
  induction l as [|a t IH]; simpl; intros x Hex Hin.
  - contradiction.
  - apply Bool.orb_false_iff in Hex. destruct Hex as [Ha Ht].
    destruct Hin as [-> | Hin]; [exact Ha | apply IH; assumption].
Qed.

Lemma draw_cross_domain_all_disagree :
  forall e, In e representation_catalog ->
    draw_cross_domain_result (draw_coq_projected_txid e) = DER_Disagreement.
Proof.
  intros e Hin.
  unfold draw_cross_domain_result.
  rewrite (draw_coq_runtime_shaped_equivalence e Hin).
  unfold draw_runtime_shaped_txid.
  pose proof (existsb_false_In golden_row_matches_frozen
                representation_catalog e
                any_golden_row_matches_frozen_false Hin) as Hfalse.
  unfold golden_row_matches_frozen, entry_runtime_txid in Hfalse.
  revert Hfalse.
  destruct (winner_txid_projection (dre_candidates e) (dre_imported_index e) =?
            draw_frozen_txid)%Z; [discriminate|reflexivity].
Qed.

(** ================================================================
    Part E:  Draw component verdict (PARTIAL — projection only)
    ================================================================ *)

Definition draw_equivalence_projection_verified : bool := true.

Definition draw_equivalence_cross_domain_closed : bool := false.

Definition draw_equivalence_status_partial : bool :=
  draw_equivalence_projection_verified &&
  negb draw_equivalence_cross_domain_closed.

Lemma draw_equivalence_status_partial_value :
  draw_equivalence_status_partial = true.
Proof. vm_compute. reflexivity. Qed.

(** Global equivalence remains false; only Draw projection layer is verified. *)
Lemma draw_does_not_establish_global_equivalence :
  negb equivalence_claimed = true.
Proof. exact equivalence_not_claimed. Qed.

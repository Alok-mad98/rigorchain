(** Phase4GRefinement/EquivalenceObligationCatalog.v
    Theorem obligation catalog for first equivalence-proof preparation
    (TASK-010X).

    This is NOT an equivalence proof. It only records the exact theorem
    obligations required to remove the disagreements classified in
    RefinementClosureAnalysis.v. No oracle/readiness/status/bridge/comparison layer
    is introduced, and no consensus / runtime / RFC / SHA256 / ConvergeRoot /
    ConnectBlock code is modified. *)

From Coq Require Import List Arith.
Import ListNotations.
Require Import Rigor.Phase4FConsensus.ConsensusOracleBridge.
Require Import Rigor.Phase4GRefinement.RefinementClosureAnalysis.

(** REQUIRED: the four theorem obligations that block disagreement removal. *)
Inductive EquivalenceObligation :=
| EO_DrawHashProjection
| EO_ReplayStateEncoding
| EO_ReorgStateEncoding
| EO_ConnectBlockStateEncoding.

Record ObligationRecord := {
  eo_kind : EquivalenceObligation;
  eo_disagreement_count : nat;
  eo_blocking : bool;
}.

(** One component-level obligation accounts for two disagreements:
    semantic-vs-runtime and semantic-vs-frozen. *)
Definition obligation_for_requirement
  (r : RefinementClosureRequirement) : ObligationRecord :=
  match r.(rcr_component), r.(rcr_required_mapping) with
  | BC_Draw, RCR_HashProjectionRequired =>
      {| eo_kind := EO_DrawHashProjection;
         eo_disagreement_count := disagreement_source_count_per_requirement;
         eo_blocking := true |}
  | BC_Replay, RCR_StateEncodingRequired =>
      {| eo_kind := EO_ReplayStateEncoding;
         eo_disagreement_count := disagreement_source_count_per_requirement;
         eo_blocking := true |}
  | BC_Reorg, RCR_StateEncodingRequired =>
      {| eo_kind := EO_ReorgStateEncoding;
         eo_disagreement_count := disagreement_source_count_per_requirement;
         eo_blocking := true |}
  | BC_ConnectBlock, RCR_StateEncodingRequired =>
      {| eo_kind := EO_ConnectBlockStateEncoding;
         eo_disagreement_count := disagreement_source_count_per_requirement;
         eo_blocking := true |}
  | _, _ =>
      (** Defensive fallback is unreachable for the 009W catalog; count zero
          prevents inventing a covered disagreement. *)
      {| eo_kind := EO_DrawHashProjection;
         eo_disagreement_count := 0;
         eo_blocking := false |}
  end.

(** REQUIRED: all obligations, derived from 009W classified requirements. *)
Definition obligation_catalog : list ObligationRecord :=
  map obligation_for_requirement refinement_closure_requirements.

Definition obligation_record_count : nat := length obligation_catalog.

Definition accounted_disagreement_count : nat :=
  fold_right (fun o acc => eo_disagreement_count o + acc) 0 obligation_catalog.

Definition obligation_is_blocking (o : ObligationRecord) : bool := o.(eo_blocking).

Lemma obligation_catalog_nonempty : obligation_catalog <> nil.
Proof. vm_compute. discriminate. Qed.

(** REQUIRED: the four obligations account for all 8 009W disagreements. *)
Lemma all_disagreements_accounted_for : accounted_disagreement_count = 8%nat.
Proof. vm_compute. reflexivity. Qed.

(** REQUIRED: counted disagreements match the 009W explained disagreement count. *)
Lemma obligation_count_matches_disagreement_count :
  accounted_disagreement_count = explained_disagreement_count.
Proof. vm_compute. reflexivity. Qed.

Lemma obligation_record_count_value : obligation_record_count = 4%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma all_obligations_blocking :
  forallb obligation_is_blocking obligation_catalog = true.
Proof. vm_compute. reflexivity. Qed.

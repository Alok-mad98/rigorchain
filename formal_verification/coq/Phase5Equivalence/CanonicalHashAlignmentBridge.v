(** Phase5Equivalence/CanonicalHashAlignmentBridge.v
    Canonical hash alignment bridge (TASK-018GG-D) — ESTABLISHMENT ONLY.

    Structural alignment between abstract Coq state hashes (018GG-C),
    frozen-C++ hash placeholders, and runtime hash placeholders.

    HONESTY BOUNDARY (CRITICAL):
    - This module DOES NOT claim Coq = Runtime nor Coq = Frozen C++.
    - Cross-source alignment functions NEVER return [AR_Aligned_Structurally];
      they emit [AR_Misaligned_DomainGap] (cross-domain) or
      [AR_MissingProjection] (component mismatch).
    - No SHA-256 / crypto / external hash imports.
    - No frozen-C++ / runtime hash payloads imported; opaque sentinels only.
    - PD-4F-OB-02 / PD-4F-CPP-01 remain OPEN. *)

From Coq Require Import List ZArith Bool.
Import ListNotations.
Require Import Rigor.Phase4FConsensus.ConsensusMachine.
Require Import Rigor.Phase4FConsensus.SemanticObservationCatalog.
Require Import Rigor.Phase5Equivalence.EquivalenceFramework.
Require Import Rigor.Phase5Equivalence.CanonicalStateEncoding.
Require Import Rigor.Phase5Equivalence.CanonicalBytesProjection.
Require Import Rigor.Phase5Equivalence.CanonicalStateHashModel.

Open Scope Z_scope.

(** ================================================================
    Part A:  Component / source vocabulary
    ================================================================ *)

Inductive Component :=
| C_Draw
| C_Replay
| C_Reorg
| C_ConnectBlock
| C_Supply
| C_Eligibility.

Definition all_components : list Component :=
  [ C_Draw; C_Replay; C_Reorg; C_ConnectBlock; C_Supply; C_Eligibility ].

Lemma all_components_count : length all_components = 6%nat.
Proof. reflexivity. Qed.

Definition component_eqb (c1 c2 : Component) : bool :=
  match c1, c2 with
  | C_Draw, C_Draw => true
  | C_Replay, C_Replay => true
  | C_Reorg, C_Reorg => true
  | C_ConnectBlock, C_ConnectBlock => true
  | C_Supply, C_Supply => true
  | C_Eligibility, C_Eligibility => true
  | _, _ => false
  end.

Lemma component_eqb_refl : forall c, component_eqb c c = true.
Proof. intro c. destruct c; reflexivity. Qed.

Lemma component_eqb_eq :
  forall c1 c2, component_eqb c1 c2 = true -> c1 = c2.
Proof.
  intros c1 c2 H. destruct c1, c2; try discriminate; reflexivity.
Qed.

Lemma component_eqb_neq :
  forall c1 c2, c1 <> c2 -> component_eqb c1 c2 = false.
Proof.
  intros c1 c2 Hne. destruct c1, c2; try reflexivity; congruence.
Qed.

Inductive HashSource :=
| HS_CoqAbstract
| HS_FrozenCpp
| HS_Runtime.

Definition source_eqb (s1 s2 : HashSource) : bool :=
  match s1, s2 with
  | HS_CoqAbstract, HS_CoqAbstract => true
  | HS_FrozenCpp, HS_FrozenCpp => true
  | HS_Runtime, HS_Runtime => true
  | _, _ => false
  end.

Lemma source_eqb_refl : forall s, source_eqb s s = true.
Proof. intro s. destruct s; reflexivity. Qed.

(** ================================================================
    Part B:  HashObservation record
    ================================================================ *)

Record HashObservation := {
  ho_component : Component;
  ho_source : HashSource;
  ho_hash : Z;
  ho_origin : nat   (* 4 = Coq semantic provenance; 0 = opaque sentinel *)
}.

(** ================================================================
    Part C:  AlignmentResult vocabulary
    ================================================================ *)

Inductive AlignmentResult :=
| AR_Aligned_Structurally
| AR_Misaligned_DomainGap
| AR_MissingProjection.

Definition ar_aligned (r : AlignmentResult) : bool :=
  match r with AR_Aligned_Structurally => true | _ => false end.
Definition ar_misaligned (r : AlignmentResult) : bool :=
  match r with AR_Misaligned_DomainGap => true | _ => false end.
Definition ar_missing (r : AlignmentResult) : bool :=
  match r with AR_MissingProjection => true | _ => false end.

(** [AR_Misaligned_DomainGap] is structurally distinct from
    [AR_MissingProjection]: a domain gap is a deliberate cross-domain
    alignment outcome, not an error or absence. *)
Lemma alignment_domain_gap_is_structural_not_failure :
  AR_Misaligned_DomainGap <> AR_MissingProjection.
Proof. discriminate. Qed.

(** ================================================================
    Part D:  Alignment relation (Prop) and alignment functions
    ================================================================ *)

(** Structural alignment relation: SAME COMPONENT only. Never asserts
    [ho_hash] equality between sources. *)
Definition CanonicalHashAlignment (a b : HashObservation) : Prop :=
  a.(ho_component) = b.(ho_component).

(** Self-alignment: same component AND same source.
    Used for the structural diagonal (each observation aligned with itself). *)
Definition align_self (a b : HashObservation) : AlignmentResult :=
  if negb (component_eqb a.(ho_component) b.(ho_component))
  then AR_MissingProjection
  else if source_eqb a.(ho_source) b.(ho_source)
       then AR_Aligned_Structurally
       else AR_Misaligned_DomainGap.

(** Cross-source alignment Coq vs Runtime — NEVER returns
    [AR_Aligned_Structurally]. Different domains by construction. *)
Definition align_coq_vs_runtime (a b : HashObservation) : AlignmentResult :=
  if negb (component_eqb a.(ho_component) b.(ho_component))
  then AR_MissingProjection
  else if andb (source_eqb a.(ho_source) HS_CoqAbstract)
               (source_eqb b.(ho_source) HS_Runtime)
       then AR_Misaligned_DomainGap
       else AR_MissingProjection.

(** Cross-source alignment Coq vs Frozen — NEVER returns
    [AR_Aligned_Structurally]. *)
Definition align_coq_vs_frozen (a b : HashObservation) : AlignmentResult :=
  if negb (component_eqb a.(ho_component) b.(ho_component))
  then AR_MissingProjection
  else if andb (source_eqb a.(ho_source) HS_CoqAbstract)
               (source_eqb b.(ho_source) HS_FrozenCpp)
       then AR_Misaligned_DomainGap
       else AR_MissingProjection.

(** Generic list-zip alignment using Coq-vs-Runtime (caller chooses). *)
Fixpoint align_all_observations
  (coqs : list HashObservation) (others : list HashObservation)
  : list AlignmentResult :=
  match coqs, others with
  | [], _ => []
  | _, [] => []
  | a :: ar, b :: br =>
      align_coq_vs_runtime a b :: align_all_observations ar br
  end.

(** ================================================================
    Part E:  Default 6 x 3 observation tables (placeholders only)
    ================================================================ *)

(** Coq-abstract observation per component. For Replay/Reorg/ConnectBlock the
    hash is the 018GG-C [hash_*_state] over a phase-4F sample trace; for
    Draw/Supply/Eligibility there is no encoder yet, so we use [0] as a
    typed placeholder (NO equivalence claim). *)
Definition observe_coq (c : Component) : HashObservation :=
  let h :=
    match c with
    | C_Replay      => hash_replay_state replay_trace
    | C_Reorg       => hash_reorg_state reorg_trace
    | C_ConnectBlock => hash_connectblock_state [ TrAcceptBlock 7 ]
    | _ => 0
    end
  in
  {| ho_component := c;
     ho_source := HS_CoqAbstract;
     ho_hash := h;
     ho_origin := 4%nat |}.

(** Frozen-C++ observation: opaque sentinel; we do NOT import the actual
    256-bit hash. *)
Definition observe_frozen (c : Component) : HashObservation :=
  {| ho_component := c;
     ho_source := HS_FrozenCpp;
     ho_hash := 0;
     ho_origin := 0%nat |}.

(** Runtime observation: opaque sentinel; we do NOT import the actual
    runtime hash. *)
Definition observe_runtime (c : Component) : HashObservation :=
  {| ho_component := c;
     ho_source := HS_Runtime;
     ho_hash := 0;
     ho_origin := 0%nat |}.

Definition coq_observations : list HashObservation :=
  map observe_coq all_components.
Definition frozen_observations : list HashObservation :=
  map observe_frozen all_components.
Definition runtime_observations : list HashObservation :=
  map observe_runtime all_components.

Lemma coq_observations_count : length coq_observations = 6%nat.
Proof. unfold coq_observations. rewrite map_length.
       exact all_components_count. Qed.
Lemma frozen_observations_count : length frozen_observations = 6%nat.
Proof. unfold frozen_observations. rewrite map_length.
       exact all_components_count. Qed.
Lemma runtime_observations_count : length runtime_observations = 6%nat.
Proof. unfold runtime_observations. rewrite map_length.
       exact all_components_count. Qed.

(** ================================================================
    Part F:  Per-component alignment triple
    ================================================================ *)

(** For each component: (Coq self) :: (Coq vs Frozen) :: (Coq vs Runtime). *)
Definition alignments_for_component (c : Component) : list AlignmentResult :=
  let oc := observe_coq c in
  [ align_self oc oc;
    align_coq_vs_frozen oc (observe_frozen c);
    align_coq_vs_runtime oc (observe_runtime c) ].

Lemma alignments_for_component_length :
  forall c, length (alignments_for_component c) = 3%nat.
Proof. intro c. reflexivity. Qed.

Definition all_alignments : list AlignmentResult :=
  flat_map alignments_for_component all_components.

Lemma all_alignments_length : length all_alignments = 18%nat.
Proof. vm_compute. reflexivity. Qed.

(** ================================================================
    Part G:  Required proof obligations
    ================================================================ *)

(** Pure functions are definitionally deterministic; equal inputs yield
    equal outputs. *)
Lemma alignment_deterministic :
  forall a1 a2 b1 b2 : HashObservation,
    a1 = a2 -> b1 = b2 ->
    align_self a1 b1 = align_self a2 b2 /\
    align_coq_vs_frozen a1 b1 = align_coq_vs_frozen a2 b2 /\
    align_coq_vs_runtime a1 b1 = align_coq_vs_runtime a2 b2.
Proof.
  intros a1 a2 b1 b2 Ha Hb. subst.
  split; [reflexivity | ].
  split; reflexivity.
Qed.

Lemma alignment_total :
  forall a b : HashObservation,
    (exists r, align_self a b = r) /\
    (exists r, align_coq_vs_frozen a b = r) /\
    (exists r, align_coq_vs_runtime a b = r).
Proof.
  intros a b.
  split; [exists (align_self a b); reflexivity | ].
  split; [exists (align_coq_vs_frozen a b); reflexivity | ].
  exists (align_coq_vs_runtime a b); reflexivity.
Qed.

(** Component indices must match for any non-MissingProjection result. *)
Lemma alignment_reflects_component_equality_only :
  forall a b : HashObservation,
    a.(ho_component) <> b.(ho_component) ->
    align_self a b = AR_MissingProjection /\
    align_coq_vs_frozen a b = AR_MissingProjection /\
    align_coq_vs_runtime a b = AR_MissingProjection.
Proof.
  intros a b Hne.
  pose proof (component_eqb_neq _ _ Hne) as Heqb.
  unfold align_self, align_coq_vs_frozen, align_coq_vs_runtime.
  rewrite Heqb. simpl.
  split; [reflexivity | ].
  split; reflexivity.
Qed.

(** Restated as preservation of component indexing. *)
Lemma alignment_preserves_component_indexing :
  forall a b : HashObservation,
    align_self a b <> AR_MissingProjection ->
    a.(ho_component) = b.(ho_component).
Proof.
  intros a b Hne.
  destruct (component_eqb a.(ho_component) b.(ho_component)) eqn:Heqb.
  - apply component_eqb_eq. exact Heqb.
  - exfalso. apply Hne.
    unfold align_self. rewrite Heqb. simpl. reflexivity.
Qed.

(** CRITICAL: cross-source alignment functions NEVER return
    [AR_Aligned_Structurally]. Hence detecting an aligned cell does not
    leak an equivalence claim to a different source. *)
Lemma no_equivalence_leakage :
  forall a b : HashObservation,
    align_coq_vs_runtime a b <> AR_Aligned_Structurally /\
    align_coq_vs_frozen a b <> AR_Aligned_Structurally.
Proof.
  intros a b. split.
  - unfold align_coq_vs_runtime.
    destruct (negb (component_eqb a.(ho_component) b.(ho_component)));
      [discriminate | ].
    destruct (andb (source_eqb a.(ho_source) HS_CoqAbstract)
                   (source_eqb b.(ho_source) HS_Runtime));
      discriminate.
  - unfold align_coq_vs_frozen.
    destruct (negb (component_eqb a.(ho_component) b.(ho_component)));
      [discriminate | ].
    destruct (andb (source_eqb a.(ho_source) HS_CoqAbstract)
                   (source_eqb b.(ho_source) HS_FrozenCpp));
      discriminate.
Qed.

(** Even at the global framework level: the existence of structurally
    aligned cells (self-alignment) does NOT bridge to global equivalence. *)
Lemma no_equivalence_leakage_global :
  equivalence_claimed = false.
Proof. reflexivity. Qed.

(** ================================================================
    Part H:  Concrete alignment-table tallies (vm_compute)
    ================================================================ *)

(** Per-component triple decomposes as: self (CoqAbstract = CoqAbstract) ->
    AR_Aligned_Structurally; the two cross-source pairs -> AR_Misaligned_DomainGap. *)
Lemma alignments_for_component_value :
  forall c,
    alignments_for_component c
    = [ AR_Aligned_Structurally;
        AR_Misaligned_DomainGap;
        AR_Misaligned_DomainGap ].
Proof.
  intro c. destruct c; vm_compute; reflexivity.
Qed.

Lemma all_alignments_aligned_count :
  length (filter ar_aligned all_alignments) = 6%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma all_alignments_misaligned_count :
  length (filter ar_misaligned all_alignments) = 12%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma all_alignments_missing_count :
  length (filter ar_missing all_alignments) = 0%nat.
Proof. vm_compute. reflexivity. Qed.

(** ================================================================
    Part I:  Framework status (no equivalence; PD remains OPEN)
    ================================================================ *)

Definition canonical_hash_alignment_framework_established : bool := true.

Definition coq_eq_runtime_established : bool := false.
Definition coq_eq_frozen_established : bool := false.

Lemma canonical_hash_alignment_framework_established_value :
  canonical_hash_alignment_framework_established = true.
Proof. reflexivity. Qed.

Lemma coq_eq_runtime_not_established :
  negb coq_eq_runtime_established = true.
Proof. reflexivity. Qed.

Lemma coq_eq_frozen_not_established :
  negb coq_eq_frozen_established = true.
Proof. reflexivity. Qed.

Lemma canonical_hash_alignment_no_equivalence_claimed :
  negb equivalence_claimed = true.
Proof. exact equivalence_not_claimed. Qed.

Lemma canonical_hash_alignment_pd_ob_02_open :
  negb proof_debt_placeholder.(pdp_pd_4f_ob_02_closed) = true.
Proof. exact canonical_encoding_pd_ob_02_open. Qed.

Lemma canonical_hash_alignment_pd_cpp_01_open :
  negb proof_debt_placeholder.(pdp_pd_4f_cpp_01_closed) = true.
Proof. exact canonical_encoding_pd_cpp_01_open. Qed.

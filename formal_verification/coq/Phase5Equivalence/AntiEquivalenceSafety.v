(** Phase5Equivalence/AntiEquivalenceSafety.v
    Anti-equivalence safety / boundary layer (TASK-018GG-E) — ESTABLISHMENT ONLY.

    Provable safeguard ensuring that the structural alignment from 018GG-D
    cannot be reinterpreted as Coq = Runtime or Coq = Frozen C++ equivalence.
    Cross-source pairs involving HS_CoqAbstract and any external source are
    explicitly tagged as [AER_ExplicitNonEquivalence].

    HONESTY BOUNDARY (CRITICAL):
    - This module DOES NOT claim Coq = Runtime nor Coq = Frozen C++.
    - No SHA-256 / external hash imports.
    - No frozen / runtime hash payloads imported (opaque sentinels only).
    - Only allowed dependencies: 018GG-B, 018GG-C, 018GG-D.
    - PD-4F-OB-02 / PD-4F-CPP-01 remain OPEN. *)

From Coq Require Import List ZArith Bool.
Import ListNotations.
Require Import Rigor.Phase4FConsensus.ConsensusMachine.
Require Import Rigor.Phase4FConsensus.SemanticObservationCatalog.
Require Import Rigor.Phase5Equivalence.EquivalenceFramework.
Require Import Rigor.Phase5Equivalence.CanonicalStateEncoding.
Require Import Rigor.Phase5Equivalence.CanonicalBytesProjection.
Require Import Rigor.Phase5Equivalence.CanonicalStateHashModel.
Require Import Rigor.Phase5Equivalence.CanonicalHashAlignmentBridge.

Open Scope Z_scope.

(** ================================================================
    Part A:  AntiEquivalenceObservation record
    ================================================================ *)

Record AntiEquivalenceObservation := {
  ae_component : Component;
  ae_source : HashSource;
  ae_hash : CanonicalStateHash;
  ae_origin : nat   (* 4 = Coq provenance; 0 = opaque external sentinel *)
}.

(** ================================================================
    Part B:  AntiEquivalenceResult vocabulary
    ================================================================ *)

Inductive AntiEquivalenceResult :=
| AER_StructurallyAligned       (* same component, same source — diagonal only *)
| AER_DomainGap                 (* same component, different non-Coq sources *)
| AER_Missing                   (* component mismatch *)
| AER_ExplicitNonEquivalence.   (* CoqAbstract vs Frozen/Runtime — never equivalence *)

Definition aer_aligned (r : AntiEquivalenceResult) : bool :=
  match r with AER_StructurallyAligned => true | _ => false end.
Definition aer_domain_gap (r : AntiEquivalenceResult) : bool :=
  match r with AER_DomainGap => true | _ => false end.
Definition aer_missing (r : AntiEquivalenceResult) : bool :=
  match r with AER_Missing => true | _ => false end.
Definition aer_non_equivalence (r : AntiEquivalenceResult) : bool :=
  match r with AER_ExplicitNonEquivalence => true | _ => false end.

(** ================================================================
    Part C:  Cross-source classification helper
    ================================================================ *)

(** True iff one source is [HS_CoqAbstract] and the other is an external
    source ([HS_FrozenCpp] / [HS_Runtime]). *)
Definition is_cross_coq_external (s1 s2 : HashSource) : bool :=
  orb (andb (source_eqb s1 HS_CoqAbstract)
            (orb (source_eqb s2 HS_FrozenCpp)
                 (source_eqb s2 HS_Runtime)))
      (andb (source_eqb s2 HS_CoqAbstract)
            (orb (source_eqb s1 HS_FrozenCpp)
                 (source_eqb s1 HS_Runtime))).

(** ================================================================
    Part D:  Anti-equivalence checker
    ================================================================ *)

Definition check_anti_equivalence
  (a b : AntiEquivalenceObservation) : AntiEquivalenceResult :=
  if negb (component_eqb a.(ae_component) b.(ae_component))
  then AER_Missing
  else if source_eqb a.(ae_source) b.(ae_source)
       then AER_StructurallyAligned
       else if is_cross_coq_external a.(ae_source) b.(ae_source)
            then AER_ExplicitNonEquivalence
            else AER_DomainGap.

(** Pairwise list version. *)
Fixpoint check_all_anti_equivalence
  (xs ys : list AntiEquivalenceObservation)
  : list AntiEquivalenceResult :=
  match xs, ys with
  | [], _ => []
  | _, [] => []
  | a :: ra, b :: rb =>
      check_anti_equivalence a b :: check_all_anti_equivalence ra rb
  end.

(** ================================================================
    Part E:  Default 6 x 3 observation tables (placeholders only)
    ================================================================ *)

Definition observe_ae_coq (c : Component) : AntiEquivalenceObservation :=
  let h :=
    match c with
    | C_Replay      => hash_replay_state replay_trace
    | C_Reorg       => hash_reorg_state reorg_trace
    | C_ConnectBlock => hash_connectblock_state [ TrAcceptBlock 7 ]
    | _ => 0
    end
  in
  {| ae_component := c;
     ae_source := HS_CoqAbstract;
     ae_hash := h;
     ae_origin := 4%nat |}.

Definition observe_ae_frozen (c : Component) : AntiEquivalenceObservation :=
  {| ae_component := c;
     ae_source := HS_FrozenCpp;
     ae_hash := 0;
     ae_origin := 0%nat |}.

Definition observe_ae_runtime (c : Component) : AntiEquivalenceObservation :=
  {| ae_component := c;
     ae_source := HS_Runtime;
     ae_hash := 0;
     ae_origin := 0%nat |}.

Definition ae_coq_observations : list AntiEquivalenceObservation :=
  map observe_ae_coq all_components.
Definition ae_frozen_observations : list AntiEquivalenceObservation :=
  map observe_ae_frozen all_components.
Definition ae_runtime_observations : list AntiEquivalenceObservation :=
  map observe_ae_runtime all_components.

(** ================================================================
    Part F:  Per-component anti-equivalence triple
    ================================================================ *)

(** For each component: (Coq self) :: (Coq vs Frozen) :: (Coq vs Runtime).
    Coq self  -> AER_StructurallyAligned (diagonal only).
    Coq vs Frozen / Runtime -> AER_ExplicitNonEquivalence. *)
Definition ae_results_for_component (c : Component)
  : list AntiEquivalenceResult :=
  let oc := observe_ae_coq c in
  [ check_anti_equivalence oc oc;
    check_anti_equivalence oc (observe_ae_frozen c);
    check_anti_equivalence oc (observe_ae_runtime c) ].

Lemma ae_results_for_component_value :
  forall c,
    ae_results_for_component c
    = [ AER_StructurallyAligned;
        AER_ExplicitNonEquivalence;
        AER_ExplicitNonEquivalence ].
Proof. intro c. destruct c; vm_compute; reflexivity. Qed.

Definition all_ae_results : list AntiEquivalenceResult :=
  flat_map ae_results_for_component all_components.

Lemma all_ae_results_length : length all_ae_results = 18%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma all_ae_results_aligned_count :
  length (filter aer_aligned all_ae_results) = 6%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma all_ae_results_domain_gap_count :
  length (filter aer_domain_gap all_ae_results) = 0%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma all_ae_results_missing_count :
  length (filter aer_missing all_ae_results) = 0%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma all_ae_results_non_equivalence_count :
  length (filter aer_non_equivalence all_ae_results) = 12%nat.
Proof. vm_compute. reflexivity. Qed.

(** ================================================================
    Part G:  Required proof obligations
    ================================================================ *)

Lemma anti_equivalence_deterministic :
  forall a1 a2 b1 b2 : AntiEquivalenceObservation,
    a1 = a2 -> b1 = b2 ->
    check_anti_equivalence a1 b1 = check_anti_equivalence a2 b2.
Proof. intros a1 a2 b1 b2 Ha Hb. subst. reflexivity. Qed.

Lemma anti_equivalence_total :
  forall a b : AntiEquivalenceObservation,
    exists r, check_anti_equivalence a b = r.
Proof. intros a b. exists (check_anti_equivalence a b). reflexivity. Qed.

(** Component preservation: any non-Missing result implies components match. *)
Lemma anti_equivalence_preserves_component :
  forall a b : AntiEquivalenceObservation,
    check_anti_equivalence a b <> AER_Missing ->
    a.(ae_component) = b.(ae_component).
Proof.
  intros a b Hne.
  destruct (component_eqb a.(ae_component) b.(ae_component)) eqn:Heqb.
  - apply component_eqb_eq. exact Heqb.
  - exfalso. apply Hne.
    unfold check_anti_equivalence. rewrite Heqb. simpl. reflexivity.
Qed.

(** Domain-gap detection: same component, cross-source between Coq and an
    external source -> [AER_ExplicitNonEquivalence] (the explicit gap). *)
Lemma anti_equivalence_detects_domain_gaps :
  forall a b : AntiEquivalenceObservation,
    a.(ae_component) = b.(ae_component) ->
    a.(ae_source) <> b.(ae_source) ->
    is_cross_coq_external a.(ae_source) b.(ae_source) = true ->
    check_anti_equivalence a b = AER_ExplicitNonEquivalence.
Proof.
  intros a b Hcomp Hsrc Hcross.
  unfold check_anti_equivalence.
  rewrite Hcomp, component_eqb_refl. simpl.
  destruct (source_eqb a.(ae_source) b.(ae_source)) eqn:Heq.
  - exfalso. apply Hsrc.
    destruct a.(ae_source), b.(ae_source);
      try discriminate; try reflexivity.
  - rewrite Hcross. reflexivity.
Qed.

(** CRITICAL: cross Coq <-> external pair NEVER yields
    [AER_StructurallyAligned]. Hence detecting any aligned cell cannot leak
    a Coq = Runtime / Coq = Frozen equivalence. *)
Lemma anti_equivalence_no_false_equivalence_leakage :
  forall a b : AntiEquivalenceObservation,
    is_cross_coq_external a.(ae_source) b.(ae_source) = true ->
    check_anti_equivalence a b <> AER_StructurallyAligned.
Proof.
  intros a b Hcross.
  unfold check_anti_equivalence.
  destruct (negb (component_eqb a.(ae_component) b.(ae_component)));
    [discriminate | ].
  destruct (source_eqb a.(ae_source) b.(ae_source)) eqn:Heqs.
  - exfalso.
    unfold is_cross_coq_external in Hcross.
    destruct a.(ae_source), b.(ae_source);
      simpl in Heqs, Hcross; try discriminate.
  - rewrite Hcross. discriminate.
Qed.

(** Even at the global framework level: the existence of structurally
    aligned cells (self-alignment) does NOT bridge to global equivalence. *)
Lemma anti_equivalence_no_equivalence_leakage_global :
  equivalence_claimed = false.
Proof. reflexivity. Qed.

(** ================================================================
    Part H:  Circular-dependency record (re-exported)
    ================================================================ *)

Record AntiEquivalenceCircularRecord := {
  aec_imported_eligibility : bool;   (* PD-4F-OB-03 *)
  aec_payload_domain_gap : bool;     (* height vs 256-bit hash *)
  aec_supply_absent : bool;          (* frozen Supply absent *)
}.

(** All three flags remain TRUE: circularity is RECORDED, not closed. *)
Definition anti_equivalence_circularity : AntiEquivalenceCircularRecord :=
  {| aec_imported_eligibility := true;
     aec_payload_domain_gap := true;
     aec_supply_absent := true |}.

Lemma anti_equivalence_circularity_recorded :
  anti_equivalence_circularity.(aec_imported_eligibility) = true /\
  anti_equivalence_circularity.(aec_payload_domain_gap) = true /\
  anti_equivalence_circularity.(aec_supply_absent) = true.
Proof. repeat split; reflexivity. Qed.

(** ================================================================
    Part I:  Framework status (no equivalence; PD remains OPEN)
    ================================================================ *)

Definition anti_equivalence_safety_framework_established : bool := true.

Definition ae_coq_eq_runtime_established : bool := false.
Definition ae_coq_eq_frozen_established : bool := false.

Lemma anti_equivalence_safety_framework_established_value :
  anti_equivalence_safety_framework_established = true.
Proof. reflexivity. Qed.

Lemma ae_coq_eq_runtime_not_established :
  negb ae_coq_eq_runtime_established = true.
Proof. reflexivity. Qed.

Lemma ae_coq_eq_frozen_not_established :
  negb ae_coq_eq_frozen_established = true.
Proof. reflexivity. Qed.

Lemma anti_equivalence_no_equivalence_claimed :
  negb equivalence_claimed = true.
Proof. exact equivalence_not_claimed. Qed.

Lemma anti_equivalence_pd_ob_02_open :
  negb proof_debt_placeholder.(pdp_pd_4f_ob_02_closed) = true.
Proof. exact canonical_encoding_pd_ob_02_open. Qed.

Lemma anti_equivalence_pd_cpp_01_open :
  negb proof_debt_placeholder.(pdp_pd_4f_cpp_01_closed) = true.
Proof. exact canonical_encoding_pd_cpp_01_open. Qed.

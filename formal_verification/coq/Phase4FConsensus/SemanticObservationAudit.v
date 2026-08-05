(** Phase4FConsensus/SemanticObservationAudit.v
    Audit of Coq-derived semantic observations (PHASE-4F-001N).

    Counts the independently derived observations per component. These payloads
    originate from the Coq model (heights / winner index), NOT from runtime or
    frozen payloads — reducing the 001M projection circularity. This does NOT
    establish Coq = Runtime / Coq = C++ (PD-4F-OB-02 / PD-4F-CPP-01 stay open).
    AUDIT only. *)

From Coq Require Import List ZArith.
Import ListNotations.
Require Import Rigor.Phase4FConsensus.ConsensusOracleBridge.
Require Import Rigor.Phase4FConsensus.SemanticObservationDerivation.
Require Import Rigor.Phase4FConsensus.SemanticObservationCatalog.

Definition obs_is_draw (o : SemanticObservation) : bool :=
  match o.(so_component) with BC_Draw => true | _ => false end.
Definition obs_is_replay (o : SemanticObservation) : bool :=
  match o.(so_component) with BC_Replay => true | _ => false end.
Definition obs_is_reorg (o : SemanticObservation) : bool :=
  match o.(so_component) with BC_Reorg => true | _ => false end.
Definition obs_is_connectblock (o : SemanticObservation) : bool :=
  match o.(so_component) with BC_ConnectBlock => true | _ => false end.

Definition semantic_observation_count : nat := length semantic_observation_catalog.
Definition draw_semantic_count : nat :=
  length (filter obs_is_draw semantic_observation_catalog).
Definition replay_semantic_count : nat :=
  length (filter obs_is_replay semantic_observation_catalog).
Definition reorg_semantic_count : nat :=
  length (filter obs_is_reorg semantic_observation_catalog).
Definition connectblock_semantic_count : nat :=
  length (filter obs_is_connectblock semantic_observation_catalog).

Lemma semantic_observation_count_value : semantic_observation_count = 4%nat.
Proof. reflexivity. Qed.

Lemma draw_semantic_count_value : draw_semantic_count = 1%nat.
Proof. reflexivity. Qed.
Lemma replay_semantic_count_value : replay_semantic_count = 1%nat.
Proof. reflexivity. Qed.
Lemma reorg_semantic_count_value : reorg_semantic_count = 1%nat.
Proof. reflexivity. Qed.
Lemma connectblock_semantic_count_value : connectblock_semantic_count = 1%nat.
Proof. reflexivity. Qed.

(** Every catalog observation carries Coq-semantic provenance (origin = 4). *)
Lemma all_semantic_origin :
  forallb (fun o => Nat.eqb o.(so_origin) 4) semantic_observation_catalog = true.
Proof. reflexivity. Qed.

(** REQUIRED-3: catalog is non-empty. *)
Lemma catalog_nonempty : semantic_observation_count <> 0%nat.
Proof. unfold semantic_observation_count. simpl. discriminate. Qed.

(** REQUIRED-3: at least one independently derived semantic observation exists. *)
Lemma semantic_entries_exist :
  exists o, In o semantic_observation_catalog /\ o.(so_present) = true.
Proof. exists sem_draw. split; [ simpl; left; reflexivity | reflexivity ]. Qed.

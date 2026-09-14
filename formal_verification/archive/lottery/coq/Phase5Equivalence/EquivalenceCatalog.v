(** Phase5Equivalence/EquivalenceCatalog.v
    Placeholder equivalence catalog (TASK-018FF-A).

    Maps each in-scope [BridgeComponent] × [EquivalenceSource] pair to
    [ECS_Pending]. No observations are imported and no payloads are compared.

    HONESTY BOUNDARY: scaffold only — all cells pending, zero resolved. *)

From Coq Require Import List.
Import ListNotations.
Require Import Rigor.Phase4FConsensus.ConsensusOracleBridge.
Require Import Rigor.Phase5Equivalence.EquivalenceFramework.

(** One catalog cell: component, source, pending status. *)
Record EquivalenceCatalogEntry := {
  ece_component : BridgeComponent;
  ece_source : EquivalenceSource;
  ece_status : EquivalenceClaimStatus;
  ece_kind : EquivalenceKind;
}.

Definition mk_pending_entry
  (c : BridgeComponent) (s : EquivalenceSource) (k : EquivalenceKind)
  : EquivalenceCatalogEntry :=
  {| ece_component := c;
     ece_source := s;
     ece_status := ECS_Pending;
     ece_kind := k |}.

(** Default equivalence kind per component (typed placeholder only). *)
Definition default_kind_for (c : BridgeComponent) : EquivalenceKind :=
  match c with
  | BC_Draw => EK_EncodingCommutativity
  | BC_Replay => EK_EncodingCommutativity
  | BC_Reorg => EK_EncodingCommutativity
  | BC_ConnectBlock => EK_EncodingCommutativity
  | BC_Supply => EK_SemanticObservationEquality
  | BC_RESRuntime => EK_RawPayloadEquality
  end.

(** Build pending entries for one component across all three sources. *)
Definition entries_for_component (c : BridgeComponent) : list EquivalenceCatalogEntry :=
  [ mk_pending_entry c ES_CoqSemantic (default_kind_for c);
    mk_pending_entry c ES_FrozenCpp (default_kind_for c);
    mk_pending_entry c ES_LiveRuntime (default_kind_for c) ].

(** REQUIRED: 5 components × 3 sources = 15 pending catalog cells. *)
Definition equivalence_catalog : list EquivalenceCatalogEntry :=
  entries_for_component BC_Draw ++
  entries_for_component BC_Replay ++
  entries_for_component BC_Reorg ++
  entries_for_component BC_ConnectBlock ++
  entries_for_component BC_Supply.

Definition catalog_entry_count : nat := length equivalence_catalog.

Definition catalog_pending_count : nat :=
  length (filter (fun e => claim_status_pending e.(ece_status)) equivalence_catalog).

Definition catalog_resolved_count : nat :=
  length (filter (fun e => claim_status_resolved e.(ece_status)) equivalence_catalog).

Lemma equivalence_catalog_nonempty : equivalence_catalog <> nil.
Proof. vm_compute. discriminate. Qed.

Lemma catalog_entry_count_value : catalog_entry_count = 15%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma catalog_pending_count_value : catalog_pending_count = 15%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma catalog_resolved_count_value : catalog_resolved_count = 0%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma catalog_counts_partition :
  catalog_pending_count + catalog_resolved_count = catalog_entry_count.
Proof. vm_compute. reflexivity. Qed.

Lemma all_catalog_entries_pending :
  forallb (fun e => claim_status_pending e.(ece_status)) equivalence_catalog
  = true.
Proof. vm_compute. reflexivity. Qed.

(** Catalog covers exactly the five Phase-5 equivalence components (each thrice). *)
Lemma catalog_component_multiset_ok :
  map (fun e => e.(ece_component)) equivalence_catalog
  = [ BC_Draw; BC_Draw; BC_Draw;
      BC_Replay; BC_Replay; BC_Replay;
      BC_Reorg; BC_Reorg; BC_Reorg;
      BC_ConnectBlock; BC_ConnectBlock; BC_ConnectBlock;
      BC_Supply; BC_Supply; BC_Supply ].
Proof. vm_compute. reflexivity. Qed.

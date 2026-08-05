(** Phase4GRefinement/RefinementClosureAnalysis.v
    Diagnostic payload-domain mismatch analysis (TASK-009W).

    This file does not introduce a new oracle/readiness/comparison layer. It only
    names the payload domains responsible for the 8 disagreements reported by
    TASK-008V and records the minimal theorem shape required to make those
    payloads comparable.

    AUDIT / DIAGNOSTIC ONLY. No consensus / runtime / RFC / SHA256 /
    ConvergeRoot / ConnectBlock / Replay / Reorg code is modified. No
    equivalence theorem is stated or claimed. *)

From Coq Require Import List.
Import ListNotations.
Require Import Rigor.Phase4FConsensus.ConsensusOracleBridge.

(** Payload domains that appear in the 008V disagreements. *)
Inductive RefinementPayloadDomain :=
| RPD_WinnerIndex          (* Coq semantic Draw payload *)
| RPD_WinnerTxid256        (* runtime/frozen Draw payload *)
| RPD_TraceHeight          (* Coq semantic Replay/Reorg/ConnectBlock payload *)
| RPD_TipHash256           (* runtime/frozen Replay payload *)
| RPD_ForkBlockHash256     (* runtime/frozen Reorg payload *)
| RPD_BlockHash256.        (* runtime/frozen ConnectBlock payload *)

(** REQUIRED-4: exact missing theorem/mapping class. *)
Inductive RefinementClosureMapping :=
| RCR_HashProjectionRequired
| RCR_StateEncodingRequired
| RCR_RuntimeNormalizationRequired
| RCR_MissingInformation
| RCR_Unknown.

(** REQUIRED-3: one diagnostic requirement per disagreement component.
    Each record covers both source-side disagreements for the component:
    semantic-vs-runtime and semantic-vs-frozen. *)
Record RefinementClosureRequirement := {
  rcr_component : BridgeComponent;
  rcr_semantic_domain : RefinementPayloadDomain;
  rcr_runtime_domain : RefinementPayloadDomain;
  rcr_frozen_domain : RefinementPayloadDomain;
  rcr_required_mapping : RefinementClosureMapping;
}.

(** Draw: Coq has an index; runtime/frozen have a txid. The minimal theorem is
    an index-to-txid hash/lookup projection theorem for the candidate set. *)
Definition rcr_draw : RefinementClosureRequirement :=
  {| rcr_component := BC_Draw;
     rcr_semantic_domain := RPD_WinnerIndex;
     rcr_runtime_domain := RPD_WinnerTxid256;
     rcr_frozen_domain := RPD_WinnerTxid256;
     rcr_required_mapping := RCR_HashProjectionRequired |}.

(** Replay: Coq has final trace height; runtime/frozen have a tip hash. The
    missing theorem must encode enough state to derive the concrete tip hash. *)
Definition rcr_replay : RefinementClosureRequirement :=
  {| rcr_component := BC_Replay;
     rcr_semantic_domain := RPD_TraceHeight;
     rcr_runtime_domain := RPD_TipHash256;
     rcr_frozen_domain := RPD_TipHash256;
     rcr_required_mapping := RCR_StateEncodingRequired |}.

(** Reorg: Coq has post-rollback height; runtime/frozen have a fork block hash. *)
Definition rcr_reorg : RefinementClosureRequirement :=
  {| rcr_component := BC_Reorg;
     rcr_semantic_domain := RPD_TraceHeight;
     rcr_runtime_domain := RPD_ForkBlockHash256;
     rcr_frozen_domain := RPD_ForkBlockHash256;
     rcr_required_mapping := RCR_StateEncodingRequired |}.

(** ConnectBlock: Coq has post-accept height; runtime/frozen have accepted block
    hash. The missing theorem must reconstruct/encode the accepted block bytes. *)
Definition rcr_connectblock : RefinementClosureRequirement :=
  {| rcr_component := BC_ConnectBlock;
     rcr_semantic_domain := RPD_TraceHeight;
     rcr_runtime_domain := RPD_BlockHash256;
     rcr_frozen_domain := RPD_BlockHash256;
     rcr_required_mapping := RCR_StateEncodingRequired |}.

(** Diagnostic catalog: exactly the four component classes that account for the
    8 TASK-008V disagreements (runtime + frozen for each component). Supply is
    missing, not a disagreement, so it is intentionally excluded. *)
Definition refinement_closure_requirements : list RefinementClosureRequirement :=
  [ rcr_draw; rcr_replay; rcr_reorg; rcr_connectblock ].

Definition mapping_is_unknown (m : RefinementClosureMapping) : bool :=
  match m with
  | RCR_Unknown => true
  | _ => false
  end.

Definition requirement_classified (r : RefinementClosureRequirement) : bool :=
  negb (mapping_is_unknown r.(rcr_required_mapping)).

(** REQUIRED-5: no disagreement component remains classified as unknown. *)
Lemma all_disagreements_classified :
  forallb requirement_classified refinement_closure_requirements = true.
Proof. reflexivity. Qed.

(** Sanity: the four component classes explain the eight TASK-008V disagreements
    because each has two sources (runtime and frozen). *)
Definition disagreement_source_count_per_requirement : nat := 2.

Definition explained_disagreement_count : nat :=
  length refinement_closure_requirements * disagreement_source_count_per_requirement.

Lemma explained_disagreement_count_value : explained_disagreement_count = 8%nat.
Proof. reflexivity. Qed.

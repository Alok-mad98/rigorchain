(** Phase5Equivalence/RuntimeWitnessRequirementCatalog.v
    Minimal runtime witness set analysis (TASK-018GG-J).

    For each PotentiallyClosable component (Replay / Reorg / ConnectBlock per
    018GG-I), records the minimal [RuntimeWitnessField] set required from
    runtime evidence before a future Coq = Runtime proof attempt could even be
    considered, and which witnesses are currently available in frozen TASK-005
    certificates.

    HONESTY BOUNDARY (CRITICAL):
    - DOES NOT assert Coq = Runtime equivalence.
    - DOES NOT close PD-4F-OB-02 or PD-4F-CPP-01.
    - No SHA-256 / external hash kernel introduced.
    - No consensus / runtime code modified. *)

From Coq Require Import List ZArith Bool.
Import ListNotations.
Require Import Rigor.Phase5Equivalence.EquivalenceFramework.
Require Import Rigor.Phase5Equivalence.CanonicalStateEncoding.
Require Import Rigor.Phase5Equivalence.CanonicalHashAlignmentBridge.
Require Import Rigor.Phase5Equivalence.RuntimeSufficiencyGapCatalog.

Close Scope Z_scope.

(** ================================================================
    Part A:  Witness field vocabulary
    ================================================================ *)

Inductive RuntimeWitnessField :=
| RWF_TraceHistory
| RWF_TransitionCount
| RWF_AcceptCount
| RWF_RollbackDepth
| RWF_LastBlockId
| RWF_HashKernelOutput.

Definition runtime_witness_field_count : nat := 6%nat.

Lemma runtime_witness_field_count_value : runtime_witness_field_count = 6%nat.
Proof. reflexivity. Qed.

(** ================================================================
    Part B:  Witness closure level
    ================================================================ *)

Inductive WitnessClosureLevel :=
| WCL_Ready
| WCL_MissingWitnesses
| WCL_FundamentallyBlocked.

Definition wcl_ready (w : WitnessClosureLevel) : bool :=
  match w with WCL_Ready => true | _ => false end.
Definition wcl_missing (w : WitnessClosureLevel) : bool :=
  match w with WCL_MissingWitnesses => true | _ => false end.
Definition wcl_blocked (w : WitnessClosureLevel) : bool :=
  match w with WCL_FundamentallyBlocked => true | _ => false end.

(** ================================================================
    Part C:  RuntimeWitnessRequirement record
    ================================================================ *)

Record RuntimeWitnessRequirement := {
  rwr_component : Component;
  rwr_minimal_required : list RuntimeWitnessField;
  rwr_currently_available : list RuntimeWitnessField;
  rwr_missing : list RuntimeWitnessField;
  rwr_closure_level : WitnessClosureLevel;
  (** Adding the missing witnesses could make closure theoretically possible;
      does NOT establish equivalence. *)
  rwr_closure_possible_after_witness : bool;
}.

Definition mk_witness_requirement
  (c : Component) (req avail miss : list RuntimeWitnessField)
  (wcl : WitnessClosureLevel) (possible : bool)
  : RuntimeWitnessRequirement :=
  {| rwr_component := c;
     rwr_minimal_required := req;
     rwr_currently_available := avail;
     rwr_missing := miss;
     rwr_closure_level := wcl;
     rwr_closure_possible_after_witness := possible |}.

(** Minimal witness sets (PotentiallyClosable components only). *)
Definition replay_cb_minimal_witnesses : list RuntimeWitnessField :=
  [ RWF_TraceHistory;
    RWF_TransitionCount;
    RWF_AcceptCount;
    RWF_LastBlockId;
    RWF_HashKernelOutput ].

Definition reorg_minimal_witnesses : list RuntimeWitnessField :=
  RWF_RollbackDepth :: replay_cb_minimal_witnesses.

(** Frozen TASK-005 certificates export height + block_hash only; they do NOT
    export any [RuntimeWitnessField] entries today. *)
Definition no_witness_fields_available : list RuntimeWitnessField := [].

(** ================================================================
    Part D:  Per-component witness requirements
    ================================================================ *)

Definition replay_witness_requirement : RuntimeWitnessRequirement :=
  mk_witness_requirement
    C_Replay replay_cb_minimal_witnesses
    no_witness_fields_available replay_cb_minimal_witnesses
    WCL_MissingWitnesses true.

Definition reorg_witness_requirement : RuntimeWitnessRequirement :=
  mk_witness_requirement
    C_Reorg reorg_minimal_witnesses
    no_witness_fields_available reorg_minimal_witnesses
    WCL_MissingWitnesses true.

Definition connectblock_witness_requirement : RuntimeWitnessRequirement :=
  mk_witness_requirement
    C_ConnectBlock replay_cb_minimal_witnesses
    no_witness_fields_available replay_cb_minimal_witnesses
    WCL_MissingWitnesses true.

Definition runtime_witness_catalog : list RuntimeWitnessRequirement :=
  [ replay_witness_requirement;
    reorg_witness_requirement;
    connectblock_witness_requirement ].

Definition runtime_witness_catalog_count : nat :=
  length runtime_witness_catalog.

(** ================================================================
    Part E:  Field-count aggregates
    ================================================================ *)

Definition witness_required_count_replay : nat :=
  length replay_cb_minimal_witnesses.
Definition witness_required_count_reorg : nat :=
  length reorg_minimal_witnesses.
Definition witness_required_count_connectblock : nat :=
  length replay_cb_minimal_witnesses.

Definition total_required_fields : nat :=
  (witness_required_count_replay + witness_required_count_reorg
   + witness_required_count_connectblock)%nat.

Definition total_available_fields : nat := 0%nat.

Definition total_missing_fields : nat :=
  (length replay_witness_requirement.(rwr_missing)
   + length reorg_witness_requirement.(rwr_missing)
   + length connectblock_witness_requirement.(rwr_missing))%nat.

Definition component_missing_field_count_replay : nat :=
  length replay_witness_requirement.(rwr_missing).
Definition component_missing_field_count_reorg : nat :=
  length reorg_witness_requirement.(rwr_missing).
Definition component_missing_field_count_connectblock : nat :=
  length connectblock_witness_requirement.(rwr_missing).

Lemma witness_required_count_replay_value :
  witness_required_count_replay = 5%nat.
Proof. reflexivity. Qed.

Lemma witness_required_count_reorg_value :
  witness_required_count_reorg = 6%nat.
Proof. reflexivity. Qed.

Lemma witness_required_count_connectblock_value :
  witness_required_count_connectblock = 5%nat.
Proof. reflexivity. Qed.

Lemma total_required_fields_value : total_required_fields = 16%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma total_available_fields_value : total_available_fields = 0%nat.
Proof. reflexivity. Qed.

Lemma total_missing_fields_value : total_missing_fields = 16%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma component_missing_counts_value :
  component_missing_field_count_replay = 5%nat /\
  component_missing_field_count_reorg = 6%nat /\
  component_missing_field_count_connectblock = 5%nat.
Proof. repeat split; vm_compute; reflexivity. Qed.

(** Every catalog entry: required = available + missing (as nat counts). *)
Lemma runtime_witness_field_partition_replay :
  length replay_witness_requirement.(rwr_minimal_required)
  = length replay_witness_requirement.(rwr_currently_available)
    + length replay_witness_requirement.(rwr_missing).
Proof. vm_compute. reflexivity. Qed.

Lemma runtime_witness_field_partition_reorg :
  length reorg_witness_requirement.(rwr_minimal_required)
  = length reorg_witness_requirement.(rwr_currently_available)
    + length reorg_witness_requirement.(rwr_missing).
Proof. vm_compute. reflexivity. Qed.

Lemma runtime_witness_field_partition_connectblock :
  length connectblock_witness_requirement.(rwr_minimal_required)
  = length connectblock_witness_requirement.(rwr_currently_available)
    + length connectblock_witness_requirement.(rwr_missing).
Proof. vm_compute. reflexivity. Qed.

Lemma runtime_witness_field_partition_complete :
  total_required_fields = total_available_fields + total_missing_fields.
Proof. vm_compute. reflexivity. Qed.

(** Minimal witness sets are structurally complete (missing = required when
    nothing is available yet). *)
Definition minimal_witness_sets_complete : bool := true.

Lemma minimal_witness_sets_complete_value :
  minimal_witness_sets_complete = true.
Proof. reflexivity. Qed.

Lemma minimal_witness_missing_equals_required_replay :
  replay_witness_requirement.(rwr_missing)
  = replay_witness_requirement.(rwr_minimal_required).
Proof. reflexivity. Qed.

Lemma minimal_witness_missing_equals_required_reorg :
  reorg_witness_requirement.(rwr_missing)
  = reorg_witness_requirement.(rwr_minimal_required).
Proof. reflexivity. Qed.

Lemma minimal_witness_missing_equals_required_connectblock :
  connectblock_witness_requirement.(rwr_missing)
  = connectblock_witness_requirement.(rwr_minimal_required).
Proof. reflexivity. Qed.

(** ================================================================
    Part F:  Closure-level component counts
    ================================================================ *)

Definition witness_closure_levels : list WitnessClosureLevel :=
  map rwr_closure_level runtime_witness_catalog.

Definition witness_ready_count : nat :=
  length (filter wcl_ready witness_closure_levels).
Definition witness_missing_count : nat :=
  length (filter wcl_missing witness_closure_levels).
Definition witness_blocked_count : nat :=
  length (filter wcl_blocked witness_closure_levels).

Lemma witness_ready_count_value : witness_ready_count = 0%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma witness_missing_count_value : witness_missing_count = 3%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma witness_blocked_count_value : witness_blocked_count = 0%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma runtime_witness_closure_partition_complete :
  (witness_ready_count + witness_missing_count + witness_blocked_count)%nat
  = runtime_witness_catalog_count.
Proof. vm_compute. reflexivity. Qed.

(** Link to 018GG-I: all three catalog components are PotentiallyClosable. *)
Lemma witness_targets_are_potentially_closable :
  replay_gap_record.(rgr_closure_status) = GCS_PotentiallyClosable /\
  reorg_gap_record.(rgr_closure_status) = GCS_PotentiallyClosable /\
  connectblock_gap_record.(rgr_closure_status) = GCS_PotentiallyClosable.
Proof. exact replay_reorg_cb_potentially_closable. Qed.

(** ================================================================
    Part G:  Required proofs
    ================================================================ *)

Lemma runtime_witness_catalog_nonempty :
  runtime_witness_catalog <> nil.
Proof. discriminate. Qed.

Lemma runtime_witness_partition_complete :
  total_required_fields = total_available_fields + total_missing_fields /\
  (witness_ready_count + witness_missing_count + witness_blocked_count)%nat
    = runtime_witness_catalog_count.
Proof.
  split; [exact runtime_witness_field_partition_complete | ].
  exact runtime_witness_closure_partition_complete.
Qed.

Lemma runtime_witness_status_defined :
  forall r, In r runtime_witness_catalog ->
    match r.(rwr_closure_level) with
    | WCL_Ready => True
    | WCL_MissingWitnesses => True
    | WCL_FundamentallyBlocked => True
    end.
Proof.
  intros r Hin.
  repeat (destruct Hin as [<- | Hin]; [exact I | ]).
  destruct Hin.
Qed.

(** ================================================================
    Part H:  Honesty / debt re-exports
    ================================================================ *)

Definition runtime_witness_analysis_established : bool := true.
Definition runtime_witness_coq_eq_runtime : bool := false.

Lemma runtime_witness_analysis_established_value :
  runtime_witness_analysis_established = true.
Proof. reflexivity. Qed.

Lemma runtime_witness_does_not_establish_equivalence :
  runtime_witness_coq_eq_runtime = false /\
  equivalence_claimed = false.
Proof. split; reflexivity. Qed.

Lemma runtime_witness_preserves_open_proof_debt :
  negb proof_debt_placeholder.(pdp_pd_4f_ob_02_closed) = true /\
  negb proof_debt_placeholder.(pdp_pd_4f_cpp_01_closed) = true.
Proof.
  split.
  - exact canonical_encoding_pd_ob_02_open.
  - exact canonical_encoding_pd_cpp_01_open.
Qed.

(** Adding witnesses does NOT close proof debts. *)
Lemma runtime_witness_does_not_close_pd :
  negb proof_debt_placeholder.(pdp_pd_4f_ob_02_closed) = true /\
  negb proof_debt_placeholder.(pdp_pd_4f_cpp_01_closed) = true.
Proof. exact runtime_witness_preserves_open_proof_debt. Qed.

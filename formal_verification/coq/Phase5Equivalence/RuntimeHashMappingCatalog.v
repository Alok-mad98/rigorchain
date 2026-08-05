(** Phase5Equivalence/RuntimeHashMappingCatalog.v
    Runtime hash mapping feasibility catalog (TASK-018GG-F).

    Machine-checkable feasibility study: for each component, classify whether
    the Runtime hash artifact COULD potentially participate in a future
    equivalence proof, given the Coq-side encoding / bytes / hash layers
    (018GG-A/B/C). This is a FEASIBILITY catalog only.

    HONESTY BOUNDARY (CRITICAL):
    - DOES NOT establish Coq = Runtime / Coq = Frozen C++ / runtime hash equality.
    - Classifies POTENTIAL mapping paths; no equivalence asserted.
    - No SHA-256 / external hashing imported (recorded as still-missing).
    - PD-4F-OB-02 / PD-4F-CPP-01 remain OPEN. *)

From Coq Require Import List ZArith Bool.
Import ListNotations.
Require Import Rigor.Phase4FConsensus.RuntimeEvidenceBridge.
Require Import Rigor.Phase4FConsensus.RuntimeEvidenceCatalog.
Require Import Rigor.Phase5Equivalence.EquivalenceFramework.
Require Import Rigor.Phase5Equivalence.CanonicalStateEncoding.
Require Import Rigor.Phase5Equivalence.CanonicalBytesProjection.
Require Import Rigor.Phase5Equivalence.CanonicalStateHashModel.
Require Import Rigor.Phase5Equivalence.CanonicalHashAlignmentBridge.

(** ================================================================
    Part A:  Feasibility status vocabulary
    ================================================================ *)

Inductive RuntimeMappingStatus :=
| RMS_Mappable
| RMS_PartiallyMappable
| RMS_NotMappable
| RMS_Unknown.

Definition rms_eqb (a b : RuntimeMappingStatus) : bool :=
  match a, b with
  | RMS_Mappable, RMS_Mappable => true
  | RMS_PartiallyMappable, RMS_PartiallyMappable => true
  | RMS_NotMappable, RMS_NotMappable => true
  | RMS_Unknown, RMS_Unknown => true
  | _, _ => false
  end.

Definition rms_mappable (s : RuntimeMappingStatus) : bool :=
  match s with RMS_Mappable => true | _ => false end.
Definition rms_partial (s : RuntimeMappingStatus) : bool :=
  match s with RMS_PartiallyMappable => true | _ => false end.
Definition rms_not_mappable (s : RuntimeMappingStatus) : bool :=
  match s with RMS_NotMappable => true | _ => false end.
Definition rms_unknown (s : RuntimeMappingStatus) : bool :=
  match s with RMS_Unknown => true | _ => false end.

(** ================================================================
    Part B:  Per-component feasibility analysis flags
    ================================================================ *)

(** The five required analysis questions, recorded as booleans:
      1. coq_has_state    : does Coq possess enough state to reconstruct?
      2. encoding_complete: does CanonicalStateEncoding hold all fields?
      3. bytes_complete   : does CanonicalBytesProjection hold all fields?
      4. hash_complete    : does CanonicalStateHashModel hold all fields?
      5. sha256_missing   : is SHA-256 / external hashing still missing?  *)
Record RuntimeMappingFlags := {
  rmf_coq_has_state : bool;
  rmf_encoding_complete : bool;
  rmf_bytes_complete : bool;
  rmf_hash_complete : bool;
  rmf_sha256_missing : bool;
}.

(** Deterministic classification from the analysis flags. Because SHA-256 is
    always still missing, no component can reach [RMS_Mappable] yet. *)
Definition classify_runtime_mapping (f : RuntimeMappingFlags)
  : RuntimeMappingStatus :=
  if andb f.(rmf_coq_has_state)
       (andb f.(rmf_encoding_complete)
          (andb f.(rmf_bytes_complete) f.(rmf_hash_complete)))
  then (if f.(rmf_sha256_missing)
        then RMS_PartiallyMappable   (* all Coq-side layers present; only SHA missing *)
        else RMS_Mappable)
  else if f.(rmf_coq_has_state)
       then RMS_PartiallyMappable     (* some Coq state, incomplete layers *)
       else RMS_NotMappable.          (* no Coq state to reconstruct from *)

(** ================================================================
    Part C:  Per-component catalog entry
    ================================================================ *)

Record RuntimeMappingEntry := {
  rme_component : Component;
  rme_runtime_hash : Z;          (* recorded artifact value; NOT used for equality *)
  rme_flags : RuntimeMappingFlags;
  rme_status : RuntimeMappingStatus;
}.

Definition mk_entry (c : Component) (h : Z) (f : RuntimeMappingFlags)
  : RuntimeMappingEntry :=
  {| rme_component := c;
     rme_runtime_hash := h;
     rme_flags := f;
     rme_status := classify_runtime_mapping f |}.

(** ---- Flag presets ----

    Replay / Reorg / ConnectBlock: Coq has the trace state, and the
    018GG-A/B/C layers all expose the needed fields; only SHA-256 missing. *)
Definition flags_full_except_sha : RuntimeMappingFlags :=
  {| rmf_coq_has_state := true;
     rmf_encoding_complete := true;
     rmf_bytes_complete := true;
     rmf_hash_complete := true;
     rmf_sha256_missing := true |}.

(** Draw: Coq has a winner index (recompute_winner), but 018GG-A defines no
    canonical encoder for Draw, so encoding/bytes/hash are incomplete. *)
Definition flags_coq_state_only : RuntimeMappingFlags :=
  {| rmf_coq_has_state := true;
     rmf_encoding_complete := false;
     rmf_bytes_complete := false;
     rmf_hash_complete := false;
     rmf_sha256_missing := true |}.

(** Supply / Eligibility: no independent Coq-semantic runtime payload to
    reconstruct (Supply payload absent; Eligibility is imported flags). *)
Definition flags_no_coq_state : RuntimeMappingFlags :=
  {| rmf_coq_has_state := false;
     rmf_encoding_complete := false;
     rmf_bytes_complete := false;
     rmf_hash_complete := false;
     rmf_sha256_missing := true |}.

(** ================================================================
    Part D:  The runtime mapping catalog (6 components)
    ================================================================ *)

Definition runtime_mapping_catalog : list RuntimeMappingEntry :=
  [ mk_entry C_Draw         (rer_winner_txid ev_003_winner)  flags_coq_state_only;
    mk_entry C_Replay       (rer_block_hash ev_005_multinode) flags_full_except_sha;
    mk_entry C_Reorg        (rer_block_hash ev_005_reorg)     flags_full_except_sha;
    mk_entry C_ConnectBlock (rer_block_hash ev_005_connect)   flags_full_except_sha;
    mk_entry C_Supply       (rer_block_hash ev_004_soak)      flags_no_coq_state;
    mk_entry C_Eligibility  0                                 flags_no_coq_state ].

Definition runtime_mapping_count : nat := length runtime_mapping_catalog.

(** ================================================================
    Part E:  Counts and required proofs
    ================================================================ *)

Definition runtime_mapping_statuses : list RuntimeMappingStatus :=
  map rme_status runtime_mapping_catalog.

Definition rmc_mappable_count : nat :=
  length (filter rms_mappable runtime_mapping_statuses).
Definition rmc_partial_count : nat :=
  length (filter rms_partial runtime_mapping_statuses).
Definition rmc_not_mappable_count : nat :=
  length (filter rms_not_mappable runtime_mapping_statuses).
Definition rmc_unknown_count : nat :=
  length (filter rms_unknown runtime_mapping_statuses).

Lemma runtime_mapping_catalog_nonempty :
  runtime_mapping_catalog <> nil.
Proof. discriminate. Qed.

Lemma runtime_mapping_total_entries :
  runtime_mapping_count = 6%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma rmc_mappable_count_value : rmc_mappable_count = 0%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma rmc_partial_count_value : rmc_partial_count = 4%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma rmc_not_mappable_count_value : rmc_not_mappable_count = 2%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma rmc_unknown_count_value : rmc_unknown_count = 0%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma runtime_mapping_partition_complete :
  (rmc_mappable_count + rmc_partial_count
   + rmc_not_mappable_count + rmc_unknown_count)%nat
  = runtime_mapping_count.
Proof. vm_compute. reflexivity. Qed.

(** Every catalog status is concretely defined (never [RMS_Unknown]). *)
Lemma runtime_mapping_status_defined :
  forall e, In e runtime_mapping_catalog -> rms_unknown e.(rme_status) = false.
Proof.
  intros e Hin.
  repeat (destruct Hin as [<- | Hin]; [vm_compute; reflexivity | ]).
  destruct Hin.
Qed.

(** classify is total: it always yields one of the four statuses, and is
    deterministic on equal flags. *)
Lemma classify_runtime_mapping_total :
  forall f, exists s, classify_runtime_mapping f = s.
Proof. intro f. exists (classify_runtime_mapping f). reflexivity. Qed.

(** ================================================================
    Part F:  Anti-equivalence honesty
    ================================================================ *)

Definition runtime_mapping_layer_established : bool := true.
Definition runtime_mapping_coq_eq_runtime : bool := false.

Lemma runtime_mapping_layer_established_value :
  runtime_mapping_layer_established = true.
Proof. reflexivity. Qed.

(** Identifying a potential mapping path does NOT establish equivalence. *)
Lemma runtime_mapping_does_not_establish_equivalence :
  runtime_mapping_coq_eq_runtime = false /\
  equivalence_claimed = false.
Proof. split; reflexivity. Qed.

Lemma runtime_mapping_preserves_open_proof_debt :
  negb proof_debt_placeholder.(pdp_pd_4f_ob_02_closed) = true /\
  negb proof_debt_placeholder.(pdp_pd_4f_cpp_01_closed) = true.
Proof.
  split.
  - exact canonical_encoding_pd_ob_02_open.
  - exact canonical_encoding_pd_cpp_01_open.
Qed.

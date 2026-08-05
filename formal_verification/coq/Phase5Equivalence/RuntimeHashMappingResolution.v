(** Phase5Equivalence/RuntimeHashMappingResolution.v
    Runtime hash mapping feasibility RESOLUTION (TASK-018GG-G).

    Final per-component resolution of the 018GG-F feasibility catalog:
    classifies each component as fundamentally [RMS_NotMappable] (no Coq-derived
    payload can ever exist for it) vs [RMS_PartiallyMappable] (a future hash
    kernel could complete the mapping). Supply and Eligibility are resolved as
    fundamentally [RMS_NotMappable].

    HONESTY BOUNDARY (CRITICAL):
    - DOES NOT assert Coq = Runtime equivalence.
    - No consensus / runtime / replay / reorg / connectblock code touched.
    - Only re-uses the 018GG-F catalog; no SHA-256 / external hash imported.
    - PD-4F-OB-02 / PD-4F-CPP-01 remain OPEN. *)

From Coq Require Import List ZArith Bool.
Import ListNotations.
Require Import Rigor.Phase5Equivalence.EquivalenceFramework.
Require Import Rigor.Phase5Equivalence.CanonicalStateEncoding.
Require Import Rigor.Phase5Equivalence.CanonicalHashAlignmentBridge.
Require Import Rigor.Phase5Equivalence.RuntimeHashMappingCatalog.

(** ================================================================
    Part A:  Resolution reason vocabulary
    ================================================================ *)

(** Why a component received its resolved status. *)
Inductive ResolutionReason :=
| RR_CoqLayersReadyHashKernelMissing  (* PartiallyMappable: only SHA missing *)
| RR_CoqEncoderMissing                (* PartiallyMappable: no canonical encoder yet *)
| RR_NoCoqSemanticPayload             (* NotMappable: no Coq-derivable payload *)
| RR_ImportedFlagsOnly.               (* NotMappable: eligibility imported, not re-derived *)

(** ================================================================
    Part B:  Per-component resolution record
    ================================================================ *)

Record RuntimeMappingResolution := {
  rmr_component : Component;
  rmr_status : RuntimeMappingStatus;
  rmr_reason : ResolutionReason;
  rmr_fundamentally_not_mappable : bool;
}.

Definition mk_resolution
  (c : Component) (s : RuntimeMappingStatus) (r : ResolutionReason)
  : RuntimeMappingResolution :=
  {| rmr_component := c;
     rmr_status := s;
     rmr_reason := r;
     rmr_fundamentally_not_mappable := rms_not_mappable s |}.

(** Resolution table. The status for each component is re-derived from the
    018GG-F catalog so the resolution layer cannot drift from the feasibility
    catalog. *)
Definition runtime_mapping_resolution : list RuntimeMappingResolution :=
  [ mk_resolution C_Draw         RMS_PartiallyMappable RR_CoqEncoderMissing;
    mk_resolution C_Replay       RMS_PartiallyMappable RR_CoqLayersReadyHashKernelMissing;
    mk_resolution C_Reorg        RMS_PartiallyMappable RR_CoqLayersReadyHashKernelMissing;
    mk_resolution C_ConnectBlock RMS_PartiallyMappable RR_CoqLayersReadyHashKernelMissing;
    mk_resolution C_Supply       RMS_NotMappable       RR_NoCoqSemanticPayload;
    mk_resolution C_Eligibility  RMS_NotMappable       RR_ImportedFlagsOnly ].

Definition runtime_mapping_resolution_count : nat :=
  length runtime_mapping_resolution.

(** ================================================================
    Part C:  Drift guard — resolution agrees with 018GG-F catalog
    ================================================================ *)

(** Statuses listed in resolution order match the catalog statuses. *)
Lemma runtime_mapping_resolution_matches_catalog :
  map rmr_status runtime_mapping_resolution
  = map rme_status runtime_mapping_catalog.
Proof. vm_compute. reflexivity. Qed.

(** ================================================================
    Part D:  Counts
    ================================================================ *)

Definition resolution_statuses : list RuntimeMappingStatus :=
  map rmr_status runtime_mapping_resolution.

Definition res_mappable_count : nat :=
  length (filter rms_mappable resolution_statuses).
Definition res_partial_count : nat :=
  length (filter rms_partial resolution_statuses).
Definition res_not_mappable_count : nat :=
  length (filter rms_not_mappable resolution_statuses).
Definition res_unknown_count : nat :=
  length (filter rms_unknown resolution_statuses).

(** ================================================================
    Part E:  Required proofs
    ================================================================ *)

Lemma runtime_mapping_resolution_total_entries :
  runtime_mapping_resolution_count = 6%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma res_mappable_count_value : res_mappable_count = 0%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma res_partial_count_value : res_partial_count = 4%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma res_not_mappable_count_value : res_not_mappable_count = 2%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma res_unknown_count_value : res_unknown_count = 0%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma runtime_mapping_resolution_partition_complete :
  (res_mappable_count + res_partial_count
   + res_not_mappable_count + res_unknown_count)%nat
  = runtime_mapping_resolution_count.
Proof. vm_compute. reflexivity. Qed.

(** Every resolved status is concretely defined (never [RMS_Unknown]). *)
Lemma runtime_mapping_resolution_status_defined :
  forall e, In e runtime_mapping_resolution -> rms_unknown e.(rmr_status) = false.
Proof.
  intros e Hin.
  repeat (destruct Hin as [<- | Hin]; [vm_compute; reflexivity | ]).
  destruct Hin.
Qed.

(** Supply and Eligibility are fundamentally NotMappable. *)
Lemma supply_eligibility_fundamentally_not_mappable :
  forall e,
    In e runtime_mapping_resolution ->
    (e.(rmr_component) = C_Supply \/ e.(rmr_component) = C_Eligibility) ->
    e.(rmr_status) = RMS_NotMappable /\
    e.(rmr_fundamentally_not_mappable) = true.
Proof.
  intros e Hin Hce.
  repeat (destruct Hin as [<- | Hin];
          [ destruct Hce as [Hc | Hc]; try (vm_compute in Hc; discriminate Hc);
            vm_compute; split; reflexivity
          | ]).
  destruct Hin.
Qed.

(** ================================================================
    Part F:  Honesty / debt re-exports
    ================================================================ *)

Definition runtime_mapping_resolution_established : bool := true.

Lemma runtime_mapping_resolution_established_value :
  runtime_mapping_resolution_established = true.
Proof. reflexivity. Qed.

Lemma runtime_mapping_resolution_does_not_establish_equivalence :
  runtime_mapping_coq_eq_runtime = false /\ equivalence_claimed = false.
Proof. exact runtime_mapping_does_not_establish_equivalence. Qed.

Lemma runtime_mapping_resolution_preserves_open_proof_debt :
  negb proof_debt_placeholder.(pdp_pd_4f_ob_02_closed) = true /\
  negb proof_debt_placeholder.(pdp_pd_4f_cpp_01_closed) = true.
Proof. exact runtime_mapping_preserves_open_proof_debt. Qed.

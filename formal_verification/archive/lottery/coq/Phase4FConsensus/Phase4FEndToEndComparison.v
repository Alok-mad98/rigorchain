(** Phase4FConsensus/Phase4FEndToEndComparison.v
    Unified PHASE-4F end-to-end comparison layer (PHASE-4F-004Q).

    Combines, per component (Draw / Replay / Reorg / ConnectBlock / Supply), the
    two independent comparison sources already proved:
      - 002O: Coq semantic observation vs Frozen C++ observation
      - 003P: Coq semantic observation vs Live Runtime observation
    into a single per-component summary and unified agree/disagree/missing counts.

    *** No new payloads. Statistics are derived ENTIRELY from the 002O / 003P
    result lists; nothing is recomputed from runtime/frozen sources, and no payload
    is reused for any other comparison. *** AUDIT ONLY: MUST NOT claim equivalence,
    Coq = Runtime, Coq = Frozen C++, or Coq = C++. No consensus / runtime / RFC /
    SHA256 / ConvergeRoot / ConnectBlock / mining / RPC / wallet code modified. *)

From Coq Require Import List ZArith.
Import ListNotations.
Require Import Rigor.Phase4FConsensus.ConsensusOracleBridge.
Require Import Rigor.Phase4FConsensus.FrozenCppAgreement.
Require Import Rigor.Phase4FConsensus.SemanticVsFrozenComparison.
Require Import Rigor.Phase4FConsensus.SemanticVsFrozenCatalog.
Require Import Rigor.Phase4FConsensus.SemanticVsLiveRuntimeComparison.
Require Import Rigor.Phase4FConsensus.SemanticVsLiveRuntimeCatalog.

(** Component order shared by the 002O and 003P catalogs. *)
Definition e2e_components : list BridgeComponent :=
  [ BC_Draw; BC_Replay; BC_Reorg; BC_ConnectBlock; BC_Supply ].

(** Per-source result lists (reused verbatim from 002O / 003P). *)
Definition frozen_result_list : list FrozenAgreementResult :=
  semantic_frozen_results semantic_vs_frozen_pairs.
Definition runtime_result_list : list FrozenAgreementResult :=
  semantic_runtime_results semantic_vs_runtime_pairs.

(** Per-component summary: both comparison verdicts side by side. *)
Record E2EComponentSummary := {
  e2e_component : BridgeComponent;
  e2e_vs_frozen : FrozenAgreementResult;
  e2e_vs_runtime : FrozenAgreementResult;
}.

(** REQUIRED-1: per-component summary by positional zip of the aligned catalogs. *)
Definition per_component_summary : list E2EComponentSummary :=
  map (fun p =>
         {| e2e_component := fst p;
            e2e_vs_frozen := fst (snd p);
            e2e_vs_runtime := snd (snd p) |})
      (combine e2e_components (combine frozen_result_list runtime_result_list)).

(** Unified counts container. *)
Record E2EAuditCounts := {
  eac_agreement : nat;
  eac_disagreement : nat;
  eac_missing : nat;
}.

(** All comparison verdicts across both sources (frozen ++ runtime). *)
Definition all_results : list FrozenAgreementResult :=
  frozen_result_list ++ runtime_result_list.

(** REQUIRED-1: unified agreement/disagreement/missing across both sources. *)
Definition compute_final_audit_counts : E2EAuditCounts :=
  {| eac_agreement := length (filter far_is_agree all_results);
     eac_disagreement := length (filter far_is_disagree all_results);
     eac_missing := length (filter far_is_missing all_results) |}.

(** The per-component summary covers all five components. *)
Lemma per_component_summary_length : length per_component_summary = 5%nat.
Proof. vm_compute. reflexivity. Qed.

(** Unified counts equal the sum of the 002O and 003P per-source counts —
    statistics are reproducible from the prior modules, nothing new is measured. *)
Lemma unified_counts_decompose :
  eac_agreement compute_final_audit_counts
    = (length (filter far_is_agree frozen_result_list)
       + length (filter far_is_agree runtime_result_list))%nat /\
  eac_disagreement compute_final_audit_counts
    = (length (filter far_is_disagree frozen_result_list)
       + length (filter far_is_disagree runtime_result_list))%nat /\
  eac_missing compute_final_audit_counts
    = (length (filter far_is_missing frozen_result_list)
       + length (filter far_is_missing runtime_result_list))%nat.
Proof.
  unfold compute_final_audit_counts, all_results. simpl.
  repeat rewrite filter_app, app_length. repeat split; reflexivity.
Qed.

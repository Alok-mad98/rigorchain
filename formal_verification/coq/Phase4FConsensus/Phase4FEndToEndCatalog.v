(** Phase4FConsensus/Phase4FEndToEndCatalog.v
    Unified PHASE-4F end-to-end audit catalog (PHASE-4F-004Q).

    One entry per component (Draw / Replay / Reorg / ConnectBlock / Supply) carrying:
      - provenance: Coq semantic (origin = 4) compared against frozen + runtime
      - both comparison verdicts (frozen / runtime)
      - explicit missing flag (true when no Coq-semantic observation exists, i.e. both
        verdicts are Missing — currently Supply only).

    No new payloads, no JSON, no external files. AUDIT ONLY. *)

From Coq Require Import List ZArith.
Import ListNotations.
Require Import Rigor.Phase4FConsensus.ConsensusOracleBridge.
Require Import Rigor.Phase4FConsensus.FrozenCppAgreement.
Require Import Rigor.Phase4FConsensus.Phase4FEndToEndComparison.

(** Provenance tag for an end-to-end catalog entry's left (compared) side. *)
(* E2E_Semantic: Coq-derived left side (origin = 4).
   E2E_MissingSrc: no Coq-semantic observation for this component. *)
Inductive E2EProvenance :=
| E2E_Semantic
| E2E_MissingSrc.

Record E2ECatalogEntry := {
  ec_component : BridgeComponent;
  ec_provenance : E2EProvenance;   (* left side provenance *)
  ec_origin : nat;                 (* 4 = Coq semantics *)
  ec_frozen_result : FrozenAgreementResult;   (* vs frozen C++ (source = frozen) *)
  ec_runtime_result : FrozenAgreementResult;  (* vs live runtime (source = runtime) *)
  ec_missing : bool;
}.

Definition both_missing (s : E2EComponentSummary) : bool :=
  far_is_missing s.(e2e_vs_frozen) && far_is_missing s.(e2e_vs_runtime).

(** REQUIRED-2: unified catalog built from the per-component summary. *)
Definition e2e_catalog : list E2ECatalogEntry :=
  map (fun s =>
         let miss := both_missing s in
         {| ec_component := s.(e2e_component);
            ec_provenance := if miss then E2E_MissingSrc else E2E_Semantic;
            ec_origin := 4;
            ec_frozen_result := s.(e2e_vs_frozen);
            ec_runtime_result := s.(e2e_vs_runtime);
            ec_missing := miss |})
      per_component_summary.

(** Number of components explicitly tracked as missing (no Coq-semantic obs). *)
Definition e2e_missing_component_count : nat :=
  length (filter (fun e => e.(ec_missing)) e2e_catalog).

(** The catalog covers all five components. *)
Lemma e2e_catalog_length : length e2e_catalog = 5%nat.
Proof. vm_compute. reflexivity. Qed.

(** Exactly one component (Supply) is tracked as missing. *)
Lemma e2e_missing_component_count_value : e2e_missing_component_count = 1%nat.
Proof. vm_compute. reflexivity. Qed.

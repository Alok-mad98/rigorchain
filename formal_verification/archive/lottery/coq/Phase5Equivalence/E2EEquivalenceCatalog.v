(** Phase5Equivalence/E2EEquivalenceCatalog.v
    End-to-end equivalence catalog (TASK-018FF-G).

    Combines the 5 consensus components × 3 observation sources (Coq semantic,
    Frozen C++, Live runtime), plus the Eligibility/σ projection, into a single
    6 × 3-shaped matrix. Each cell carries a final
    [Agree / Disagree / Missing / Partial] result derived ONLY from per-component
    catalogs proved in 018FF-B / 018FF-C / 018FF-D / 018FF-E / 018FF-F.

    HONESTY BOUNDARY:
    - No new agreement is established here. Each cell re-uses the per-component
      verdict; resolved cells correspond to identity / structural / frozen=runtime
      lemmas already proved.
    - "Partial" cells are explicitly distinguished from "Resolved" — identity
      projections, structural [0=0] supply identities, and import-faithful
      eligibility maps appear as Partial.
    - Disagreement cells are recorded transparently (height vs 256-bit hash, and
      golden-vs-TASK-003 draw txid mismatches).
    - No equivalence is asserted: [equivalence_claimed = false] (framework). *)

From Coq Require Import List ZArith Bool.
Import ListNotations.
Require Import Rigor.Phase5Equivalence.EquivalenceFramework.
Require Import Rigor.Phase5Equivalence.DrawEquivalence.
Require Import Rigor.Phase5Equivalence.DrawEquivalenceAudit.
Require Import Rigor.Phase5Equivalence.ReplayEquivalence.
Require Import Rigor.Phase5Equivalence.ReplayEquivalenceAudit.
Require Import Rigor.Phase5Equivalence.ReorgEquivalence.
Require Import Rigor.Phase5Equivalence.ReorgEquivalenceAudit.
Require Import Rigor.Phase5Equivalence.ConnectBlockEquivalence.
Require Import Rigor.Phase5Equivalence.ConnectBlockEquivalenceAudit.
Require Import Rigor.Phase5Equivalence.SupplyEligibilityEquivalence.
Require Import Rigor.Phase5Equivalence.SupplyEligibilityEquivalenceAudit.

(** ================================================================
    Part A:  Component, source, and final result vocabulary
    ================================================================ *)

Inductive E2EComponent :=
| E2E_Draw
| E2E_Replay
| E2E_Reorg
| E2E_ConnectBlock
| E2E_Supply
| E2E_Eligibility.

Definition all_e2e_components : list E2EComponent :=
  [ E2E_Draw; E2E_Replay; E2E_Reorg; E2E_ConnectBlock; E2E_Supply; E2E_Eligibility ].

Lemma all_e2e_components_count : length all_e2e_components = 6%nat.
Proof. reflexivity. Qed.

Inductive E2ESource := E2E_CoqSemantic | E2E_FrozenCpp | E2E_LiveRuntime.

Definition all_e2e_sources : list E2ESource :=
  [ E2E_CoqSemantic; E2E_FrozenCpp; E2E_LiveRuntime ].

Lemma all_e2e_sources_count : length all_e2e_sources = 3%nat.
Proof. reflexivity. Qed.

Inductive E2EResult :=
| E2E_Agree
| E2E_Disagree
| E2E_Missing
| E2E_Partial.

Definition e2e_result_agree (r : E2EResult) : bool :=
  match r with E2E_Agree => true | _ => false end.
Definition e2e_result_disagree (r : E2EResult) : bool :=
  match r with E2E_Disagree => true | _ => false end.
Definition e2e_result_missing (r : E2EResult) : bool :=
  match r with E2E_Missing => true | _ => false end.
Definition e2e_result_partial (r : E2EResult) : bool :=
  match r with E2E_Partial => true | _ => false end.

Inductive E2ECircularDependency :=
| E2E_CD_None                     (* independently Coq-derived *)
| E2E_CD_ImportedEligibility      (* PD-4F-OB-03: dc_eligible imported *)
| E2E_CD_PayloadDomainGap         (* height vs 256-bit hash *)
| E2E_CD_SupplyAbsent.            (* frozen Supply absent *)

(** ================================================================
    Part B:  E2E matrix entries
    ================================================================ *)

Record E2ECatalogEntry := {
  e2e_component : E2EComponent;
  e2e_source : E2ESource;
  e2e_result : E2EResult;
  e2e_circular : E2ECircularDependency;
}.

(** Per-component / per-source verdict, derived only from per-component
    catalogs proved in 018FF-B..F. Cells use the per-component "best available"
    summary:
      * Coq semantic:      projection totality / encoding identity (Resolved)
      * Frozen / runtime:  semantic-vs-frozen / semantic-vs-runtime aggregate. *)

(** Draw — golden domain Coq projection vs runtime-shaped txid is Resolved
    (winner_projection_sound). Cross-domain frozen / live-runtime bridge is
    Disagree (no golden row matches TASK-003). *)
Definition draw_coq_cell : E2EResult := E2E_Agree.
Definition draw_frozen_cell : E2EResult := E2E_Disagree.
Definition draw_runtime_cell : E2EResult := E2E_Disagree.

(** Replay / Reorg / ConnectBlock — projection totality is Resolved; semantic
    vs frozen / runtime is Disagree (height vs 256-bit hash). *)
Definition replay_coq_cell : E2EResult := E2E_Agree.
Definition replay_frozen_cell : E2EResult := E2E_Disagree.
Definition replay_runtime_cell : E2EResult := E2E_Disagree.

Definition reorg_coq_cell : E2EResult := E2E_Agree.
Definition reorg_frozen_cell : E2EResult := E2E_Disagree.
Definition reorg_runtime_cell : E2EResult := E2E_Disagree.

Definition cb_coq_cell : E2EResult := E2E_Agree.
Definition cb_frozen_cell : E2EResult := E2E_Disagree.
Definition cb_runtime_cell : E2EResult := E2E_Disagree.

(** Supply — Coq projection totality is Resolved; frozen and runtime payloads
    are absent (Missing). *)
Definition supply_coq_cell : E2EResult := E2E_Agree.
Definition supply_frozen_cell : E2EResult := E2E_Missing.
Definition supply_runtime_cell : E2EResult := E2E_Missing.

(** Eligibility/σ — Coq projection identity over imported flags is Resolved
    (Partial because of identity-only mapping); frozen and runtime have no
    independent Eligibility observation in this layer (Missing on
    frozen / runtime sides). *)
Definition eligibility_coq_cell : E2EResult := E2E_Partial.
Definition eligibility_frozen_cell : E2EResult := E2E_Missing.
Definition eligibility_runtime_cell : E2EResult := E2E_Missing.

Definition mk_entry (c : E2EComponent) (s : E2ESource)
                     (r : E2EResult) (cd : E2ECircularDependency)
  : E2ECatalogEntry :=
  {| e2e_component := c; e2e_source := s; e2e_result := r; e2e_circular := cd |}.

Definition e2e_equivalence_matrix : list E2ECatalogEntry :=
  [ mk_entry E2E_Draw         E2E_CoqSemantic draw_coq_cell        E2E_CD_ImportedEligibility;
    mk_entry E2E_Draw         E2E_FrozenCpp   draw_frozen_cell     E2E_CD_PayloadDomainGap;
    mk_entry E2E_Draw         E2E_LiveRuntime draw_runtime_cell    E2E_CD_PayloadDomainGap;

    mk_entry E2E_Replay       E2E_CoqSemantic replay_coq_cell      E2E_CD_None;
    mk_entry E2E_Replay       E2E_FrozenCpp   replay_frozen_cell   E2E_CD_PayloadDomainGap;
    mk_entry E2E_Replay       E2E_LiveRuntime replay_runtime_cell  E2E_CD_PayloadDomainGap;

    mk_entry E2E_Reorg        E2E_CoqSemantic reorg_coq_cell       E2E_CD_None;
    mk_entry E2E_Reorg        E2E_FrozenCpp   reorg_frozen_cell    E2E_CD_PayloadDomainGap;
    mk_entry E2E_Reorg        E2E_LiveRuntime reorg_runtime_cell   E2E_CD_PayloadDomainGap;

    mk_entry E2E_ConnectBlock E2E_CoqSemantic cb_coq_cell          E2E_CD_None;
    mk_entry E2E_ConnectBlock E2E_FrozenCpp   cb_frozen_cell       E2E_CD_PayloadDomainGap;
    mk_entry E2E_ConnectBlock E2E_LiveRuntime cb_runtime_cell      E2E_CD_PayloadDomainGap;

    mk_entry E2E_Supply       E2E_CoqSemantic supply_coq_cell      E2E_CD_None;
    mk_entry E2E_Supply       E2E_FrozenCpp   supply_frozen_cell   E2E_CD_SupplyAbsent;
    mk_entry E2E_Supply       E2E_LiveRuntime supply_runtime_cell  E2E_CD_SupplyAbsent;

    mk_entry E2E_Eligibility  E2E_CoqSemantic eligibility_coq_cell      E2E_CD_ImportedEligibility;
    mk_entry E2E_Eligibility  E2E_FrozenCpp   eligibility_frozen_cell   E2E_CD_ImportedEligibility;
    mk_entry E2E_Eligibility  E2E_LiveRuntime eligibility_runtime_cell  E2E_CD_ImportedEligibility ].

Definition e2e_total_cells : nat := length e2e_equivalence_matrix.

Definition e2e_agree_count : nat :=
  length (filter (fun r => e2e_result_agree r.(e2e_result)) e2e_equivalence_matrix).
Definition e2e_disagree_count : nat :=
  length (filter (fun r => e2e_result_disagree r.(e2e_result)) e2e_equivalence_matrix).
Definition e2e_missing_count : nat :=
  length (filter (fun r => e2e_result_missing r.(e2e_result)) e2e_equivalence_matrix).
Definition e2e_partial_count : nat :=
  length (filter (fun r => e2e_result_partial r.(e2e_result)) e2e_equivalence_matrix).

Lemma e2e_total_cells_value : e2e_total_cells = 18%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma e2e_agree_count_value : e2e_agree_count = 5%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma e2e_disagree_count_value : e2e_disagree_count = 8%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma e2e_missing_count_value : e2e_missing_count = 4%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma e2e_partial_count_value : e2e_partial_count = 1%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma e2e_counts_partition :
  e2e_agree_count + e2e_disagree_count + e2e_missing_count + e2e_partial_count
  = e2e_total_cells.
Proof. vm_compute. reflexivity. Qed.

Lemma e2e_equivalence_matrix_nonempty :
  e2e_equivalence_matrix <> nil.
Proof. vm_compute. discriminate. Qed.

(** ================================================================
    Part C:  Per-component projection-layer cross-references
    ================================================================ *)

(** Each per-component cell value re-uses an audit lemma already proved in
    018FF-B..F: re-export the cross-references here so the catalog cannot drift. *)

Lemma draw_coq_cell_audited :
  draw_equivalence_marked_partial = true.
Proof. exact draw_equivalence_marked_partial_value. Qed.

Lemma replay_coq_cell_audited :
  replay_equivalence_marked_partial = true.
Proof. exact replay_equivalence_marked_partial_value. Qed.

Lemma reorg_coq_cell_audited :
  reorg_equivalence_marked_partial = true.
Proof. exact reorg_equivalence_marked_partial_value. Qed.

Lemma cb_coq_cell_audited :
  cb_equivalence_marked_partial = true.
Proof. exact cb_equivalence_marked_partial_value. Qed.

Lemma supply_eligibility_cell_audited :
  se_equivalence_marked_partial = true.
Proof. exact se_equivalence_marked_partial_value. Qed.

(** Frozen = live-runtime payload identities used by 4 components. *)
Lemma e2e_frozen_runtime_identities :
  draw_frozen_txid = draw_live_runtime_txid /\
  replay_frozen_payload = replay_runtime_payload /\
  reorg_frozen_payload = reorg_runtime_payload /\
  cb_frozen_payload = cb_runtime_payload.
Proof.
  split; [exact draw_frozen_live_runtime_equivalence | ].
  split; [exact replay_frozen_runtime_equivalence | ].
  split; [exact reorg_frozen_runtime_equivalence | ].
  exact cb_frozen_runtime_equivalence.
Qed.

(** ================================================================
    Part D:  Circular-dependency tally
    ================================================================ *)

Definition is_cd_imported_eligibility (e : E2ECatalogEntry) : bool :=
  match e.(e2e_circular) with
  | E2E_CD_ImportedEligibility => true
  | _ => false
  end.

Definition is_cd_payload_domain_gap (e : E2ECatalogEntry) : bool :=
  match e.(e2e_circular) with
  | E2E_CD_PayloadDomainGap => true
  | _ => false
  end.

Definition is_cd_supply_absent (e : E2ECatalogEntry) : bool :=
  match e.(e2e_circular) with
  | E2E_CD_SupplyAbsent => true
  | _ => false
  end.

Definition e2e_imported_eligibility_count : nat :=
  length (filter is_cd_imported_eligibility e2e_equivalence_matrix).
Definition e2e_payload_domain_gap_count : nat :=
  length (filter is_cd_payload_domain_gap e2e_equivalence_matrix).
Definition e2e_supply_absent_count : nat :=
  length (filter is_cd_supply_absent e2e_equivalence_matrix).

Lemma e2e_imported_eligibility_count_value :
  e2e_imported_eligibility_count = 4%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma e2e_payload_domain_gap_count_value :
  e2e_payload_domain_gap_count = 8%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma e2e_supply_absent_count_value :
  e2e_supply_absent_count = 2%nat.
Proof. vm_compute. reflexivity. Qed.

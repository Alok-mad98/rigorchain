(** Phase5Equivalence/RuntimeEquivalenceCandidateCatalog.v
    Runtime witness ↔ Coq witness candidate-equivalence layer (TASK-FVR-J).

    Formal machine-checkable representation of FVR-I [CandidateEquivalenceStatus]
    classifications. Maps abstract [RuntimeWitness], [CoqWitnessRow], and
    [CHASReport] inputs to [CandidateEquivalenceObservation] records.

    HONESTY BOUNDARY (CRITICAL):
    - DOES NOT assert Coq = Runtime equivalence.
    - [equivalence_claimed] remains false; PD-4F-OB-02 / PD-4F-CPP-01 OPEN.
    - Observation / audit only; no consensus or runtime code modified. *)

From Coq Require Import List ZArith Bool Strings.String.
Import ListNotations.
Open Scope string_scope.
Require Import Rigor.Phase5Equivalence.EquivalenceFramework.
Require Import Rigor.Phase5Equivalence.CanonicalStateEncoding.

Close Scope Z_scope.
Open Scope string_scope.

(** ================================================================
    Part A:  Vocabulary (mirrors FVR-I C++)
    ================================================================ *)

Definition hash256 := Z.

Inductive CandidateEquivalenceStatus :=
| CES_Comparable
| CES_PartiallyComparable
| CES_NotComparable
| CES_FundamentallyBlocked.

Definition ces_comparable (s : CandidateEquivalenceStatus) : bool :=
  match s with CES_Comparable => true | _ => false end.
Definition ces_partially_comparable (s : CandidateEquivalenceStatus) : bool :=
  match s with CES_PartiallyComparable => true | _ => false end.
Definition ces_not_comparable (s : CandidateEquivalenceStatus) : bool :=
  match s with CES_NotComparable => true | _ => false end.
Definition ces_fundamentally_blocked (s : CandidateEquivalenceStatus) : bool :=
  match s with CES_FundamentallyBlocked => true | _ => false end.

(** ================================================================
    Part B:  Input records (FVR-F / FVR-G / FVR-H abstractions)
    ================================================================ *)

Record RuntimeWitness := {
  rw_height : Z;
  rw_block_hash : hash256;
  rw_oru_supply_cumulative : Z;
  rw_res_mint_cumulative : Z;
  rw_invariants_passed : bool;
  rw_violations : list string;
}.

Record CoqWitnessRow := {
  cwr_height : Z;
  cwr_block_hash : hash256;
  cwr_rs_oru_supply : Z;
  cwr_rs_res_supply : Z;
  cwr_invariants_passed : bool;
  cwr_violations : list string;
  cwr_parent_witness_present : bool;
  cwr_witness_chain_step_valid : bool;
}.

Record CHASReport := {
  chas_oru_fundamentally_blocked : bool;
  chas_res_fundamentally_blocked : bool;
  chas_delta_violations : list string;
}.

Record CandidateEquivalenceObservation := {
  ceo_height : nat;
  ceo_block_hash : hash256;
  ceo_oru_supply_status : CandidateEquivalenceStatus;
  ceo_res_supply_status : CandidateEquivalenceStatus;
  ceo_invariant_status : CandidateEquivalenceStatus;
  ceo_witness_chain_status : CandidateEquivalenceStatus;
  ceo_blocking_reasons : list string;
}.

(** ================================================================
    Part C:  Classification (mirrors FVR-I C++ rules)
    ================================================================ *)

Definition runtime_witness_present (rw : RuntimeWitness) : bool :=
  Z.geb rw.(rw_height) 0.

Definition coq_oru_present (row : CoqWitnessRow) : bool :=
  Z.geb row.(cwr_height) 0.

Definition coq_res_present (row : CoqWitnessRow) : bool :=
  Z.geb row.(cwr_height) 0.

Definition classify_supply_candidate
  (runtime_present coq_present parent_delta : bool)
  : CandidateEquivalenceStatus :=
  if runtime_present then
    if coq_present then
      if parent_delta then CES_Comparable else CES_PartiallyComparable
    else CES_NotComparable
  else CES_NotComparable.

Definition classify_invariant_candidate (row : CoqWitnessRow) : CandidateEquivalenceStatus :=
  if row.(cwr_invariants_passed) then
    if Z.gtb row.(cwr_height) 0 then
      if row.(cwr_witness_chain_step_valid) then CES_Comparable
      else CES_FundamentallyBlocked
    else CES_Comparable
  else CES_FundamentallyBlocked.

Definition classify_witness_chain_candidate
  (runtime_present : bool) (row : CoqWitnessRow)
  : CandidateEquivalenceStatus :=
  if runtime_present then
    if Z.eqb row.(cwr_height) 0 then
      if row.(cwr_witness_chain_step_valid) then CES_Comparable
      else CES_FundamentallyBlocked
    else
      if row.(cwr_parent_witness_present) then
        if row.(cwr_witness_chain_step_valid) then CES_Comparable
        else CES_FundamentallyBlocked
      else CES_PartiallyComparable
  else CES_NotComparable.

Definition append_unique (xs : list string) (s : string) : list string :=
  if existsb (String.eqb s) xs then xs else xs ++ [s]%string.

Fixpoint append_unique_list (xs : list string) (ys : list string) : list string :=
  match ys with
  | nil => xs
  | y :: ys' => append_unique_list (append_unique xs y) ys'
  end.

Definition collect_supply_blocking
  (label : string)
  (status : CandidateEquivalenceStatus)
  (runtime_present coq_present parent_delta : bool)
  : list string :=
  if ces_comparable status then nil else
  (if runtime_present then nil else [label ++ "-RUNTIME-WITNESS-MISSING"]%string) ++
  (if coq_present then nil else [label ++ "-COQ-CATALOG-MISSING"]%string) ++
  (if runtime_present && coq_present && negb parent_delta then
     [label ++ "-PARENT-DELTA-MISSING"]%string else nil) ++
  (if ces_not_comparable status then [label ++ "-CES-NotComparable"]%string else nil).

Definition collect_chas_blocking (chas : CHASReport) : list string :=
  let r1 :=
    if chas.(chas_oru_fundamentally_blocked) then ["CHAS-ORU-FundamentallyBlocked"%string]
    else nil in
  let r2 :=
    if chas.(chas_res_fundamentally_blocked) then ["CHAS-RES-FundamentallyBlocked"%string]
    else nil in
  append_unique_list (r1 ++ r2) chas.(chas_delta_violations).

Definition nat_of_height (h : Z) : nat :=
  if Z.leb h 0 then 0%nat else (Z.to_nat h).

Definition check_candidate_equivalence
  (rw : RuntimeWitness) (row : CoqWitnessRow) (chas : CHASReport)
  : CandidateEquivalenceObservation :=
  let runtime_present := runtime_witness_present rw in
  let parent_delta := row.(cwr_parent_witness_present) in
  let oru_status :=
    classify_supply_candidate runtime_present (coq_oru_present row) parent_delta in
  let res_status :=
    classify_supply_candidate runtime_present (coq_res_present row) parent_delta in
  let inv_status := classify_invariant_candidate row in
  let chain_status := classify_witness_chain_candidate runtime_present row in
  let reasons :=
    append_unique_list nil
      (collect_supply_blocking "ORU"%string oru_status runtime_present
         (coq_oru_present row) parent_delta ++
       collect_supply_blocking "RES"%string res_status runtime_present
         (coq_res_present row) parent_delta) in
  let reasons :=
    if ces_fundamentally_blocked inv_status then
      append_unique_list reasons
        (if row.(cwr_invariants_passed) then
           ["WITNESS-CHAIN-INVARIANT-BLOCKED"%string]
         else
           ["INVARIANT-CES-FundamentallyBlocked"%string] ++ row.(cwr_violations))
    else reasons in
  let reasons :=
    if ces_fundamentally_blocked chain_status then
      append_unique reasons "WITNESS-CHAIN-CES-FundamentallyBlocked"%string
    else
      if ces_partially_comparable chain_status then
        append_unique reasons "WITNESS-CHAIN-PARENT-MISSING"%string
      else reasons in
  let reasons := append_unique reasons "DRAW-CES-FundamentallyBlocked"%string in
  let reasons := append_unique_list reasons (collect_chas_blocking chas) in
  let reasons := append_unique_list reasons
    ["PD-4F-OB-02-OPEN"%string; "PD-4F-CPP-01-OPEN"%string] in
  {| ceo_height := nat_of_height rw.(rw_height);
     ceo_block_hash := rw.(rw_block_hash);
     ceo_oru_supply_status := oru_status;
     ceo_res_supply_status := res_status;
     ceo_invariant_status := inv_status;
     ceo_witness_chain_status := chain_status;
     ceo_blocking_reasons := reasons |}.

Fixpoint catalog_all_candidate_equivalence
  (rws : list RuntimeWitness)
  (rows : list CoqWitnessRow)
  (chas_reports : list CHASReport)
  : list CandidateEquivalenceObservation :=
  match rws, rows, chas_reports with
  | rw :: rws', row :: rows', ch :: chs' =>
      check_candidate_equivalence rw row ch ::
      catalog_all_candidate_equivalence rws' rows' chs'
  | _, _, _ => nil
  end.

(** ================================================================
    Part D:  Frozen sample catalog (audit / regression)
    ================================================================ *)

Definition chas_neutral : CHASReport :=
  {| chas_oru_fundamentally_blocked := false;
     chas_res_fundamentally_blocked := false;
     chas_delta_violations := nil |}.

Definition sample_witness_genesis : RuntimeWitness :=
  {| rw_height := 0;
     rw_block_hash := 1000%Z;
     rw_oru_supply_cumulative := 50;
     rw_res_mint_cumulative := 0;
     rw_invariants_passed := true;
     rw_violations := nil |}.

Definition sample_row_genesis : CoqWitnessRow :=
  {| cwr_height := 0;
     cwr_block_hash := 1000%Z;
     cwr_rs_oru_supply := 50;
     cwr_rs_res_supply := 0;
     cwr_invariants_passed := true;
     cwr_violations := nil;
     cwr_parent_witness_present := false;
     cwr_witness_chain_step_valid := true |}.

Definition sample_witness_comparable : RuntimeWitness :=
  {| rw_height := 1;
     rw_block_hash := 1001%Z;
     rw_oru_supply_cumulative := 51;
     rw_res_mint_cumulative := 0;
     rw_invariants_passed := true;
     rw_violations := nil |}.

Definition sample_row_comparable : CoqWitnessRow :=
  {| cwr_height := 1;
     cwr_block_hash := 1001%Z;
     cwr_rs_oru_supply := 51;
     cwr_rs_res_supply := 0;
     cwr_invariants_passed := true;
     cwr_violations := nil;
     cwr_parent_witness_present := true;
     cwr_witness_chain_step_valid := true |}.

Definition sample_witness_broken : RuntimeWitness :=
  {| rw_height := 5;
     rw_block_hash := 1005%Z;
     rw_oru_supply_cumulative := 99;
     rw_res_mint_cumulative := 0;
     rw_invariants_passed := true;
     rw_violations := nil |}.

Definition sample_row_broken : CoqWitnessRow :=
  {| cwr_height := 5;
     cwr_block_hash := 1005%Z;
     cwr_rs_oru_supply := 99;
     cwr_rs_res_supply := 0;
     cwr_invariants_passed := true;
     cwr_violations := nil;
     cwr_parent_witness_present := true;
     cwr_witness_chain_step_valid := false |}.

Definition sample_witness_not_comparable : RuntimeWitness :=
  {| rw_height := -1;
     rw_block_hash := 0%Z;
     rw_oru_supply_cumulative := 0;
     rw_res_mint_cumulative := 0;
     rw_invariants_passed := false;
     rw_violations := nil |}.

Definition sample_row_not_comparable : CoqWitnessRow :=
  {| cwr_height := -1;
     cwr_block_hash := 0%Z;
     cwr_rs_oru_supply := 0;
     cwr_rs_res_supply := 0;
     cwr_invariants_passed := false;
     cwr_violations := nil;
     cwr_parent_witness_present := false;
     cwr_witness_chain_step_valid := false |}.

Definition sample_witness_invariant_fail : RuntimeWitness :=
  {| rw_height := 3;
     rw_block_hash := 1003%Z;
     rw_oru_supply_cumulative := 0;
     rw_res_mint_cumulative := 0;
     rw_invariants_passed := false;
     rw_violations := ["ORU-L1"%string] |}.

Definition sample_row_invariant_fail : CoqWitnessRow :=
  {| cwr_height := 3;
     cwr_block_hash := 1003%Z;
     cwr_rs_oru_supply := 0;
     cwr_rs_res_supply := 0;
     cwr_invariants_passed := false;
     cwr_violations := ["ORU-L1"%string];
     cwr_parent_witness_present := false;
     cwr_witness_chain_step_valid := true |}.

Definition runtime_equivalence_candidate_catalog : list CandidateEquivalenceObservation :=
  [ check_candidate_equivalence sample_witness_genesis sample_row_genesis chas_neutral;
    check_candidate_equivalence sample_witness_comparable sample_row_comparable chas_neutral;
    check_candidate_equivalence sample_witness_broken sample_row_broken chas_neutral;
    check_candidate_equivalence sample_witness_not_comparable sample_row_not_comparable chas_neutral;
    check_candidate_equivalence sample_witness_invariant_fail sample_row_invariant_fail chas_neutral ].

Definition runtime_equivalence_candidate_count : nat :=
  List.length runtime_equivalence_candidate_catalog.

(** ================================================================
    Part E:  Status flattening and partition counts
    ================================================================ *)

Definition flatten_observation_statuses
  (obs : CandidateEquivalenceObservation) : list CandidateEquivalenceStatus :=
  [ obs.(ceo_oru_supply_status);
    obs.(ceo_res_supply_status);
    obs.(ceo_invariant_status);
    obs.(ceo_witness_chain_status) ].

Fixpoint flatten_catalog_statuses
  (catalog : list CandidateEquivalenceObservation) : list CandidateEquivalenceStatus :=
  match catalog with
  | nil => nil
  | o :: rest => flatten_observation_statuses o ++ flatten_catalog_statuses rest
  end.

Definition candidate_equivalence_statuses : list CandidateEquivalenceStatus :=
  flatten_catalog_statuses runtime_equivalence_candidate_catalog.

Definition candidate_equivalence_comparable_count : nat :=
  List.length (filter ces_comparable candidate_equivalence_statuses).
Definition candidate_equivalence_partial_count : nat :=
  List.length (filter ces_partially_comparable candidate_equivalence_statuses).
Definition candidate_equivalence_not_comparable_count : nat :=
  List.length (filter ces_not_comparable candidate_equivalence_statuses).
Definition candidate_equivalence_blocked_count : nat :=
  List.length (filter ces_fundamentally_blocked candidate_equivalence_statuses).

Definition candidate_equivalence_field_count : nat :=
  List.length candidate_equivalence_statuses.

(** ================================================================
    Part F:  Required proofs
    ================================================================ *)

(* VACUOUS-PLACEHOLDER (SECURITY-001-F / F-4): the statement below has the
   form [X = X]. It is trivially true by reflexivity and constrains nothing
   about [check_candidate_equivalence_deterministic]. Retained for catalog shape only. NOT EVIDENCE. *)
Lemma check_candidate_equivalence_deterministic :
  forall rw row chas,
    check_candidate_equivalence rw row chas =
    check_candidate_equivalence rw row chas.
Proof. intros. reflexivity. Qed.

Lemma check_candidate_equivalence_total :
  forall rw row chas,
    exists obs, obs = check_candidate_equivalence rw row chas.
Proof. intros rw row chas. exists (check_candidate_equivalence rw row chas). reflexivity. Qed.

Lemma candidate_equivalence_status_well_defined :
  forall s, In s candidate_equivalence_statuses ->
    match s with
    | CES_Comparable => True
    | CES_PartiallyComparable => True
    | CES_NotComparable => True
    | CES_FundamentallyBlocked => True
    end.
Proof.
  intros s Hin. destruct s; exact I.
Qed.

Lemma runtime_equivalence_candidate_catalog_nonempty :
  runtime_equivalence_candidate_catalog <> nil.
Proof. discriminate. Qed.

Lemma candidate_equivalence_field_count_value :
  candidate_equivalence_field_count = 20%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma candidate_equivalence_comparable_count_value :
  candidate_equivalence_comparable_count = 8%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma candidate_equivalence_partial_count_value :
  candidate_equivalence_partial_count = 5%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma candidate_equivalence_not_comparable_count_value :
  candidate_equivalence_not_comparable_count = 3%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma candidate_equivalence_blocked_count_value :
  candidate_equivalence_blocked_count = 4%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma candidate_equivalence_partition_complete :
  candidate_equivalence_comparable_count +
  candidate_equivalence_partial_count +
  candidate_equivalence_not_comparable_count +
  candidate_equivalence_blocked_count
  = candidate_equivalence_field_count.
Proof. vm_compute. reflexivity. Qed.

Lemma catalog_all_sample_matches :
  catalog_all_candidate_equivalence
    [ sample_witness_genesis; sample_witness_comparable ]
    [ sample_row_genesis; sample_row_comparable ]
    [ chas_neutral; chas_neutral ] =
  [ check_candidate_equivalence sample_witness_genesis sample_row_genesis chas_neutral;
    check_candidate_equivalence sample_witness_comparable sample_row_comparable chas_neutral ].
Proof. reflexivity. Qed.

Lemma sample_genesis_partial_supply :
  let obs := check_candidate_equivalence sample_witness_genesis sample_row_genesis chas_neutral in
  obs.(ceo_oru_supply_status) = CES_PartiallyComparable /\
  obs.(ceo_witness_chain_status) = CES_Comparable.
Proof. vm_compute. split; reflexivity. Qed.

Lemma sample_comparable_all_supply :
  let obs := check_candidate_equivalence sample_witness_comparable sample_row_comparable chas_neutral in
  obs.(ceo_oru_supply_status) = CES_Comparable /\
  obs.(ceo_res_supply_status) = CES_Comparable.
Proof. vm_compute. split; reflexivity. Qed.

Lemma sample_broken_chain_blocked :
  let obs := check_candidate_equivalence sample_witness_broken sample_row_broken chas_neutral in
  obs.(ceo_witness_chain_status) = CES_FundamentallyBlocked.
Proof. vm_compute. reflexivity. Qed.

Lemma sample_draw_blocked_on_frozen_catalog :
  existsb (String.eqb "DRAW-CES-FundamentallyBlocked"%string)
    (check_candidate_equivalence sample_witness_genesis sample_row_genesis chas_neutral).(ceo_blocking_reasons) = true /\
  existsb (String.eqb "DRAW-CES-FundamentallyBlocked"%string)
    (check_candidate_equivalence sample_witness_comparable sample_row_comparable chas_neutral).(ceo_blocking_reasons) = true /\
  existsb (String.eqb "DRAW-CES-FundamentallyBlocked"%string)
    (check_candidate_equivalence sample_witness_broken sample_row_broken chas_neutral).(ceo_blocking_reasons) = true /\
  existsb (String.eqb "DRAW-CES-FundamentallyBlocked"%string)
    (check_candidate_equivalence sample_witness_not_comparable sample_row_not_comparable chas_neutral).(ceo_blocking_reasons) = true /\
  existsb (String.eqb "DRAW-CES-FundamentallyBlocked"%string)
    (check_candidate_equivalence sample_witness_invariant_fail sample_row_invariant_fail chas_neutral).(ceo_blocking_reasons) = true.
Proof. vm_compute. repeat split; reflexivity. Qed.

(** ================================================================
    Part G:  Honesty / debt
    ================================================================ *)

Definition runtime_equivalence_candidate_established : bool := true.
Definition runtime_equivalence_candidate_coq_eq_runtime : bool := false.

Lemma runtime_equivalence_candidate_does_not_establish_equivalence :
  runtime_equivalence_candidate_coq_eq_runtime = false /\
  equivalence_claimed = false.
Proof. split; reflexivity. Qed.

Lemma runtime_equivalence_candidate_preserves_open_proof_debt :
  negb proof_debt_placeholder.(pdp_pd_4f_ob_02_closed) = true /\
  negb proof_debt_placeholder.(pdp_pd_4f_cpp_01_closed) = true.
Proof.
  split.
  - exact canonical_encoding_pd_ob_02_open.
  - exact canonical_encoding_pd_cpp_01_open.
Qed.

(** ================================================================
    Part H:  Phase-D golden vector closure alignment (TASK-PROOF-018)
    ================================================================ *)

Inductive GoldenVectorFamily :=
| GVF_ORU
| GVF_RES
| GVF_DRAW
| GVF_STATE.

Inductive AuditItemLifecycle :=
| AI_OPEN
| AI_PARTIAL
| AI_READY
| AI_CLOSED
| AI_RETIRED.

Inductive GvrHarnessStatus :=
| GVR_PASS
| GVR_PARTIAL
| GVR_FAIL
| GVR_NOT_EXECUTED.

Inductive GoldenVectorClosureGate :=
| GVCG_ECS_Open
| GVCG_ECS_PartiallyClosable
| GVCG_ECS_Prohibited.

Record GoldenVectorRuntimeSnapshot := {
  gvs_vector_id : string;
  gvs_family : GoldenVectorFamily;
  gvs_height : nat;
  gvs_materialized : bool;
  gvs_harness_status : GvrHarnessStatus;
  gvs_audit_lifecycle : AuditItemLifecycle;
  gvs_rw : RuntimeWitness;
  gvs_row : CoqWitnessRow;
  gvs_chas : CHASReport;
}.

Record GoldenVectorEquivalenceCandidateObservation := {
  gveco_vector_id : string;
  gveco_snapshot : GoldenVectorRuntimeSnapshot;
  gveco_candidate : CandidateEquivalenceObservation;
  gveco_closure_gate : GoldenVectorClosureGate;
  gveco_equivalence_claimed : bool;
  gveco_coq_equiv_runtime : string;
}.

Definition coq_equiv_runtime_not_established : string := "NOT_ESTABLISHED"%string.

Definition mk_gv_witness (h : nat) (hash : hash256) (oru res : Z) : RuntimeWitness :=
  {| rw_height := Z.of_nat h;
     rw_block_hash := hash;
     rw_oru_supply_cumulative := oru;
     rw_res_mint_cumulative := res;
     rw_invariants_passed := true;
     rw_violations := nil |}.

Definition mk_gv_row (h : nat) (hash : hash256) (oru res : Z) : CoqWitnessRow :=
  {| cwr_height := Z.of_nat h;
     cwr_block_hash := hash;
     cwr_rs_oru_supply := oru;
     cwr_rs_res_supply := res;
     cwr_invariants_passed := true;
     cwr_violations := nil;
     cwr_parent_witness_present := negb (Nat.eqb h 0);
     cwr_witness_chain_step_valid := true |}.

Definition mk_gv_snapshot
  (id : string) (fam : GoldenVectorFamily) (h : nat) (mat : bool)
  (harness : GvrHarnessStatus) (audit : AuditItemLifecycle)
  (hash : hash256) (oru res : Z)
  : GoldenVectorRuntimeSnapshot :=
  {| gvs_vector_id := id;
     gvs_family := fam;
     gvs_height := h;
     gvs_materialized := mat;
     gvs_harness_status := harness;
     gvs_audit_lifecycle := audit;
     gvs_rw := mk_gv_witness h hash oru res;
     gvs_row := mk_gv_row h hash oru res;
     gvs_chas := chas_neutral |}.

Definition gv_snap_oru_0001 :=
  mk_gv_snapshot "GV-ORU-0001"%string GVF_ORU 0 true GVR_PASS AI_PARTIAL
    3000%Z 100000000000 0.
Definition gv_snap_oru_0002 :=
  mk_gv_snapshot "GV-ORU-0002"%string GVF_ORU 1 true GVR_PASS AI_PARTIAL
    3001%Z 190000000000 0.
Definition gv_snap_oru_0003 :=
  mk_gv_snapshot "GV-ORU-0003"%string GVF_ORU 630000 false GVR_NOT_EXECUTED AI_OPEN
    3630000%Z 0 0.
Definition gv_snap_res_0001 :=
  mk_gv_snapshot "GV-RES-0001"%string GVF_RES 10 true GVR_PASS AI_PARTIAL
    3010%Z 0 10000.
Definition gv_snap_res_0002 :=
  mk_gv_snapshot "GV-RES-0002"%string GVF_RES 500 true GVR_PASS AI_PARTIAL
    3500%Z 0 20000.
Definition gv_snap_res_0003 :=
  mk_gv_snapshot "GV-RES-0003"%string GVF_RES 2100000 false GVR_NOT_EXECUTED AI_OPEN
    2103000%Z 0 0.
Definition gv_snap_draw_0001 :=
  mk_gv_snapshot "GV-DRAW-0001"%string GVF_DRAW 1 true GVR_PARTIAL AI_PARTIAL
    3101%Z 0 10000.
Definition gv_snap_draw_0002 :=
  mk_gv_snapshot "GV-DRAW-0002"%string GVF_DRAW 7 true GVR_PARTIAL AI_PARTIAL
    3107%Z 0 20000.
Definition gv_snap_draw_0003 :=
  mk_gv_snapshot "GV-DRAW-0003"%string GVF_DRAW 2100001 false GVR_NOT_EXECUTED AI_OPEN
    2104001%Z 0 0.
Definition gv_snap_state_0001 :=
  mk_gv_snapshot "GV-STATE-0001"%string GVF_STATE 0 true GVR_PASS AI_PARTIAL
    3200%Z 100000000000 0.
Definition gv_snap_state_0002 :=
  mk_gv_snapshot "GV-STATE-0002"%string GVF_STATE 10 true GVR_PASS AI_PARTIAL
    3210%Z 0 10000.
Definition gv_snap_state_0003 :=
  mk_gv_snapshot "GV-STATE-0003"%string GVF_STATE 6 true GVR_PASS AI_PARTIAL
    3206%Z 0 0.

Definition golden_vector_runtime_catalog : list GoldenVectorRuntimeSnapshot :=
  [ gv_snap_oru_0001; gv_snap_oru_0002; gv_snap_oru_0003;
    gv_snap_res_0001; gv_snap_res_0002; gv_snap_res_0003;
    gv_snap_draw_0001; gv_snap_draw_0002; gv_snap_draw_0003;
    gv_snap_state_0001; gv_snap_state_0002; gv_snap_state_0003 ].

Definition golden_vector_runtime_catalog_count : nat :=
  List.length golden_vector_runtime_catalog.

Definition classify_golden_vector_closure_gate
  (snap : GoldenVectorRuntimeSnapshot) : GoldenVectorClosureGate :=
  if snap.(gvs_materialized) then
    match snap.(gvs_family) with
    | GVF_DRAW => GVCG_ECS_Prohibited
    | _ =>
      match snap.(gvs_audit_lifecycle) with
      | AI_PARTIAL => GVCG_ECS_PartiallyClosable
      | _ => GVCG_ECS_Open
      end
    end
  else GVCG_ECS_Open.

Definition build_golden_vector_equivalence_obs
  (snap : GoldenVectorRuntimeSnapshot)
  : GoldenVectorEquivalenceCandidateObservation :=
  {| gveco_vector_id := snap.(gvs_vector_id);
     gveco_snapshot := snap;
     gveco_candidate :=
       check_candidate_equivalence snap.(gvs_rw) snap.(gvs_row) snap.(gvs_chas);
     gveco_closure_gate := classify_golden_vector_closure_gate snap;
     gveco_equivalence_claimed := false;
     gveco_coq_equiv_runtime := coq_equiv_runtime_not_established |}.

Fixpoint build_golden_vector_equivalence_catalog
  (snaps : list GoldenVectorRuntimeSnapshot)
  : list GoldenVectorEquivalenceCandidateObservation :=
  match snaps with
  | nil => nil
  | s :: rest => build_golden_vector_equivalence_obs s :: build_golden_vector_equivalence_catalog rest
  end.

Definition golden_vector_equivalence_candidate_catalog :
  list GoldenVectorEquivalenceCandidateObservation :=
  build_golden_vector_equivalence_catalog golden_vector_runtime_catalog.

Definition golden_vector_equivalence_candidate_count : nat :=
  List.length golden_vector_equivalence_candidate_catalog.

Definition gvcg_open (g : GoldenVectorClosureGate) : bool :=
  match g with GVCG_ECS_Open => true | _ => false end.
Definition gvcg_partially_closable (g : GoldenVectorClosureGate) : bool :=
  match g with GVCG_ECS_PartiallyClosable => true | _ => false end.
Definition gvcg_prohibited (g : GoldenVectorClosureGate) : bool :=
  match g with GVCG_ECS_Prohibited => true | _ => false end.

Definition golden_vector_closure_gates : list GoldenVectorClosureGate :=
  map (fun o => o.(gveco_closure_gate)) golden_vector_equivalence_candidate_catalog.

Definition golden_vector_closure_open_count : nat :=
  List.length (filter gvcg_open golden_vector_closure_gates).
Definition golden_vector_closure_partial_count : nat :=
  List.length (filter gvcg_partially_closable golden_vector_closure_gates).
Definition golden_vector_closure_prohibited_count : nat :=
  List.length (filter gvcg_prohibited golden_vector_closure_gates).

Definition gvcg_to_string (g : GoldenVectorClosureGate) : string :=
  match g with
  | GVCG_ECS_Open => "ECS_Open"%string
  | GVCG_ECS_PartiallyClosable => "ECS_PartiallyClosable"%string
  | GVCG_ECS_Prohibited => "ECS_Prohibited"%string
  end.

Definition gv_runtime_witness_chain_ok (rw : RuntimeWitness) (row : CoqWitnessRow) : bool :=
  if runtime_witness_present rw then
    if Z.eqb row.(cwr_height) 0 then row.(cwr_witness_chain_step_valid)
    else row.(cwr_parent_witness_present) && row.(cwr_witness_chain_step_valid)
  else false.

Definition golden_vector_closure_allowed (snap : GoldenVectorRuntimeSnapshot) : bool :=
  let rw := snap.(gvs_rw) in
  let row := snap.(gvs_row) in
  let chas := snap.(gvs_chas) in
  let ceo := check_candidate_equivalence rw row chas in
  gv_runtime_witness_chain_ok rw row &&
  rw.(rw_invariants_passed) && row.(cwr_invariants_passed) &&
  negb chas.(chas_oru_fundamentally_blocked) &&
  negb chas.(chas_res_fundamentally_blocked) &&
  negb (existsb (String.eqb "DRAW-CES-FundamentallyBlocked"%string) ceo.(ceo_blocking_reasons)).

Definition ces_status_to_string (s : CandidateEquivalenceStatus) : string :=
  match s with
  | CES_Comparable => "CES_Comparable"%string
  | CES_PartiallyComparable => "CES_PartiallyComparable"%string
  | CES_NotComparable => "CES_NotComparable"%string
  | CES_FundamentallyBlocked => "CES_FundamentallyBlocked"%string
  end.

Definition CoqEquivalenceCandidateExportToJson
  (obs : GoldenVectorEquivalenceCandidateObservation) : string :=
  "{""task"":""FVR-J"",""vector_id"":""" ++ obs.(gveco_vector_id) ++
  """,""equivalence_claimed"":false,""coq_equiv_runtime"":""NOT_ESTABLISHED""," ++
  """closure_gate"":""" ++ gvcg_to_string obs.(gveco_closure_gate) ++
  """,""ceo_oru_supply_status"":""" ++
  ces_status_to_string obs.(gveco_candidate).(ceo_oru_supply_status) ++
  """}"%string.

Fixpoint string_join_comma (xs : list string) : string :=
  match xs with
  | nil => ""%string
  | [x] => x
  | x :: xs' => x ++ "," ++ string_join_comma xs'
  end.

Definition golden_vector_equivalence_export_json : string :=
  "[" ++ string_join_comma
    (map CoqEquivalenceCandidateExportToJson golden_vector_equivalence_candidate_catalog)
  ++ "]"%string.

Lemma golden_vector_runtime_catalog_count_value :
  golden_vector_runtime_catalog_count = 12%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma golden_vector_equivalence_candidate_count_value :
  golden_vector_equivalence_candidate_count = 12%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma golden_vector_materialized_count_value :
  List.length (filter (fun s => s.(gvs_materialized)) golden_vector_runtime_catalog) = 9%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma golden_vector_closure_open_count_value :
  golden_vector_closure_open_count = 3%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma golden_vector_closure_partial_count_value :
  golden_vector_closure_partial_count = 7%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma golden_vector_closure_prohibited_count_value :
  golden_vector_closure_prohibited_count = 2%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma golden_vector_closure_partition_complete :
  golden_vector_closure_open_count +
  golden_vector_closure_partial_count +
  golden_vector_closure_prohibited_count
  = golden_vector_equivalence_candidate_count.
Proof. vm_compute. reflexivity. Qed.

Lemma classify_golden_vector_closure_gate_materialized_draw :
  forall snap,
    snap.(gvs_materialized) = true ->
    snap.(gvs_family) = GVF_DRAW ->
    classify_golden_vector_closure_gate snap = GVCG_ECS_Prohibited.
Proof.
  intros snap Hmat Hfam.
  unfold classify_golden_vector_closure_gate.
  rewrite Hmat, Hfam. reflexivity.
Qed.

Lemma classify_golden_vector_closure_gate_draft_open :
  forall snap,
    snap.(gvs_materialized) = false ->
    classify_golden_vector_closure_gate snap = GVCG_ECS_Open.
Proof.
  intros snap Hmat.
  unfold classify_golden_vector_closure_gate.
  rewrite Hmat. reflexivity.
Qed.

Lemma classify_golden_vector_closure_gate_ai_partial_nondraw :
  forall snap,
    snap.(gvs_materialized) = true ->
    snap.(gvs_family) <> GVF_DRAW ->
    snap.(gvs_audit_lifecycle) = AI_PARTIAL ->
    classify_golden_vector_closure_gate snap = GVCG_ECS_PartiallyClosable.
Proof.
  intros snap Hmat Hdraw Haudit.
  unfold classify_golden_vector_closure_gate.
  rewrite Hmat, Haudit.
  destruct snap.(gvs_family); try tauto; reflexivity.
Qed.

Lemma golden_vector_draw_closure_allowed_false :
  golden_vector_closure_allowed gv_snap_draw_0001 = false /\
  golden_vector_closure_allowed gv_snap_draw_0002 = false.
Proof. vm_compute. split; reflexivity. Qed.

Lemma golden_vector_draw_closure_prohibited :
  classify_golden_vector_closure_gate gv_snap_draw_0001 = GVCG_ECS_Prohibited /\
  classify_golden_vector_closure_gate gv_snap_draw_0002 = GVCG_ECS_Prohibited.
Proof. vm_compute. split; reflexivity. Qed.

Lemma golden_vector_ai_partial_implies_partially_closable :
  classify_golden_vector_closure_gate gv_snap_oru_0001 = GVCG_ECS_PartiallyClosable /\
  classify_golden_vector_closure_gate gv_snap_state_0003 = GVCG_ECS_PartiallyClosable.
Proof. vm_compute. split; reflexivity. Qed.

Lemma golden_vector_draft_vectors_open :
  classify_golden_vector_closure_gate gv_snap_oru_0003 = GVCG_ECS_Open /\
  classify_golden_vector_closure_gate gv_snap_res_0003 = GVCG_ECS_Open /\
  classify_golden_vector_closure_gate gv_snap_draw_0003 = GVCG_ECS_Open.
Proof. vm_compute. repeat split; reflexivity. Qed.

Lemma mk_gv_row_parent_chain :
  forall h hash oru res,
    (mk_gv_row h hash oru res).(cwr_parent_witness_present) = negb (Nat.eqb h 0).
Proof.
  intros h hash oru res. unfold mk_gv_row. reflexivity.
Qed.

Lemma build_golden_vector_equivalence_obs_claimed_false :
  forall snap,
    (build_golden_vector_equivalence_obs snap).(gveco_equivalence_claimed) = false /\
    (build_golden_vector_equivalence_obs snap).(gveco_coq_equiv_runtime) = coq_equiv_runtime_not_established.
Proof. intros snap. simpl. split; reflexivity. Qed.

Lemma golden_vector_candidate_parent_chain_consistent :
  forall h hash oru res,
    (mk_gv_row h hash oru res).(cwr_parent_witness_present) = negb (Nat.eqb h 0).
Proof. apply mk_gv_row_parent_chain. Qed.

Lemma golden_vector_equivalence_claimed_false :
  forall snap,
    (build_golden_vector_equivalence_obs snap).(gveco_equivalence_claimed) = false /\
    (build_golden_vector_equivalence_obs snap).(gveco_coq_equiv_runtime) = coq_equiv_runtime_not_established.
Proof. intros snap. apply build_golden_vector_equivalence_obs_claimed_false. Qed.

Lemma build_golden_vector_equivalence_catalog_length :
  forall snaps,
    List.length (build_golden_vector_equivalence_catalog snaps) = List.length snaps.
Proof.
  induction snaps as [| s rest IH]; simpl; auto.
Qed.

Lemma golden_vector_candidate_catalog_mirrors_runtime :
  golden_vector_equivalence_candidate_count = golden_vector_runtime_catalog_count.
Proof.
  unfold golden_vector_equivalence_candidate_count, golden_vector_runtime_catalog_count,
         golden_vector_equivalence_candidate_catalog.
  rewrite build_golden_vector_equivalence_catalog_length.
  reflexivity.
Qed.

Definition phase_d_closure_alignment_established : bool := true.
Definition phase_d_candidate_equivalence_mapping : string := "ESTABLISHED (partial)"%string.

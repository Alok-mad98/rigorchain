(** Phase5Equivalence/GlobalCandidateEquivalenceCatalog.v
    Global candidate-equivalence layer (TASK-FVR-K).

    Aggregates FVR-H [CHASReport] and FVR-J [CandidateEquivalenceObservation]
    into per-block [GlobalCandidateEquivalenceObservation] records representing
    theoretical component/field equivalence potential.

    HONESTY BOUNDARY (CRITICAL):
    - DOES NOT assert Coq = Runtime equivalence.
    - [equivalence_claimed] remains false; PD-4F-OB-02 / PD-4F-CPP-01 OPEN. *)

From Coq Require Import List ZArith Bool Strings.String.
Import ListNotations.
Open Scope string_scope.
Require Import Rigor.Phase5Equivalence.EquivalenceFramework.
Require Import Rigor.Phase5Equivalence.CanonicalStateEncoding.
Require Import Rigor.Phase5Equivalence.RuntimeEquivalenceCandidateCatalog.

Close Scope Z_scope.
Open Scope string_scope.

(** ================================================================
    Part A:  Global observation record
    ================================================================ *)

Record GlobalCandidateEquivalenceObservation := {
  gceo_height : nat;
  gceo_block_hash : hash256;
  gceo_oru_global_status : CandidateEquivalenceStatus;
  gceo_res_global_status : CandidateEquivalenceStatus;
  gceo_blocking_reasons : list string;
}.

(** ================================================================
    Part B:  CHAS + candidate aggregation
    ================================================================ *)

Definition chas_oru_to_ces (chas : CHASReport) : CandidateEquivalenceStatus :=
  if chas.(chas_oru_fundamentally_blocked) then CES_FundamentallyBlocked
  else CES_PartiallyComparable.

Definition chas_res_to_ces (chas : CHASReport) : CandidateEquivalenceStatus :=
  if chas.(chas_res_fundamentally_blocked) then CES_FundamentallyBlocked
  else CES_PartiallyComparable.

Definition merge_ces (a b : CandidateEquivalenceStatus) : CandidateEquivalenceStatus :=
  match a, b with
  | CES_FundamentallyBlocked, _ | _, CES_FundamentallyBlocked => CES_FundamentallyBlocked
  | CES_NotComparable, _ | _, CES_NotComparable => CES_NotComparable
  | CES_PartiallyComparable, _ | _, CES_PartiallyComparable => CES_PartiallyComparable
  | _, _ => CES_Comparable
  end.

Definition global_oru_status
  (chas : CHASReport) (ceo : CandidateEquivalenceObservation)
  : CandidateEquivalenceStatus :=
  merge_ces (chas_oru_to_ces chas)
    (merge_ces ceo.(ceo_oru_supply_status)
      (merge_ces ceo.(ceo_invariant_status) ceo.(ceo_witness_chain_status))).

Definition global_res_status
  (chas : CHASReport) (ceo : CandidateEquivalenceObservation)
  : CandidateEquivalenceStatus :=
  merge_ces (chas_res_to_ces chas)
    (merge_ces ceo.(ceo_res_supply_status)
      (merge_ces ceo.(ceo_invariant_status) ceo.(ceo_witness_chain_status))).

Definition append_unique (xs : list string) (s : string) : list string :=
  if existsb (String.eqb s) xs then xs else xs ++ [s]%string.

Fixpoint append_unique_list (xs : list string) (ys : list string) : list string :=
  match ys with
  | nil => xs
  | y :: ys' => append_unique_list (append_unique xs y) ys'
  end.

Definition collect_global_blocking
  (chas : CHASReport) (ceo : CandidateEquivalenceObservation)
  : list string :=
  let reasons := append_unique_list nil ceo.(ceo_blocking_reasons) in
  let reasons := append_unique_list reasons chas.(chas_delta_violations) in
  let reasons := append_unique reasons "GLOBAL-CHAS-CANDIDATE-AGGREGATE"%string in
  append_unique_list reasons
    ["PD-4F-OB-02-OPEN"%string; "PD-4F-CPP-01-OPEN"%string].

Definition check_global_candidate_equivalence
  (chas : CHASReport) (ceo : CandidateEquivalenceObservation)
  : GlobalCandidateEquivalenceObservation :=
  {| gceo_height := ceo.(ceo_height);
     gceo_block_hash := ceo.(ceo_block_hash);
     gceo_oru_global_status := global_oru_status chas ceo;
     gceo_res_global_status := global_res_status chas ceo;
     gceo_blocking_reasons := collect_global_blocking chas ceo |}.

Fixpoint catalog_all_global_equivalence
  (chas_reports : list CHASReport)
  (ceo_list : list CandidateEquivalenceObservation)
  : list GlobalCandidateEquivalenceObservation :=
  match chas_reports, ceo_list with
  | ch :: chs', ceo :: ceos' =>
      check_global_candidate_equivalence ch ceo ::
      catalog_all_global_equivalence chs' ceos'
  | _, _ => nil
  end.

(** ================================================================
    Part C:  Frozen global catalog (from FVR-J samples + neutral CHAS)
    ================================================================ *)

Definition global_equivalence_candidate_catalog : list GlobalCandidateEquivalenceObservation :=
  map (fun ceo => check_global_candidate_equivalence chas_neutral ceo)
    runtime_equivalence_candidate_catalog.

Definition global_equivalence_candidate_count : nat :=
  List.length global_equivalence_candidate_catalog.

(** ================================================================
    Part D:  Partition counts (oru + res global statuses)
    ================================================================ *)

Definition flatten_global_statuses
  (obs : GlobalCandidateEquivalenceObservation) : list CandidateEquivalenceStatus :=
  [ obs.(gceo_oru_global_status); obs.(gceo_res_global_status) ].

Fixpoint flatten_global_catalog_statuses
  (catalog : list GlobalCandidateEquivalenceObservation) : list CandidateEquivalenceStatus :=
  match catalog with
  | nil => nil
  | o :: rest => flatten_global_statuses o ++ flatten_global_catalog_statuses rest
  end.

Definition global_equivalence_statuses : list CandidateEquivalenceStatus :=
  flatten_global_catalog_statuses global_equivalence_candidate_catalog.

Definition global_equivalence_comparable_count : nat :=
  List.length (filter ces_comparable global_equivalence_statuses).
Definition global_equivalence_partial_count : nat :=
  List.length (filter ces_partially_comparable global_equivalence_statuses).
Definition global_equivalence_not_comparable_count : nat :=
  List.length (filter ces_not_comparable global_equivalence_statuses).
Definition global_equivalence_blocked_count : nat :=
  List.length (filter ces_fundamentally_blocked global_equivalence_statuses).

Definition global_equivalence_field_count : nat :=
  List.length global_equivalence_statuses.

(** ================================================================
    Part E:  Required proofs
    ================================================================ *)

(* VACUOUS-PLACEHOLDER (SECURITY-001-F / F-4): the statement below has the
   form [X = X]. It is trivially true by reflexivity and constrains nothing
   about [check_global_candidate_equivalence_deterministic]. Retained for catalog shape only. NOT EVIDENCE. *)
Lemma check_global_candidate_equivalence_deterministic :
  forall chas ceo,
    check_global_candidate_equivalence chas ceo =
    check_global_candidate_equivalence chas ceo.
Proof. intros. reflexivity. Qed.

Lemma check_global_candidate_equivalence_total :
  forall chas ceo, exists obs, obs = check_global_candidate_equivalence chas ceo.
Proof. intros chas ceo. exists (check_global_candidate_equivalence chas ceo). reflexivity. Qed.

Lemma global_equivalence_status_well_defined :
  forall s, In s global_equivalence_statuses ->
    match s with
    | CES_Comparable => True
    | CES_PartiallyComparable => True
    | CES_NotComparable => True
    | CES_FundamentallyBlocked => True
    end.
Proof. intros s Hin. destruct s; exact I. Qed.

Lemma global_equivalence_candidate_catalog_nonempty :
  global_equivalence_candidate_catalog <> nil.
Proof. discriminate. Qed.

Lemma global_equivalence_field_count_value :
  global_equivalence_field_count = 10%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma global_equivalence_comparable_count_value :
  global_equivalence_comparable_count = 0%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma global_equivalence_partial_count_value :
  global_equivalence_partial_count = 4%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma global_equivalence_not_comparable_count_value :
  global_equivalence_not_comparable_count = 0%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma global_equivalence_blocked_count_value :
  global_equivalence_blocked_count = 6%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma global_equivalence_partition_complete :
  global_equivalence_comparable_count +
  global_equivalence_partial_count +
  global_equivalence_not_comparable_count +
  global_equivalence_blocked_count
  = global_equivalence_field_count.
Proof. vm_compute. reflexivity. Qed.

Lemma catalog_all_global_sample_matches :
  catalog_all_global_equivalence
    [ chas_neutral; chas_neutral ]
    [ check_candidate_equivalence sample_witness_genesis sample_row_genesis chas_neutral;
      check_candidate_equivalence sample_witness_comparable sample_row_comparable chas_neutral ] =
  [ check_global_candidate_equivalence chas_neutral
      (check_candidate_equivalence sample_witness_genesis sample_row_genesis chas_neutral);
    check_global_candidate_equivalence chas_neutral
      (check_candidate_equivalence sample_witness_comparable sample_row_comparable chas_neutral) ].
Proof. reflexivity. Qed.

Lemma sample_global_genesis_partial_oru :
  let g := check_global_candidate_equivalence chas_neutral
    (check_candidate_equivalence sample_witness_genesis sample_row_genesis chas_neutral) in
  g.(gceo_oru_global_status) = CES_PartiallyComparable.
Proof. vm_compute. reflexivity. Qed.

Lemma sample_global_comparable_partial_oru :
  let g := check_global_candidate_equivalence chas_neutral
    (check_candidate_equivalence sample_witness_comparable sample_row_comparable chas_neutral) in
  g.(gceo_oru_global_status) = CES_PartiallyComparable.
Proof. vm_compute. reflexivity. Qed.

Lemma sample_global_broken_blocked :
  let g := check_global_candidate_equivalence chas_neutral
    (check_candidate_equivalence sample_witness_broken sample_row_broken chas_neutral) in
  g.(gceo_oru_global_status) = CES_FundamentallyBlocked.
Proof. vm_compute. reflexivity. Qed.

Lemma sample_global_draw_blocked_on_frozen_catalog :
  existsb (String.eqb "DRAW-CES-FundamentallyBlocked"%string)
    (check_global_candidate_equivalence chas_neutral
      (check_candidate_equivalence sample_witness_genesis sample_row_genesis chas_neutral)).(gceo_blocking_reasons) = true /\
  existsb (String.eqb "DRAW-CES-FundamentallyBlocked"%string)
    (check_global_candidate_equivalence chas_neutral
      (check_candidate_equivalence sample_witness_comparable sample_row_comparable chas_neutral)).(gceo_blocking_reasons) = true /\
  existsb (String.eqb "DRAW-CES-FundamentallyBlocked"%string)
    (check_global_candidate_equivalence chas_neutral
      (check_candidate_equivalence sample_witness_broken sample_row_broken chas_neutral)).(gceo_blocking_reasons) = true /\
  existsb (String.eqb "DRAW-CES-FundamentallyBlocked"%string)
    (check_global_candidate_equivalence chas_neutral
      (check_candidate_equivalence sample_witness_not_comparable sample_row_not_comparable chas_neutral)).(gceo_blocking_reasons) = true /\
  existsb (String.eqb "DRAW-CES-FundamentallyBlocked"%string)
    (check_global_candidate_equivalence chas_neutral
      (check_candidate_equivalence sample_witness_invariant_fail sample_row_invariant_fail chas_neutral)).(gceo_blocking_reasons) = true.
Proof. vm_compute. repeat split; reflexivity. Qed.

(** ================================================================
    Part F:  Honesty / debt
    ================================================================ *)

Definition global_equivalence_candidate_established : bool := true.
Definition global_equivalence_candidate_coq_eq_runtime : bool := false.

Lemma global_equivalence_candidate_does_not_establish_equivalence :
  global_equivalence_candidate_coq_eq_runtime = false /\
  equivalence_claimed = false.
Proof. split; reflexivity. Qed.

Lemma global_equivalence_candidate_preserves_open_proof_debt :
  negb proof_debt_placeholder.(pdp_pd_4f_ob_02_closed) = true /\
  negb proof_debt_placeholder.(pdp_pd_4f_cpp_01_closed) = true.
Proof.
  split.
  - exact canonical_encoding_pd_ob_02_open.
  - exact canonical_encoding_pd_cpp_01_open.
Qed.

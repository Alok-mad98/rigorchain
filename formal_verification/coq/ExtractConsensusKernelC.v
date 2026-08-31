(** ExtractConsensusKernelC.v — C ABI packaging of the P049/P050 kernel.

    NOT listed in _CoqProject. Re-running [coqc] rewrites consensus_kernel_c.ml.

    Surface aliases pack existing executable definitions into C-ABI-shaped
    records. They do not introduce a second epoch, schedule, draw, or
    classifier.

    No [Extract Constant] / [Extract Inductive] overrides.
    [ExtrOcamlNatInt]: [nat] as machine [int] (script bytes 0..255).
    [Z] stays the extracted Coq integer.

    Coq 8.15 has no [Print Extraction] vernacular. The extraction inventory
    is the [Extraction] command at the bottom of this file.

    Not a refinement. Honesty flags stay false. *)

From Coq Require Import Extraction ExtrOcamlBasic ExtrOcamlNatInt.
From Coq Require Import ZArith.ZArith.
From Coq Require Import List.
Import ListNotations.

From Rigor Require Import ResSchedule.
Require Import Rigor.legacy_account_model.Types.
Require Import Rigor.legacy_account_model.Selection.
Require Import Rigor.Sigma.
From Rigor.FeeBurn Require Import Accounting.
From RigorV01 Require Import IssuanceHeightBoundary.
From RigorV01 Require Import CanonicalIssuance.
From RigorV01 Require Import AssetClassification.
From RigorV01 Require Import MintSupplyConservation.
From RigorV01 Require Import WindowGeneric.

Local Open Scope Z_scope.

Record rigor_issuance_result := {
  ri_oru_subsidy : Z;
  ri_res_reward_schedule : Z;
  ri_epoch : Z
}.

(** Packaging of [issuance_epoch] / [oru_subsidy] / [get_res_mint_reward]. *)
Definition rigor_compute_issuance (h : Z) : rigor_issuance_result :=
  {| ri_oru_subsidy := oru_subsidy h;
     ri_res_reward_schedule := get_res_mint_reward h;
     ri_epoch := issuance_epoch h |}.

Definition compute_rigor_issuance (h : Z) : (Z * Z * Z) :=
  (issuance_epoch h, oru_subsidy h, get_res_mint_reward h).

Definition rigor_res_coinbase_expected (fees_res bootstrap mint : Z) : Z :=
  fees_res + bootstrap + mint.

Definition ledger_oru_delta (subsidy converge : Z) : Z :=
  subsidy - converge.

Definition eligible_with_T_bool (t : Tx) (C B : list byte) (h T : Z) : bool :=
  if Z_lt_dec (Sigma t C B h) (T * weight t) then true else false.

Definition resolve_convergence_draw
  (F : list Tx) (C B : list byte) (h T : Z) : option Tx :=
  select_winner (filter (fun t => eligible_with_T_bool t C B h T) F) C B h.

Extraction Language OCaml.
Extraction "consensus_kernel_c.ml"
  rigor_compute_issuance
  compute_rigor_issuance
  issuance_epoch
  oru_subsidy
  get_res_mint_reward
  res_issued_at_height
  COINBASE_MATURITY
  coinbase_mature
  classify
  apply_delta
  genesis_supply
  coinbase_oru_cap
  ledger_oru_delta
  rigor_res_coinbase_expected
  RES_PROBABILITY_TARGET
  select_winner
  resolve_convergence_draw.

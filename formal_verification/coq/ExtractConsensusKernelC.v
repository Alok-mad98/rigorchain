(** ExtractConsensusKernelC.v — C ABI packaging (RFC-019).

    NOT listed in _CoqProject. Re-running [coqc] rewrites consensus_kernel_c.ml.

    Issuance is (oru_subsidy, res_destroy_cost_at_height(h), epoch). There is no W(h) schedule
    field and no draw extraction. Coinbase maturity is the 100-block
    constant. Classify is the live AssetClassification function.

    No [Extract Constant] / [Extract Inductive] overrides.
    [ExtrOcamlNatInt]: [nat] as machine [int] (script bytes 0..255).
    [Z] stays the extracted Coq integer.

    Not a refinement. Honesty flags stay false. *)

From Coq Require Import Extraction ExtrOcamlBasic ExtrOcamlNatInt.
From Coq Require Import ZArith.ZArith.
From Coq Require Import List.
Import ListNotations.

From Rigor Require Import ResSchedule.
From RigorV01 Require Import IssuanceHeightBoundary.
From RigorV01 Require Import AssetClassification.

Local Open Scope Z_scope.

Record rigor_issuance_result := {
  ri_oru_subsidy : Z;
  ri_res_destroy_cost : Z;
  ri_epoch : Z
}.

Definition rigor_compute_issuance (h : Z) : rigor_issuance_result :=
  {| ri_oru_subsidy := oru_subsidy h;
     ri_res_destroy_cost := res_destroy_cost_at_height h;
     ri_epoch := issuance_epoch h |}.

Definition compute_rigor_issuance (h : Z) : (Z * Z * Z) :=
  (issuance_epoch h, oru_subsidy h, res_destroy_cost_at_height h).

Definition COINBASE_MATURITY : Z := 100.

Definition coinbase_mature (spend_height coin_height : Z) : bool :=
  spend_height - coin_height >=? COINBASE_MATURITY.

Definition ledger_oru_delta (subsidy converge : Z) : Z :=
  subsidy - converge.

Extraction Language OCaml.
Extraction "consensus_kernel_c.ml"
  rigor_compute_issuance
  compute_rigor_issuance
  issuance_epoch
  oru_subsidy
  res_destroy_cost_at_height
  COINBASE_MATURITY
  coinbase_mature
  classify
  ledger_oru_delta.

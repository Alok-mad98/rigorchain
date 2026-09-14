(** ExtractConsensusKernel.v — P050, rebased to RFC-019.

    NOT listed in _CoqProject. Re-running [coqc] rewrites consensus_kernel.ml.
    Aligns with ExtractConsensusKernelC.v: issuance is
    (epoch, oru_subsidy, res_destroy_cost_at_height(h)). There is no W(h) schedule field and no
    draw extraction.

    apply_delta / genesis_supply live in MintSupplyConservation (RFC-020
    conservation slice, no WindowGeneric). They are extracted by
    ExtractClassifySupply.v, not here.
    Coinbase maturity is the 100-block constant. Classify is the live
    AssetClassification function.

    No [Extract Constant] / [Extract Inductive] overrides.
    [ExtrOcamlNatInt]: [nat] as machine [int] (script bytes 0..255).
    [Z] stays the extracted Coq integer.

    Not a refinement. Honesty flags stay false. *)

From Coq Require Import Extraction ExtrOcamlBasic ExtrOcamlNatInt.
From Coq Require Import ZArith.ZArith.
From Coq Require Import List.
Import ListNotations.

From Rigor Require Import ResSchedule.
From Rigor.FeeBurn Require Import Accounting.
From RigorV01 Require Import IssuanceHeightBoundary.
From RigorV01 Require Import AssetClassification.

Local Open Scope Z_scope.

(** C++ [ComputeRigorIssuance] plus res_destroy_cost_at_height(h). Third component is a price. *)
Definition compute_rigor_issuance (h : Z) : (Z * Z * Z) :=
  (issuance_epoch h, oru_subsidy h, res_destroy_cost_at_height h).

(** C++ [RigorResCoinbaseExpected]: fees + bootstrap. No mint term. *)
Definition rigor_res_coinbase_expected (fees_res bootstrap : Z) : Z :=
  fees_res + bootstrap.

(** RFC-002 ledger Δ. Maps to C++ [RigorSupplyDelta.oru_circulating_delta]. *)
Definition ledger_oru_delta (subsidy converge : Z) : Z :=
  subsidy - converge.

Definition COINBASE_MATURITY : Z := 100.

Definition coinbase_mature (spend_height coin_height : Z) : bool :=
  spend_height - coin_height >=? COINBASE_MATURITY.

(** RFC-019: no scheduled mint quantity. Actual issuance is a ledger
    observation. Genesis is 0; the extractor reports 0 at every height. *)
Definition res_issued_at_height (_ : Z) : Z := 0.

Extraction Language OCaml.
Extraction "consensus_kernel.ml"
  issuance_epoch
  oru_subsidy
  res_destroy_cost_at_height
  res_issued_at_height
  compute_rigor_issuance
  COINBASE_MATURITY
  coinbase_mature
  classify
  coinbase_oru_cap
  ledger_oru_delta
  rigor_res_coinbase_expected.

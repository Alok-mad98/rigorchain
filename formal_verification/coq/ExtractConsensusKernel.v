(** ExtractConsensusKernel.v — P050. NOT listed in _CoqProject.

    Extracts the P049-aligned Consensus Kernel. Re-running [coqc] rewrites
    the OCaml sources; the default tree build does not compile this file.

    Surface aliases below are packaging / composition of existing executable
    definitions. They do not introduce a second epoch, schedule, draw, or
    classifier.

    No [Extract Constant] / [Extract Inductive] overrides.
    [ExtrOcamlNatInt]: [nat] as machine [int] (script bytes 0..255).
    [Z] stays the extracted Coq integer (arbitrary precision). Required:
    σ and T do not fit in OCaml [int].

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

(** C++ [ComputeRigorIssuance]: epoch, ORU schedule, RES schedule. *)
Definition compute_rigor_issuance (h : Z) : (Z * Z * Z) :=
  (issuance_epoch h, oru_subsidy h, get_res_mint_reward h).

(** C++ [RigorResCoinbaseExpected]: fees + bootstrap + mint. Predicate, not a builder. *)
Definition rigor_res_coinbase_expected (fees_res bootstrap mint : Z) : Z :=
  fees_res + bootstrap + mint.

(** RFC-002 ledger Δ. Same identity as [BlockSupply.ledger_oru_delta].
    Maps to C++ [RigorSupplyDelta.oru_circulating_delta], not Coq circulating. *)
Definition ledger_oru_delta (subsidy converge : Z) : Z :=
  subsidy - converge.

(** [ResolveConvergenceDraw(F,T)] surface: eligible filter then [select_winner].
    Does not assemble F from chain/UTXO. *)
Definition eligible_with_T_bool (t : Tx) (C B : list byte) (h T : Z) : bool :=
  if Z_lt_dec (Sigma t C B h) (T * weight t) then true else false.

Definition resolve_convergence_draw
  (F : list Tx) (C B : list byte) (h T : Z) : option Tx :=
  select_winner (filter (fun t => eligible_with_T_bool t C B h T) F) C B h.

Extraction Language OCaml.
Extraction "consensus_kernel.ml"
  issuance_epoch
  oru_subsidy
  get_res_mint_reward
  res_issued_at_height
  compute_rigor_issuance
  COINBASE_MATURITY
  coinbase_mature
  classify
  apply_delta
  genesis_supply
  coinbase_oru_cap
  ledger_oru_delta
  rigor_res_coinbase_expected
  RES_PROBABILITY_TARGET
  RES_REWARD_GENESIS
  select_winner
  resolve_convergence_draw.

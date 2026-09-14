(** RigorKernel.Supply.ORUModel — Phase A ORU accounting model.

    Scope: consensus kernel only.

    Fee is always ORU-denominated. RES value never enters the fee.
    RFC-018 burn: floor(fee / 2), only on the RES-transfer domain.

    Claimable identity (mirrors core/src/consensus/rigor_supply.cpp:45):
      oru_claimable = (fees_oru - fee_burn) + subsidy

    NOT CLAIMED:
      C++ implementation equivalence
      Full chain validity
      RES issuance soundness (out of Phase A)
*)
From Coq Require Import List.
From Coq Require Import ZArith.ZArith.
From Coq Require Import Lia.
Import ListNotations.
From RigorKernel Require Import Core.Amount.

Local Open Scope Z_scope.

(** Input-domain of a non-coinbase transaction (RFC-003):
    any RES prevout ⇒ RES domain; ORU-only ⇒ ORU domain. *)
Inductive input_domain := DomORU | DomRES.

(** mirrors core/src/consensus/rigor_consensus_lock.h
    ORU_RES_TRANSFER_FEE_BURN_NUM / DEN *)
Definition BURN_NUM : Z := 1.
Definition BURN_DEN : Z := 2.

Definition burn_share (fee : Z) : Z :=
  if fee <=? 0 then 0 else (fee * BURN_NUM) / BURN_DEN.

Definition tx_burn (dom : input_domain) (fee : Z) : Z :=
  match dom with
  | DomRES => burn_share fee
  | DomORU => 0
  end.

Definition tx_claimable_share (dom : input_domain) (fee : Z) : Z :=
  fee - tx_burn dom fee.

(** Block-level claimable ORU (C++ [EvaluateSupply].oru_claimable). *)
Definition oru_claimable (fees_oru fee_burn subsidy : Z) : Z :=
  (fees_oru - fee_burn) + subsidy.

Record oru_tx := {
  tx_dom : input_domain;
  oru_in : Z;
  oru_out : Z;
  res_in : Z;
  res_out : Z
}.

Definition tx_fee (t : oru_tx) : Z := oru_in t - oru_out t.

Definition valid_oru_tx (t : oru_tx) : Prop :=
  0 <= oru_in t /\
  0 <= oru_out t /\
  0 <= res_in t /\
  0 <= res_out t /\
  oru_out t <= oru_in t /\
  match tx_dom t with
  | DomORU => res_in t = 0 /\ res_out t = 0
  | DomRES => 0 < oru_in t /\ res_out t <= res_in t
  end.

Fixpoint sum_fees (ts : list oru_tx) : Z :=
  match ts with
  | nil => 0
  | t :: rest => tx_fee t + sum_fees rest
  end.

Fixpoint sum_burns (ts : list oru_tx) : Z :=
  match ts with
  | nil => 0
  | t :: rest => tx_burn (tx_dom t) (tx_fee t) + sum_burns rest
  end.

(** Supply increment under a maximal coinbase claim. *)
Definition ORU_supply_transition (old_supply subsidy fees burn : Z) : Z :=
  old_supply + subsidy + fees - burn.

(** First-class ORU economic domain. Coq kernel model only.

    Fees, subsidy, burn, and transfer are ORU-domain quantities.
    RES issuance is not modelled here.

    NOT CLAIMED: C++ EvaluateSupply, a full monetary system, node.
*)
From Coq Require Import ZArith.ZArith.

Local Open Scope Z_scope.

Record ORUState := {
  oru_stock : Z
}.

Record ORUValueFlow := {
  ovf_incoming : Z;
  ovf_outgoing : Z;
  ovf_fees : Z;
  ovf_subsidy : Z;
  ovf_burn : Z
}.

Definition old_value (s : ORUState) : Z := oru_stock s.

Definition incoming_value (f : ORUValueFlow) : Z :=
  ovf_subsidy f + ovf_incoming f.

Definition burned_value (f : ORUValueFlow) : Z := ovf_burn f.

Definition outgoing_value (f : ORUValueFlow) (post : ORUState) : Z :=
  ovf_outgoing f + oru_stock post.

Definition valid_oru_flow (f : ORUValueFlow) : Prop :=
  0 <= ovf_incoming f /\
  0 <= ovf_outgoing f /\
  0 <= ovf_fees f /\
  0 <= ovf_subsidy f /\
  0 <= ovf_burn f.

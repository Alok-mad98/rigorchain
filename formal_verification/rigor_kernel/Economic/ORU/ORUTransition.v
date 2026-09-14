(** ORU domain transition. Fees are not a mint term.

    M26 adds [ovf_fees] as a creation term.

    NOT CLAIMED: C++ ConnectBlock, RES substitution, node accounting.
*)
From Coq Require Import ZArith.ZArith.
From RigorKernel Require Import Economic.ORU.ORUDomain.

Local Open Scope Z_scope.

Record ORUTransition := {
  ot_pre : ORUState;
  ot_flow : ORUValueFlow;
  ot_post : ORUState
}.

(** Honest ORU apply. Fees are already in the in/out residual. *)
Definition oru_apply (pre : ORUState) (f : ORUValueFlow) : ORUState :=
  {| oru_stock :=
       oru_stock pre + ovf_subsidy f + ovf_incoming f
         - ovf_outgoing f - ovf_burn f |}.

Definition oru_step_of (pre : ORUState) (f : ORUValueFlow) : ORUTransition :=
  {| ot_pre := pre; ot_flow := f; ot_post := oru_apply pre f |}.

(** Transfer residual (no subsidy). M27 adds a hidden [+1]. *)
Definition oru_move_delta (f : ORUValueFlow) : Z :=
  ovf_incoming f - ovf_outgoing f - ovf_burn f.

Definition oru_move (pre : ORUState) (f : ORUValueFlow) : ORUState :=
  {| oru_stock := oru_stock pre + oru_move_delta f |}.

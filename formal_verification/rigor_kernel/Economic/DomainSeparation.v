(** RES creation and ORU transfer are not substitutes.

    RES: controlled mint into the RES pool.
    ORU: moves existing value (plus optional ORU subsidy).

    NOT CLAIMED: C++ dual-pool equivalence, full monetary system.
*)
From Coq Require Import ZArith.ZArith.
From Coq Require Import Lia.
From RigorKernel Require Import Economic.ORU.ORUDomain.
From RigorKernel Require Import Economic.ORU.ORUTransition.

Local Open Scope Z_scope.

Record DualPools := {
  dp_oru : Z;
  dp_res : Z
}.

Definition apply_domains (pre : DualPools) (f : ORUValueFlow) (res_mint : Z)
    : DualPools :=
  {| dp_oru := dp_oru pre + ovf_subsidy f + oru_move_delta f;
     dp_res := dp_res pre + res_mint |}.

Theorem res_creation_separate_from_oru_transfer :
  forall (pre : DualPools) (f : ORUValueFlow) (res_mint : Z),
    dp_oru (apply_domains pre f res_mint)
      = dp_oru (apply_domains pre f 0) /\
    dp_res (apply_domains pre f res_mint)
      = dp_res pre + res_mint /\
    dp_oru (apply_domains pre f res_mint)
      = dp_oru pre + ovf_subsidy f + oru_move_delta f /\
    oru_move_delta f + ovf_outgoing f + ovf_burn f = ovf_incoming f.
Proof.
  intros pre f res_mint.
  unfold apply_domains, oru_move_delta. simpl.
  split; [lia|].
  split; [lia|].
  split; [lia|].
  lia.
Qed.

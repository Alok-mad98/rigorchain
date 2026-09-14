(** RigorSurface.Supply.KernelTx — [kernel_tx] / [to_oru] slice.

    Lifted out of [proofs/ORU_Conservation/ORUConservation.v].
    No theorems.
*)
From Coq Require Import ZArith.ZArith.
From RigorSurface Require Import Supply.ORUModel.

Local Open Scope Z_scope.

Record kernel_tx := {
  kt_dom : input_domain;
  kt_oru_in : Z;
  kt_oru_out : Z;
  kt_res_in : Z;
  kt_res_out : Z;
  kt_old_supply : Z;
  kt_subsidy : Z
}.

Definition to_oru (t : kernel_tx) : oru_tx :=
  {| tx_dom := kt_dom t;
     oru_in := kt_oru_in t;
     oru_out := kt_oru_out t;
     res_in := kt_res_in t;
     res_out := kt_res_out t |}.

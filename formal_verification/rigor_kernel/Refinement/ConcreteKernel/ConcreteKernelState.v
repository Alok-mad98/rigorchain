(** Concrete kernel *model* state for Coq-to-Coq refinement.

    [AbstractKernelState] is the unified kernel state.
    [ConcreteKernelState] is a flattened Coq model of the same
    fields. Neither is a C++ type or a node.

    NOT CLAIMED: C++ refinement, node, compiler correctness.
*)
From Coq Require Import ZArith.ZArith.
From RigorKernel Require Import Kernel.UnifiedState.

Local Open Scope Z_scope.

Definition AbstractKernelState := KernelState.

Record ConcreteKernelState := {
  ck_utxo_flag : bool;
  ck_fees : Z;
  ck_burn : Z;
  ck_subsidy : Z;
  ck_res_stock : Z;
  ck_res_cap : Z;
  ck_oru_stock : Z;
  ck_script_ok : bool;
  ck_pow_ok : bool;
  ck_asert_ok : bool
}.

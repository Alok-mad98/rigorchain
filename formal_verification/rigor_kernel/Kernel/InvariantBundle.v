(** Global kernel invariant bundle.

    Conjoins the six domain invariants over [KernelState].

    NOT CLAIMED: node safety, C++ ConnectBlock, compiler.
*)
From Coq Require Import ZArith.ZArith.
From RigorKernel Require Import Kernel.UnifiedState.

Local Open Scope Z_scope.

Definition utxo_sound (s : KernelState) : Prop :=
  utxo_wf_flag (ks_utxo s) = true.

Definition oru_conservation (s : KernelState) : Prop :=
  0 <= oru_stock (ks_oru s).

Definition res_bound (s : KernelState) : Prop :=
  0 <= res_stock (ks_res s) <= res_cap (ks_res s).

Definition script_classification (s : KernelState) : Prop :=
  script_class_ok (ks_script s) = true.

Definition pow_validity (s : KernelState) : Prop :=
  pow_ok (ks_pow s) = true.

Definition asert_boundary (s : KernelState) : Prop :=
  asert_ok (ks_asert s) = true.

Definition kernel_invariants (s : KernelState) : Prop :=
  utxo_sound s /\
  oru_conservation s /\
  res_bound s /\
  script_classification s /\
  pow_validity s /\
  asert_boundary s.

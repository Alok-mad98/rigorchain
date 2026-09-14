(** Executable kernel *model* state. Projects onto the concrete kernel.

    [ConcreteKernelState] is the Coq concrete record.
    [ExecutableKernelState] is a flattened executable-model twin.
    Neither is C++ or a node.

    NOT CLAIMED: production refinement, compiler, binary equivalence.
*)
From Coq Require Import Bool.Bool.
From Coq Require Import ZArith.ZArith.
From RigorKernel Require Import Refinement.ConcreteKernel.ConcreteKernelState.
From RigorKernel Require Import Refinement.ConcreteKernel.ConcreteKernelMapping.

Local Open Scope Z_scope.

Record ExecutableKernelState := {
  ek_utxo_flag : bool;
  ek_fees : Z;
  ek_burn : Z;
  ek_subsidy : Z;
  ek_res_stock : Z;
  ek_res_cap : Z;
  ek_oru_stock : Z;
  ek_script_ok : bool;
  ek_pow_ok : bool;
  ek_asert_ok : bool
}.

(** Named so M33 can flip the projected UTXO flag. *)
Definition exec_project_utxo (e : ExecutableKernelState) : bool :=
  ek_utxo_flag e.

Definition execution_projection (e : ExecutableKernelState) : ConcreteKernelState :=
  {| ck_utxo_flag := exec_project_utxo e;
     ck_fees := ek_fees e;
     ck_burn := ek_burn e;
     ck_subsidy := ek_subsidy e;
     ck_res_stock := ek_res_stock e;
     ck_res_cap := ek_res_cap e;
     ck_oru_stock := ek_oru_stock e;
     ck_script_ok := ek_script_ok e;
     ck_pow_ok := ek_pow_ok e;
     ck_asert_ok := ek_asert_ok e |}.

Definition embed_concrete (c : ConcreteKernelState) : ExecutableKernelState :=
  {| ek_utxo_flag := ck_utxo_flag c;
     ek_fees := ck_fees c;
     ek_burn := ck_burn c;
     ek_subsidy := ck_subsidy c;
     ek_res_stock := ck_res_stock c;
     ek_res_cap := ck_res_cap c;
     ek_oru_stock := ck_oru_stock c;
     ek_script_ok := ck_script_ok c;
     ek_pow_ok := ck_pow_ok c;
     ek_asert_ok := ck_asert_ok c |}.

Definition executable_invariants (e : ExecutableKernelState) : Prop :=
  ek_utxo_flag e = true /\
  0 <= ek_oru_stock e /\
  0 <= ek_res_stock e <= ek_res_cap e /\
  ek_script_ok e = true /\
  ek_pow_ok e = true /\
  ek_asert_ok e = true.

Definition check_executable_kernel (e : ExecutableKernelState) : bool :=
  andb (ek_utxo_flag e)
    (andb (Z.leb 0 (ek_oru_stock e))
      (andb (Z.leb 0 (ek_res_stock e))
        (andb (Z.leb (ek_res_stock e) (ek_res_cap e))
          (andb (ek_script_ok e)
            (andb (ek_pow_ok e) (ek_asert_ok e)))))).

Theorem execution_projection_preserves_invariants :
  forall (e : ExecutableKernelState),
    executable_invariants e ->
    concrete_invariants (execution_projection e).
Proof.
  intros e Hinv.
  unfold executable_invariants, concrete_invariants,
         execution_projection, exec_project_utxo in *.
  simpl.
  exact Hinv.
Qed.

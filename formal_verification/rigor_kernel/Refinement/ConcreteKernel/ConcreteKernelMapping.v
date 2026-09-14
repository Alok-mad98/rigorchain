(** Projection: concrete model state → unified abstract kernel.

    [kernel_projection] is Coq-to-Coq. It does not observe C++.

    NOT CLAIMED: implementation refinement, compiler, binary.
*)
From Coq Require Import Bool.Bool.
From Coq Require Import ZArith.ZArith.
From RigorKernel Require Import Kernel.UnifiedState.
From RigorKernel Require Import Kernel.InvariantBundle.
From RigorKernel Require Import Refinement.ConcreteKernel.ConcreteKernelState.

Local Open Scope Z_scope.

(** Named so M31 can flip the projected UTXO flag. *)
Definition project_utxo_flag (c : ConcreteKernelState) : bool :=
  ck_utxo_flag c.

Definition kernel_projection (c : ConcreteKernelState) : AbstractKernelState :=
  {| ks_utxo := {| utxo_wf_flag := project_utxo_flag c |};
     ks_econ := {| econ_fees := ck_fees c;
                   econ_burn := ck_burn c;
                   econ_subsidy := ck_subsidy c |};
     ks_res := {| res_stock := ck_res_stock c;
                  res_cap := ck_res_cap c |};
     ks_oru := {| oru_stock := ck_oru_stock c |};
     ks_script := {| script_class_ok := ck_script_ok c |};
     ks_pow := {| pow_ok := ck_pow_ok c |};
     ks_asert := {| asert_ok := ck_asert_ok c |} |}.

Definition concrete_invariants (c : ConcreteKernelState) : Prop :=
  ck_utxo_flag c = true /\
  0 <= ck_oru_stock c /\
  0 <= ck_res_stock c <= ck_res_cap c /\
  ck_script_ok c = true /\
  ck_pow_ok c = true /\
  ck_asert_ok c = true.

Theorem concrete_state_preserves_invariant :
  forall (c : ConcreteKernelState) (a : AbstractKernelState),
    kernel_invariants a ->
    kernel_projection c = a ->
    concrete_invariants c.
Proof.
  intros c a Hinv Hp.
  rewrite <- Hp in Hinv.
  unfold kernel_invariants, kernel_projection, project_utxo_flag,
         concrete_invariants, utxo_sound, oru_conservation, res_bound,
         script_classification, pow_validity, asert_boundary in Hinv.
  simpl in Hinv.
  exact Hinv.
Qed.

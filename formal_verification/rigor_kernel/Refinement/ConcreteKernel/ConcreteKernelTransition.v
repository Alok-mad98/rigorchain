(** Concrete kernel step, refined onto the unified abstract step.

    [concrete_kernel_step] is a Coq-model transition. A successful
    concrete step projects to [kernel_step].

    NOT CLAIMED: C++ ConnectBlock, node, CompCert.
*)
From Coq Require Import ZArith.ZArith.
From RigorKernel Require Import Kernel.UnifiedState.
From RigorKernel Require Import Kernel.UnifiedTransition.
From RigorKernel Require Import Refinement.ConcreteKernel.ConcreteKernelState.
From RigorKernel Require Import Refinement.ConcreteKernel.ConcreteKernelMapping.

Local Open Scope Z_scope.

Definition concrete_step_of (c : ConcreteKernelState) (i : KernelInput)
  : ConcreteKernelState :=
  let a' := kernel_step_of (kernel_projection c) i in
  {| ck_utxo_flag := utxo_wf_flag (ks_utxo a');
     ck_fees := econ_fees (ks_econ a');
     ck_burn := econ_burn (ks_econ a');
     ck_subsidy := econ_subsidy (ks_econ a');
     ck_res_stock := res_stock (ks_res a');
     ck_res_cap := res_cap (ks_res a');
     ck_oru_stock := oru_stock (ks_oru a');
     ck_script_ok := script_class_ok (ks_script a');
     ck_pow_ok := pow_ok (ks_pow a');
     ck_asert_ok := asert_ok (ks_asert a') |}.

Inductive concrete_kernel_apply :
    ConcreteKernelState -> KernelInput -> ConcreteKernelState -> Prop :=
| ckapp_fun : forall c i,
    input_ok (kernel_projection c) i ->
    concrete_kernel_apply c i (concrete_step_of c i).

Definition concrete_kernel_step
    (c c' : ConcreteKernelState) : Prop :=
  exists i : KernelInput, concrete_kernel_apply c i c'.

Lemma projection_step_commute :
  forall (c : ConcreteKernelState) (i : KernelInput),
    kernel_projection (concrete_step_of c i)
      = kernel_step_of (kernel_projection c) i.
Proof.
  intros c i.
  remember (kernel_step_of (kernel_projection c) i) as a' eqn:Ha'.
  unfold concrete_step_of.
  rewrite <- Ha'.
  unfold kernel_projection, project_utxo_flag. simpl.
  destruct a' as [u e r o sc p ae].
  destruct u, e, r, o, sc, p, ae.
  reflexivity.
Qed.

Theorem concrete_step_refines_abstract_step :
  forall (c c' : ConcreteKernelState),
    concrete_kernel_step c c' ->
    kernel_step (kernel_projection c) (kernel_projection c').
Proof.
  intros c c' [i Happ].
  inversion Happ; subst.
  exists i.
  rewrite projection_step_commute.
  apply kapp_fun.
  assumption.
Qed.

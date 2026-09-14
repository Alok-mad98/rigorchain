(** Executable kernel step, refined onto the concrete kernel step.

    [executable_kernel_step] is a Coq-model transition. A successful
    executable step projects to [concrete_kernel_step].

    NOT CLAIMED: C++ ConnectBlock, node, CompCert.
*)
From Coq Require Import Bool.Bool.
From Coq Require Import ZArith.ZArith.
From RigorKernel Require Import Kernel.UnifiedTransition.
From RigorKernel Require Import Refinement.ConcreteKernel.ConcreteKernelState.
From RigorKernel Require Import Refinement.ConcreteKernel.ConcreteKernelMapping.
From RigorKernel Require Import Refinement.ConcreteKernel.ConcreteKernelTransition.
From RigorKernel Require Import ExecutionBoundary.ConcreteExecutionState.

Local Open Scope Z_scope.

Definition executable_step_of (e : ExecutableKernelState) (i : KernelInput)
  : ExecutableKernelState :=
  embed_concrete (concrete_step_of (execution_projection e) i).

Inductive executable_kernel_apply :
    ExecutableKernelState -> KernelInput -> ExecutableKernelState -> Prop :=
| ekapp_fun : forall e i,
    input_ok (kernel_projection (execution_projection e)) i ->
    executable_kernel_apply e i (executable_step_of e i).

Definition executable_kernel_step
    (s s' : ExecutableKernelState) : Prop :=
  exists i : KernelInput, executable_kernel_apply s i s'.

(** * The relation is the graph of the step function

    [executable_kernel_apply] used to carry a second constructor, guarded by a
    constant [executable_step_unique = true] that only a mutant ever set to
    [false]. The constructor was unreachable in the healthy tree, and
    its only effect was to make determinism look like a property of
    the kernel rules. It was not: the rules were already a function.

    What is worth stating is the correspondence itself. Everything
    the relation can do on an input the kernel accepts, the function
    does, and nothing else. Determinism follows from this rather than
    from the absence of an alternative constructor. *)

Theorem executable_apply_is_graph :
  forall (e e' : ExecutableKernelState) (i : KernelInput),
    executable_kernel_apply e i e' <->
    input_ok (kernel_projection (execution_projection e)) i
      /\ e' = executable_step_of e i.
Proof.
  intros e e' i. split.
  - intros H. inversion H; subst. split; [assumption | reflexivity].
  - intros [Hok ->]. apply ekapp_fun, Hok.
Qed.

Lemma execution_step_commute :
  forall (e : ExecutableKernelState) (i : KernelInput),
    execution_projection (executable_step_of e i)
      = concrete_step_of (execution_projection e) i.
Proof.
  intros e i.
  unfold executable_step_of, embed_concrete, execution_projection,
         exec_project_utxo.
  simpl.
  destruct (concrete_step_of (execution_projection e) i).
  reflexivity.
Qed.

Theorem executable_step_refines_concrete_step :
  forall (s s' : ExecutableKernelState),
    executable_kernel_step s s' ->
    concrete_kernel_step (execution_projection s) (execution_projection s').
Proof.
  intros s s' [i Happ].
  apply executable_apply_is_graph in Happ as [Hok ->].
  exists i.
  rewrite execution_step_commute.
  apply ckapp_fun, Hok.
Qed.

Theorem executable_transition_deterministic :
  forall s i s1 s2,
    executable_kernel_apply s i s1 ->
    executable_kernel_apply s i s2 ->
    s1 = s2.
Proof.
  intros s i s1 s2 H1 H2.
  apply executable_apply_is_graph in H1 as [_ ->].
  apply executable_apply_is_graph in H2 as [_ ->].
  reflexivity.
Qed.

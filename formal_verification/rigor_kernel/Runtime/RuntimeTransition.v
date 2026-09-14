(** Coq runtime-model transition. Refines [executable_kernel_step].

    Not an OCaml runtime step and not a CPU transition.

    NOT CLAIMED: ocamlopt, ABI, native binary.
*)
From Coq Require Import Bool.Bool.
From Coq Require Import ZArith.ZArith.
From RigorKernel Require Import Kernel.UnifiedTransition.
From RigorKernel Require Import Refinement.ConcreteKernel.ConcreteKernelMapping.
From RigorKernel Require Import ExecutionBoundary.ConcreteExecutionState.
From RigorKernel Require Import ExecutionBoundary.ExecutableTransition.
From RigorKernel Require Import Runtime.RuntimeState.

Local Open Scope Z_scope.

Definition runtime_step_of (r : RuntimeKernelState) (i : RuntimeKernelInput)
  : RuntimeKernelState :=
  embed_executable (executable_step_of (runtime_projection r) i) (rk_entry r).

Inductive runtime_apply :
    RuntimeKernelState -> RuntimeKernelInput -> RuntimeKernelState -> Prop :=
| rapp_fun : forall r i,
    input_ok (kernel_projection (execution_projection (runtime_projection r))) i ->
    runtime_apply r i (runtime_step_of r i).

Definition runtime_step (r r' : RuntimeKernelState) : Prop :=
  exists i : RuntimeKernelInput, runtime_apply r i r'.

(** * The relation is the graph of the step function

    [runtime_apply] used to carry a second constructor, guarded by a
    constant [runtime_step_unique = true] that only a mutant ever set to
    [false]. The constructor was unreachable in the healthy tree, and
    its only effect was to make determinism look like a property of
    the kernel rules. It was not: the rules were already a function.

    What is worth stating is the correspondence itself. Everything
    the relation can do on an input the kernel accepts, the function
    does, and nothing else. Determinism follows from this rather than
    from the absence of an alternative constructor. *)

Theorem runtime_apply_is_graph :
  forall (r r' : RuntimeKernelState) (i : RuntimeKernelInput),
    runtime_apply r i r' <->
    input_ok (kernel_projection (execution_projection (runtime_projection r))) i
      /\ r' = runtime_step_of r i.
Proof.
  intros r r' i. split.
  - intros H. inversion H; subst. split; [assumption | reflexivity].
  - intros [Hok ->]. apply rapp_fun, Hok.
Qed.

Lemma runtime_step_commute :
  forall (r : RuntimeKernelState) (i : RuntimeKernelInput),
    runtime_projection (runtime_step_of r i)
      = executable_step_of (runtime_projection r) i.
Proof.
  intros r i.
  unfold runtime_step_of, embed_executable, runtime_projection,
         runtime_project_utxo.
  simpl.
  destruct (executable_step_of (runtime_projection r) i).
  reflexivity.
Qed.

Theorem runtime_step_refines_executable_step :
  forall r r' : RuntimeKernelState,
    runtime_step r r' ->
    executable_kernel_step (runtime_projection r) (runtime_projection r').
Proof.
  intros r r' [i Happ].
  apply runtime_apply_is_graph in Happ as [Hok ->].
  exists i.
  rewrite runtime_step_commute.
  apply ekapp_fun, Hok.
Qed.

Theorem runtime_step_deterministic :
  forall r i r1 r2,
    runtime_apply r i r1 ->
    runtime_apply r i r2 ->
    r1 = r2.
Proof.
  intros r i r1 r2 H1 H2.
  apply runtime_apply_is_graph in H1 as [_ ->].
  apply runtime_apply_is_graph in H2 as [_ ->].
  reflexivity.
Qed.

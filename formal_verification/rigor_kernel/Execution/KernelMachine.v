(** Small-step executable kernel machine. Preparation only.

    Prepares a future verified-compiler path. This is not CompCert,
    not machine code, and not a node.

    NOT CLAIMED: verified compiler, ABI, integer-width, C++.
*)
From Coq Require Import Arith.Arith.

Record KernelMachineState := {
  km_ok : bool;
  km_fuel : nat
}.

Inductive KernelInstruction :=
| KI_Nop
| KI_Accept
| KI_Reject.

Definition KernelTransition
    (s : KernelMachineState) (i : KernelInstruction) : KernelMachineState :=
  match i with
  | KI_Nop => s
  | KI_Accept => {| km_ok := true; km_fuel := km_fuel s |}
  | KI_Reject => {| km_ok := false; km_fuel := 0 |}
  end.

Definition machine_invariant (s : KernelMachineState) : Prop :=
  km_ok s = false -> km_fuel s = 0.

Inductive machine_step :
    KernelMachineState -> KernelInstruction -> KernelMachineState -> Prop :=
| mstep_fun : forall s i,
    machine_step s i (KernelTransition s i).

(** * The relation is the graph of [KernelTransition]

    A second constructor used to sit here behind a
    [machine_step_unique = true] constant that only a mutant ever
    cleared. It was unreachable, and it made determinism look like a
    fact about the machine rather than about a function. This states
    the correspondence instead, and determinism follows from it. *)

Theorem machine_step_is_graph :
  forall (s s' : KernelMachineState) (i : KernelInstruction),
    machine_step s i s' <-> s' = KernelTransition s i.
Proof.
  intros s s' i. split.
  - intros H. inversion H; subst. reflexivity.
  - intros ->. apply mstep_fun.
Qed.

Theorem machine_step_deterministic :
  forall s i s1 s2,
    machine_step s i s1 ->
    machine_step s i s2 ->
    s1 = s2.
Proof.
  intros s i s1 s2 H1 H2.
  apply machine_step_is_graph in H1 as ->.
  apply machine_step_is_graph in H2 as ->.
  reflexivity.
Qed.

Theorem machine_state_preserves_invariant :
  forall s i,
    machine_invariant s ->
    machine_invariant (KernelTransition s i).
Proof.
  intros [ok fuel] i Hinv.
  unfold machine_invariant, KernelTransition in *.
  destruct i; simpl.
  - exact Hinv.
  - discriminate.
  - reflexivity.
Qed.

(** Unified kernel transition. Model composition only.

    [kernel_step] is a relation on [KernelState]. Domain steps are
    composed; they are not production transitions.

    NOT CLAIMED: node, C++ refinement, compiler correctness.
*)
From Coq Require Import Bool.Bool.
From Coq Require Import ZArith.ZArith.
From Coq Require Import Lia.
From RigorKernel Require Import Kernel.UnifiedState.

Local Open Scope Z_scope.

Record KernelInput := {
  ki_utxo_ok : bool;
  ki_incoming : Z;
  ki_outgoing : Z;
  ki_fees : Z;
  ki_burn : Z;
  ki_subsidy : Z;
  ki_res_mint : Z;
  ki_script_ok : bool;
  ki_pow_ok : bool;
  ki_asert_ok : bool
}.

Definition utxo_transition (u : UTXOState) (i : KernelInput) : UTXOState :=
  {| utxo_wf_flag := andb (utxo_wf_flag u) (ki_utxo_ok i) |}.

(** Honest value-flow residual. M30 adds a hidden [+1]. *)
Definition oru_delta (i : KernelInput) : Z :=
  ki_subsidy i + ki_incoming i - ki_outgoing i - ki_burn i.

Definition oru_transition (o : ORUState) (i : KernelInput) : ORUState :=
  {| oru_stock := oru_stock o + oru_delta i |}.

Definition res_transition (r : RESState) (i : KernelInput) : RESState :=
  {| res_stock := Z.min (res_cap r) (res_stock r + Z.max 0 (ki_res_mint i));
     res_cap := res_cap r |}.

Definition script_transition (sc : ScriptState) (i : KernelInput) : ScriptState :=
  {| script_class_ok := andb (script_class_ok sc) (ki_script_ok i) |}.

Definition pow_transition (p : PowState) (i : KernelInput) : PowState :=
  {| pow_ok := andb (pow_ok p) (ki_pow_ok i) |}.

Definition asert_transition (a : ASERTState) (i : KernelInput) : ASERTState :=
  {| asert_ok := andb (asert_ok a) (ki_asert_ok i) |}.

Definition econ_transition (i : KernelInput) : EconomicState :=
  {| econ_fees := ki_fees i;
     econ_burn := ki_burn i;
     econ_subsidy := ki_subsidy i |}.

Definition kernel_step_of (s : KernelState) (i : KernelInput) : KernelState :=
  {| ks_utxo := utxo_transition (ks_utxo s) i;
     ks_econ := econ_transition i;
     ks_res := res_transition (ks_res s) i;
     ks_oru := oru_transition (ks_oru s) i;
     ks_script := script_transition (ks_script s) i;
     ks_pow := pow_transition (ks_pow s) i;
     ks_asert := asert_transition (ks_asert s) i |}.

Definition input_ok (s : KernelState) (i : KernelInput) : Prop :=
  ki_utxo_ok i = true /\
  ki_script_ok i = true /\
  ki_pow_ok i = true /\
  ki_asert_ok i = true /\
  0 <= ki_incoming i /\
  0 <= ki_outgoing i /\
  0 <= ki_fees i /\
  0 <= ki_burn i /\
  0 <= ki_subsidy i /\
  0 <= ki_res_mint i /\
  ki_outgoing i + ki_burn i
    <= oru_stock (ks_oru s) + ki_subsidy i + ki_incoming i /\
  0 <= res_cap (ks_res s) /\
  res_stock (ks_res s) <= res_cap (ks_res s).

Inductive kernel_apply :
    KernelState -> KernelInput -> KernelState -> Prop :=
| kapp_fun : forall s i,
    input_ok s i ->
    kernel_apply s i (kernel_step_of s i).

Definition kernel_step (s s' : KernelState) : Prop :=
  exists i : KernelInput, kernel_apply s i s'.

(** * The relation is the graph of the step function

    [kernel_apply] used to carry a second constructor, guarded by a
    constant [kernel_step_unique = true] that only a mutant ever set to
    [false]. The constructor was unreachable in the healthy tree, and
    its only effect was to make determinism look like a property of
    the kernel rules. It was not: the rules were already a function.

    What is worth stating is the correspondence itself. Everything
    the relation can do on an input the kernel accepts, the function
    does, and nothing else. Determinism follows from this rather than
    from the absence of an alternative constructor. *)

Theorem kernel_apply_is_graph :
  forall (s s' : KernelState) (i : KernelInput),
    kernel_apply s i s' <-> input_ok s i /\ s' = kernel_step_of s i.
Proof.
  intros s s' i. split.
  - intros H. inversion H; subst. split; [assumption | reflexivity].
  - intros [Hok ->]. apply kapp_fun, Hok.
Qed.

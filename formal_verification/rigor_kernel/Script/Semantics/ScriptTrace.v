(** Finite classification traces. Kernel subset only.

    A recognized script does not inhabit two consensus marker classes.
    This is not a Script VM.

    NOT CLAIMED: full Bitcoin Script, signatures, C++ refinement.
*)
From Coq Require Import Arith.Arith.
From Coq Require Import List.
From RigorKernel Require Import Core.Bytes.
From RigorKernel Require Import Script.ScriptTypes.
From RigorKernel Require Import Script.ScriptClassifier.
From RigorKernel Require Import Script.Semantics.ScriptState.
From RigorKernel Require Import Script.Semantics.ScriptStep.

Definition ScriptTrace := list ScriptState.

Inductive OpKind :=
| OK_PUSH
| OK_MARKER_MINT
| OK_MARKER_CONVERGE
| OK_OTHER.

(** Kernel opcode-class recognizer (markers only). *)
Definition classify_op (b : byte) : OpKind :=
  if Nat.eqb b RIGOR_MARKER_RES_MINT then OK_MARKER_MINT
  else if Nat.eqb b RIGOR_MARKER_CONVERGE then OK_MARKER_CONVERGE
  else if Nat.leb b RIGOR_PUSH_16 then OK_PUSH
  else OK_OTHER.

Theorem script_trace_deterministic :
  forall s st1 st2,
    ScriptStep {| st_script := s; st_class := T_NONSTANDARD |} st1 ->
    ScriptStep {| st_script := s; st_class := T_NONSTANDARD |} st2 ->
    st1 = st2.
Proof.
  intros s [s1 c1] [s2 c2] [H1a H1b] [H2a H2b].
  simpl in *. subst s1 s2 c1 c2. reflexivity.
Qed.

(** Mint and converge markers are distinct consensus classes. *)
Theorem recognizer_trace_disjoint :
  classify_op RIGOR_MARKER_RES_MINT <> classify_op RIGOR_MARKER_CONVERGE /\
  forall b,
    classify_op b = OK_MARKER_MINT ->
    classify_op b <> OK_MARKER_CONVERGE.
Proof.
  split.
  - unfold classify_op, RIGOR_MARKER_RES_MINT, RIGOR_MARKER_CONVERGE.
    simpl. discriminate.
  - intros b H. unfold classify_op in *.
    destruct (Nat.eqb b RIGOR_MARKER_RES_MINT); [discriminate|].
    destruct (Nat.eqb b RIGOR_MARKER_CONVERGE); congruence.
Qed.

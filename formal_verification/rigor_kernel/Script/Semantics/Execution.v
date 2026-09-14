(** Minimal kernel-subset execution. Not a Bitcoin Script VM.

    Small-step classification only. No opcode interpreter, sighash,
    or signature checks.

    NOT CLAIMED: Bitcoin Script, C++ EvalScript, compiler correctness.
*)
From Coq Require Import ZArith.ZArith.
From RigorKernel Require Import Script.ScriptTypes.
From RigorKernel Require Import Script.ScriptClassifier.

Inductive ExecState :=
| ES_Run (scr : Script) (cls : ScriptType)
| ES_Reject.

Inductive exec_step : ExecState -> ExecState -> Prop :=
| step_classify : forall scr cls,
    exec_step (ES_Run scr cls) (ES_Run scr (classify_script scr))
| step_stuck :
    exec_step ES_Reject ES_Reject.

Theorem script_step_deterministic :
  forall s s1 s2,
    exec_step s s1 ->
    exec_step s s2 ->
    s1 = s2.
Proof.
  intros s s1 s2 H1 H2.
  destruct H1; inversion H2; subst; reflexivity.
Qed.

Theorem script_execution_preserves_class :
  forall scr cls s',
    exec_step (ES_Run scr cls) s' ->
    s' = ES_Run scr (classify_script scr).
Proof.
  intros scr cls s' H.
  inversion H; subst. reflexivity.
Qed.

Theorem script_invalid_state_rejected :
  forall s,
    exec_step ES_Reject s ->
    s = ES_Reject.
Proof.
  intros s H. inversion H; subst. reflexivity.
Qed.

(** RigorKernel.Kernel.ScriptValidator — executable script check.

    Scope: consensus kernel only.

    [check_script] compares [classify_script] against an expected
    [ScriptType]. Classification rules are unchanged.
    No new consensus rule.

    NOT CLAIMED:
      C++ classifier equivalence
      Script interpreter / signature checks
*)
From Coq Require Import Bool.Bool.
From Coq Require Import Lists.List.
From RigorKernel Require Import Script.ScriptTypes.
From RigorKernel Require Import Script.ScriptClassifier.
From RigorKernel.proofs.Script_Correctness Require Import ScriptCorrectness.

Record ScriptState := {
  ss_script : Script;
  ss_expected : ScriptType
}.

Lemma script_type_eqb_ok :
  forall a b : ScriptType,
    script_type_eqb a b = true <-> a = b.
Proof.
  intros a b; destruct a, b; simpl;
    split; intros H; discriminate || reflexivity || inversion H.
Qed.

Definition check_script (s : ScriptState) : bool :=
  script_type_eqb (classify_script (ss_script s)) (ss_expected s).

(** Statement: a passing check means the classifier returned the
    expected type, and [script_correctness] holds for the script.
    Assumptions: none.
    Consensus kernel only: yes. *)
Theorem check_script_sound :
  forall s,
    check_script s = true ->
    classify_script (ss_script s) = ss_expected s
    /\ In (classify (ss_script s)) reachable_assets
    /\ (shape_hits (ss_script s) <= 1)%nat.
Proof.
  intros s H.
  destruct script_correctness as [Hrange _].
  split; [apply script_type_eqb_ok; exact H |].
  split; [apply Hrange | apply shape_hits_le_one].
Qed.

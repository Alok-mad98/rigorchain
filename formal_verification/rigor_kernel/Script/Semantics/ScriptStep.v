(** One classification step on a kernel script. Not a VM step. *)
From RigorKernel Require Import Script.ScriptTypes.
From RigorKernel Require Import Script.ScriptClassifier.
From RigorKernel Require Import Script.Semantics.ScriptState.

Definition ScriptStep (s1 s2 : ScriptState) : Prop :=
  st_script s2 = st_script s1 /\
  st_class s2 = classify_script (st_script s1).

Definition classify_once (s : Script) : ScriptState :=
  {| st_script := s; st_class := classify_script s |}.

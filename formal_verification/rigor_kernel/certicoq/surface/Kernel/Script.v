(** RigorSurface.Kernel.Script — [check_script]. No theorems. *)
From RigorSurface Require Import Script.ScriptTypes.
From RigorSurface Require Import Script.Classify.

Record ScriptState := {
  ss_script : Script;
  ss_expected : ScriptType
}.

Definition check_script (s : ScriptState) : bool :=
  script_type_eqb (classify_script (ss_script s)) (ss_expected s).

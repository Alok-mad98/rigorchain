(** Kernel-subset script state. Not a Bitcoin Script VM.

    NOT CLAIMED: opcode interpreter, sighash, Ed25519, C++ classifier
    refinement.
*)
From RigorKernel Require Import Script.ScriptTypes.

Record ScriptState := {
  st_script : Script;
  st_class : ScriptType
}.

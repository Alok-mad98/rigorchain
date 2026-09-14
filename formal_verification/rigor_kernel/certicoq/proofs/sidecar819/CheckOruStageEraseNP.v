(** Eval erasure without [Compiler.pipeline]. *)
From Phase10 Require Import CheckOruNoPipeline.

Eval vm_compute in (stage_ok erase_stage).

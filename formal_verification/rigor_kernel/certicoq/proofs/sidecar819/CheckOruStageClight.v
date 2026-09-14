(** Eval in-memory [compile_Clight]. Inspect [Ret]/[Err] only.
    Does not print Clight and does not write [.c]. *)
From Phase10 Require Import CheckOruNoPipeline.

Eval vm_compute in (stage_ok clight_mem_stage).

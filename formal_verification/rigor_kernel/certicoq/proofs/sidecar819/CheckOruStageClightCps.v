(** Eval in-memory [compile_Clight] from the CPS term (no ANF opts). *)
From Phase10 Require Import CheckOruNoPipeline.

Eval vm_compute in (stage_ok clight_from_cps_stage).

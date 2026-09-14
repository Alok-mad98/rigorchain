(** Eval LambdaANF CPS conversion. Inspect [Ret]/[Err] only. *)
From Phase10 Require Import CheckOruNoPipeline.

Eval vm_compute in (stage_ok anf_cps_stage).

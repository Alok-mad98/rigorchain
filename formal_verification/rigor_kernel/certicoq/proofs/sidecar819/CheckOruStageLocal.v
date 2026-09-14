(** Eval LambdaBoxLocal stage. Inspect [Ret]/[Err] only. *)
From Phase10 Require Import CheckOruNoPipeline.

Eval vm_compute in (stage_ok local_stage).

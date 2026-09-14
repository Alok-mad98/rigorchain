(** First pipeline stage only: MetaCoq erasure / LambdaBoxMut. *)
From CertiCoq.Common Require Import Pipeline_utils compM.
From CertiCoq.Compiler Require Import pipeline.
From CheckOruMem Require Import CheckOruMemClight.

Definition erase_only :=
  run_pipeline _ _ default_opts q_check_oru
    (compile_LambdaBoxMut Erasure.default_erasure_config).

Definition erase_ok : bool :=
  match fst erase_only with
  | Ret _ => true
  | Err _ => false
  end.

Eval vm_compute in erase_ok.

(** Compute the in-memory CertiCoq pipeline. Inspect only the
    [Ret]/[Err] constructor — do not print the Clight term. *)
From CertiCoq.Common Require Import compM.
From CheckOruMem Require Import CheckOruMemClight.

Definition compiled_ok : bool :=
  match fst compiled_mem with
  | Ret _ => true
  | Err _ => false
  end.

Definition ir_ok : bool :=
  match fst ir_mem with
  | Ret _ => true
  | Err _ => false
  end.

Eval native_compute in compiled_ok.
Eval native_compute in ir_ok.

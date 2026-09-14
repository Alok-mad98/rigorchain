From Coq Require Import String.
From Phase10 Require Import ClosedCompCertOru.
From compcert Require Import Errors.

Definition cshm_tag : string :=
  match cshm_from_clight with
  | OK _ => "OK"
  | Error (MSG s :: _) => s
  | Error _ => "ERR"
  end.

Timeout 600 Eval vm_compute in cshm_tag.

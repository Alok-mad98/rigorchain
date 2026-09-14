From Coq Require Import String List.
From Phase10 Require Import ClosedCompCertOru.
From compcert Require Import Errors.
Import ListNotations.

Definition simpllocals_tag : string :=
  match simpllocals_res with
  | OK _ => "OK"%string
  | Error (MSG s :: _) => s
  | Error (CTX _ :: MSG s :: _) => s
  | Error _ => "ERR"%string
  end.

Eval vm_compute in simpllocals_tag.

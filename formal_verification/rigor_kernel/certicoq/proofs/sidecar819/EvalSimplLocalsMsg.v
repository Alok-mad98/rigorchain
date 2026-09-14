From Coq Require Import String List.
From Phase10 Require Import ClosedCompCertOru.
From compcert Require Import Errors.
Import ListNotations.

Fixpoint msgs (e: errmsg) : string :=
  match e with
  | nil => EmptyString
  | MSG s :: t => append s (msgs t)
  | CTX _ :: t => append "<id>" (msgs t)
  | POS _ :: t => append "<pos>" (msgs t)
  end.

Definition sl_msgs : string :=
  match simpllocals_res with
  | OK _ => "OK"%string
  | Error e => msgs e
  end.

Eval vm_compute in sl_msgs.

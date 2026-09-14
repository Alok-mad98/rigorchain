From Phase10 Require Import ClosedCompCertOru.
From compcert Require Import Errors.

Definition sl_is_ok : bool :=
  match simpllocals_res with
  | OK _ => true
  | Error _ => false
  end.

Definition sl_msg_head : nat :=
  match simpllocals_res with
  | Error (MSG _ :: CTX _ :: MSG s :: _) => String.length s
  | Error (MSG _ :: CTX _ :: _) => 1
  | Error (MSG _ :: _) => 2
  | Error _ => 3
  | OK _ => 0
  end.

(** Count errcodes so we can see the shape. *)
Fixpoint err_len (e: errmsg) : nat :=
  match e with
  | nil => 0
  | _ :: t => S (err_len t)
  end.

Definition sl_err_len : nat :=
  match simpllocals_res with
  | OK _ => 0
  | Error e => err_len e
  end.

Eval vm_compute in sl_is_ok.
Eval vm_compute in sl_err_len.
Eval vm_compute in sl_msg_head.

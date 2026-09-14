(** Show only the [Err] constructor from CPS→Clight. *)
From CertiCoq.Common Require Import compM.
From MetaCoq.Utils Require Import bytestring.
From Phase10 Require Import CheckOruNoPipeline.

Definition clight_from_cps_fst := fst clight_from_cps_stage.

Eval vm_compute in
  match clight_from_cps_fst with
  | Ret _ => "Ret"%bs
  | Err s => s
  end.

(** Phase 10 step 2: CompCert on the in-memory [Clight.program].

    Uses [transf_clight_program], not [print_Clight] + [.c] +
    [clightgen]. Inspect [OK]/[Error] only. A [ccomp] exit code
    is not this theorem.
*)
From compcert Require Import Compiler Errors.
From Phase10 Require Import CheckOruNoPipeline.

Definition mem_clight_present : bool :=
  match mem_clight_prog with
  | Some _ => true
  | None => false
  end.

Definition transf_clight_ok : bool :=
  match mem_clight_prog with
  | Some p =>
      match transf_clight_program p with
      | OK _ => true
      | Error _ => false
      end
  | None => false
  end.

Definition simpllocals_ok : bool :=
  match mem_clight_prog with
  | Some p =>
      match SimplLocals.transf_program p with
      | OK _ => true
      | Error _ => false
      end
  | None => false
  end.

Eval vm_compute in mem_clight_present.
Eval vm_compute in simpllocals_ok.
Eval vm_compute in transf_clight_ok.

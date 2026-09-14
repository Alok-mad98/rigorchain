(** TASK-018: Cminorgen after Cshmgen on [check_oru] Clight.

    Skips SimplLocals (that pass returns Error "bad temporary" on
    this unnormalized CertiCoq Clight). Does not Require Compiler.
*)
From Phase10 Require Import ClosedCompCertOru.

Theorem cminorgen_check_oru_ok :
  cminor_from_clight_ok = true.
Proof.
  vm_compute.
  reflexivity.
Qed.

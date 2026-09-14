(** TASK-018: Cshmgen on the existing [check_oru] Clight AST.

    Does not [Require Compiler] (no [print_Clight] Parameter).
    Official pipeline order is SimplLocals then Cshmgen; SimplLocals
    returns Error on this CertiCoq Clight ("bad temporary").
    Cshmgen is still a verified CompCert pass on [Clight.program].
*)
From Phase10 Require Import ClosedCompCertOru.

Theorem cshmgen_check_oru_ok :
  cshm_from_clight_ok = true.
Proof.
  vm_compute.
  reflexivity.
Qed.

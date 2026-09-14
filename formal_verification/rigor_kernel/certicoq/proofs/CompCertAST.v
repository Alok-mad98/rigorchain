(** Phase 9 CompCert AST attempt — cone 8.15 documentation.

    Sidecar 8.19 [clightgen -csyntax] produced a well-typed
    [Csyntax.program] from CertiCoq [check_oru] C.
    [transf_c_program] did not [vm_compute] to [OK _] / [Error _]
    within 120s. [ccomp] success is not this theorem.

    This file does not Require CompCert or the generated AST
    (I-6 / I-7: keep the 8.15 cone free of sidecar plugins).
*)

Definition clightgen_csyntax_of_check_oru : bool := true.
Definition transf_c_program_computed_on_check_oru : bool := false.
Definition ccomp_exit_is_not_the_theorem : bool := true.

Theorem clightgen_read_back_check_oru :
  clightgen_csyntax_of_check_oru = true.
Proof. reflexivity. Qed.

Theorem transf_c_program_not_computed :
  transf_c_program_computed_on_check_oru = false.
Proof. reflexivity. Qed.

Theorem ccomp_success_is_not_transf_c_program :
  ccomp_exit_is_not_the_theorem = true.
Proof. reflexivity. Qed.

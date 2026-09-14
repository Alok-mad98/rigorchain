(** Phase 9 LambdaANF attempt — cone 8.15 documentation.

    Sidecar 8.19 loaded [LambdaANF.toplevel_theorems] without
    [Compiler.pipeline] (so without [Axiom print_Clight]).
    Phase 9: no [exp] of [check_oru] without running a
    pipeline. Phase 10 sidecar computed a CPS [exp] without
    [Compiler.pipeline] ([sidecar_cps_exp_of_check_oru] in
    [Phase10Flags.v]); [well_scoped] / [correct] still not
    Closed. Codegen still has no [Theorem].
*)

Definition lambdaanf_lemmas_load_without_pipeline : bool := true.
Definition lambdaanf_exp_of_check_oru : bool := false.
Definition codegen_has_published_theorem : bool := false.

Theorem lambdaanf_lemmas_available_without_print_Clight :
  lambdaanf_lemmas_load_without_pipeline = true.
Proof. reflexivity. Qed.

Theorem no_check_oru_lambdaanf_exp :
  lambdaanf_exp_of_check_oru = false.
Proof. reflexivity. Qed.

Theorem codegen_still_unproved :
  codegen_has_published_theorem = false.
Proof. reflexivity. Qed.

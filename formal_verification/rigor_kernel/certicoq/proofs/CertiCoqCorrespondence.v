(** Phase 7 correspondence — listed passes, not a pipeline instance.

    Sidecar theorems were *read* in CertiCoq 0.9+8.19
    (Compiler/pipeline.v, LambdaANF/toplevel_theorems.v,
    Codegen/toplevel.v). This file does not [Require] those
    modules: [pipeline.v] contains [Axiom print_Clight], and
    loading it would pollute Print Assumptions.

    Surface equality is already Closed in the 8.15 cone
    ([Kernel/SurfaceCorrespondence.v]). There was no fragment
    rewrite, so there is no [rewrite_check_eq] obligation.

    NOT CLAIMED:
      CertiCoq pipeline instantiated on [check_*]
      CompCert AST theorem
      binary semantic equivalence
      C++ / ConnectBlock / rigord
      [refinement_proved]
*)

Definition certicoq_pipeline_instantiated_on_check_utxo : bool := false.
Definition certicoq_pipeline_instantiated_on_check_oru : bool := false.
Definition certicoq_pipeline_instantiated_on_check_res : bool := false.
Definition certicoq_pipeline_instantiated_on_check_script : bool := false.
Definition certicoq_pipeline_instantiated_on_check_pow : bool := false.
Definition certicoq_pipeline_instantiated_on_check_asert : bool := false.
Definition certicoq_pipeline_instantiated_on_validate_kernel : bool := false.

Definition tcb_certicoq_print_Clight_axiom : bool := true.
Definition tcb_certicoq_codegen_unproved : bool := true.
Definition tcb_certicoq_lambdaanf_uninstantiated : bool := true.
Definition no_certicoq_fragment_rewrite : bool := true.

Theorem certicoq_applies_to_check_utxo :
  certicoq_pipeline_instantiated_on_check_utxo = false.
Proof. reflexivity. Qed.

Theorem certicoq_applies_to_check_oru :
  certicoq_pipeline_instantiated_on_check_oru = false.
Proof. reflexivity. Qed.

Theorem certicoq_applies_to_check_res :
  certicoq_pipeline_instantiated_on_check_res = false.
Proof. reflexivity. Qed.

Theorem certicoq_applies_to_check_script :
  certicoq_pipeline_instantiated_on_check_script = false.
Proof. reflexivity. Qed.

Theorem certicoq_applies_to_check_pow :
  certicoq_pipeline_instantiated_on_check_pow = false.
Proof. reflexivity. Qed.

Theorem certicoq_applies_to_check_asert :
  certicoq_pipeline_instantiated_on_check_asert = false.
Proof. reflexivity. Qed.

Theorem certicoq_applies_to_validate_kernel :
  certicoq_pipeline_instantiated_on_validate_kernel = false.
Proof. reflexivity. Qed.

Theorem print_Clight_is_tcb :
  tcb_certicoq_print_Clight_axiom = true.
Proof. reflexivity. Qed.

Theorem codegen_is_tcb :
  tcb_certicoq_codegen_unproved = true.
Proof. reflexivity. Qed.

Theorem lambdaanf_pass_theorems_not_instantiated :
  tcb_certicoq_lambdaanf_uninstantiated = true.
Proof. reflexivity. Qed.

Theorem rewrite_check_eq_vacuous :
  no_certicoq_fragment_rewrite = true.
Proof. reflexivity. Qed.

(** Phase 10 documentation — cone 8.15.

    Sidecar 8.19 measurements (2026-09-13). This file does not
    Require CertiCoq or CompCert (I-6 / I-7).
*)

(* 1. In-memory Clight from Gallina (no print_Clight) *)
Definition erase_stage_ret : bool := true.
Definition local_stage_ret : bool := true.
Definition anf_cps_stage_ret : bool := true.
Definition anf_full_stage_computed : bool := false.
Definition clight_from_cps_ret : bool := false.
Definition mem_clight_from_gallina_pipeline : bool := false.

(* 2. CompCert on that memory Clight *)
Definition transf_clight_on_mem_clight : bool := false.

(* 3. LambdaANF exp without Compiler.pipeline *)
Definition sidecar_cps_exp_of_check_oru : bool := true.
Definition sidecar_exp_well_scoped : bool := false.
Definition correct_instantiated_on_check_oru : bool := false.

(* 4. Codegen — follow upstream; do not write a generator *)
Definition codegen_has_published_theorem : bool := false.
Definition handwritten_codegen : bool := false.

(* 5. Level 5 / §10 *)
Definition both_sides_closed : bool := false.
Definition section_10_sentence_taken : bool := false.

Theorem phase10_erase_local_cps_ret :
  erase_stage_ret = true /\
  local_stage_ret = true /\
  anf_cps_stage_ret = true.
Proof. repeat split; reflexivity. Qed.

Theorem phase10_no_mem_clight :
  mem_clight_from_gallina_pipeline = false.
Proof. reflexivity. Qed.

Theorem phase10_no_transf_on_mem_clight :
  transf_clight_on_mem_clight = false.
Proof. reflexivity. Qed.

Theorem phase10_sidecar_cps_exp :
  sidecar_cps_exp_of_check_oru = true.
Proof. reflexivity. Qed.

Theorem phase10_no_well_scoped_or_correct :
  sidecar_exp_well_scoped = false /\
  correct_instantiated_on_check_oru = false.
Proof. split; reflexivity. Qed.

Theorem phase10_codegen_still_unproved :
  codegen_has_published_theorem = false /\
  handwritten_codegen = false.
Proof. split; reflexivity. Qed.

Theorem phase10_level5_not_taken :
  both_sides_closed = false /\
  section_10_sentence_taken = false.
Proof. split; reflexivity. Qed.

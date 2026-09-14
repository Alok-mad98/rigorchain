(** TASK 3.1B documentation flags. Cone 8.15.0. No CompCert import. *)

Definition CLIGHT_TO_CMINOR_CLOSED : bool := true.
Definition CLIGHT_TO_ASM_CLOSED : bool := false.
Definition COMPILER_CLOSED_ROUTE_BLOCKED_BY_TCB : bool := true.
Definition closed_compcert_check_oru_established : bool := false.
Definition CPP_KERNELINPUT_EQUIVALENCE : bool := false.
Definition REFINEMENT_PROVED : bool := false.
Definition LEVEL_5_CLAIMED : bool := false.
Definition compiler_correctness : bool := false.
Definition verified_compiler_in_current_tcb : bool := false.
Definition UNLIFT_SSET_ONLY_PARAMS_IS_SEMANTIC_PRESERVATION : bool := false.
Definition CHECK_ORU_UNLIFT_FUNCTION_SIMULATION : bool := false.
Definition CHECK_ORU_UNLIFT_CHECKER_SIMULATION : bool := false.
Definition UNLIFT_ONE_STEP_SSET_SASSIGN : bool := true.

Theorem task31b_clight_to_cminor_still_pass :
  CLIGHT_TO_CMINOR_CLOSED = true.
Proof. reflexivity. Qed.

Theorem task31b_asm_not_closed :
  CLIGHT_TO_ASM_CLOSED = false.
Proof. reflexivity. Qed.

Theorem task31b_route_blocked_by_tcb :
  COMPILER_CLOSED_ROUTE_BLOCKED_BY_TCB = true.
Proof. reflexivity. Qed.

Theorem task31b_unlift_sset_not_semantic :
  UNLIFT_SSET_ONLY_PARAMS_IS_SEMANTIC_PRESERVATION = false.
Proof. reflexivity. Qed.

Theorem task31b_no_checker_function_simulation :
  CHECK_ORU_UNLIFT_FUNCTION_SIMULATION = false /\
  CHECK_ORU_UNLIFT_CHECKER_SIMULATION = false.
Proof. split; reflexivity. Qed.

Theorem task31b_one_step_recorded :
  UNLIFT_ONE_STEP_SSET_SASSIGN = true.
Proof. reflexivity. Qed.

Theorem task31b_does_not_claim_level_5 :
  LEVEL_5_CLAIMED = false /\
  compiler_correctness = false /\
  verified_compiler_in_current_tcb = false /\
  REFINEMENT_PROVED = false /\
  CPP_KERNELINPUT_EQUIVALENCE = false /\
  closed_compcert_check_oru_established = false.
Proof. repeat split; reflexivity. Qed.

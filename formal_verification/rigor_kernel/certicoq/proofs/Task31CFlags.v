(** TASK 3.1C documentation flags. Cone 8.15.0. No CompCert import. *)

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

(** Route decision. Exactly one of the three route flags is true. *)
Definition FORWARD_SIMULATION_ROUTE_FEASIBLE : bool := false.
Definition FORWARD_SIMULATION_ROUTE_NOT_FEASIBLE_WITHIN_SCOPE : bool := true.
Definition FORWARD_SIMULATION_ROUTE_BLOCKED_BY_OFFICIAL_SEMANTICS : bool := false.
Definition OFFICIAL_SEMANTICS_BLOCKER : bool := false.
Definition RETURN_FREE_LIST_REQUIRES_UPSTREAM_PROOF : bool := true.

Theorem task31c_clight_to_cminor_still_pass :
  CLIGHT_TO_CMINOR_CLOSED = true.
Proof. reflexivity. Qed.

Theorem task31c_asm_not_closed :
  CLIGHT_TO_ASM_CLOSED = false.
Proof. reflexivity. Qed.

Theorem task31c_route_blocked_by_tcb :
  COMPILER_CLOSED_ROUTE_BLOCKED_BY_TCB = true.
Proof. reflexivity. Qed.

Theorem task31c_unlift_sset_not_semantic :
  UNLIFT_SSET_ONLY_PARAMS_IS_SEMANTIC_PRESERVATION = false.
Proof. reflexivity. Qed.

Theorem task31c_no_checker_simulation :
  CHECK_ORU_UNLIFT_FUNCTION_SIMULATION = false /\
  CHECK_ORU_UNLIFT_CHECKER_SIMULATION = false.
Proof. split; reflexivity. Qed.

Theorem task31c_forward_sim_route_decision :
  FORWARD_SIMULATION_ROUTE_FEASIBLE = false /\
  FORWARD_SIMULATION_ROUTE_NOT_FEASIBLE_WITHIN_SCOPE = true /\
  FORWARD_SIMULATION_ROUTE_BLOCKED_BY_OFFICIAL_SEMANTICS = false /\
  OFFICIAL_SEMANTICS_BLOCKER = false /\
  RETURN_FREE_LIST_REQUIRES_UPSTREAM_PROOF = true.
Proof. repeat split; reflexivity. Qed.

Theorem task31c_does_not_claim_level_5 :
  LEVEL_5_CLAIMED = false /\
  compiler_correctness = false /\
  verified_compiler_in_current_tcb = false /\
  REFINEMENT_PROVED = false /\
  CPP_KERNELINPUT_EQUIVALENCE = false /\
  closed_compcert_check_oru_established = false.
Proof. repeat split; reflexivity. Qed.

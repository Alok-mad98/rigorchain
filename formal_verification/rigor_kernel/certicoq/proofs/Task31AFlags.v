(** TASK 3.1A documentation flags. Cone 8.15.0. No CompCert import. *)

Definition CLIGHT_TO_CMINOR_CLOSED : bool := true.
Definition CLIGHT_TO_ASM_CLOSED : bool := false.
Definition COMPILER_CLOSED_ROUTE_BLOCKED_BY_TCB : bool := true.
Definition COMPUTABLE_WITH_NATIVE_COMPUTE : bool := false.
Definition closed_compcert_check_oru_established : bool := false.
Definition CPP_KERNELINPUT_EQUIVALENCE : bool := false.
Definition REFINEMENT_PROVED : bool := false.
Definition LEVEL_5_CLAIMED : bool := false.
Definition compiler_correctness : bool := false.
Definition verified_compiler_in_current_tcb : bool := false.

Theorem task31a_clight_to_cminor_still_pass :
  CLIGHT_TO_CMINOR_CLOSED = true.
Proof. reflexivity. Qed.

Theorem task31a_asm_not_closed :
  CLIGHT_TO_ASM_CLOSED = false.
Proof. reflexivity. Qed.

Theorem task31a_route_blocked_by_tcb :
  COMPILER_CLOSED_ROUTE_BLOCKED_BY_TCB = true.
Proof. reflexivity. Qed.

Theorem task31a_native_compute_not_a_route :
  COMPUTABLE_WITH_NATIVE_COMPUTE = false.
Proof. reflexivity. Qed.

Theorem task31a_no_closed_compcert_check_oru :
  closed_compcert_check_oru_established = false.
Proof. reflexivity. Qed.

Theorem task31a_does_not_claim_level_5 :
  LEVEL_5_CLAIMED = false /\
  compiler_correctness = false /\
  verified_compiler_in_current_tcb = false /\
  REFINEMENT_PROVED = false /\
  CPP_KERNELINPUT_EQUIVALENCE = false.
Proof. repeat split; reflexivity. Qed.

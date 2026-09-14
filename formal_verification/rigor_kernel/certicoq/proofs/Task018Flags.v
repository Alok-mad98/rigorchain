(** TASK-018 documentation flags. Cone 8.15.0. No CompCert import.

    Records the sidecar Closed Clight → Cminor probe. Does not
    instantiate [transf_clight_program] to Asm. Does not take
    Level 5.
*)

Definition CLIGHT_TO_CMINOR_CLOSED : bool := true.
Definition CLIGHT_TO_ASM_CLOSED : bool := false.
Definition CLIGHT_TO_SELECTION_CLOSED : bool := false.
Definition closed_compcert_check_oru_established : bool := false.
Definition CPP_KERNELINPUT_EQUIVALENCE : bool := false.
Definition REFINEMENT_PROVED : bool := false.
Definition LEVEL_5_CLAIMED : bool := false.
Definition compiler_correctness : bool := false.
Definition binary_semantic_equivalence : bool := false.
Definition verified_compiler_in_current_tcb : bool := false.

Theorem clight_to_cminor_closed_six_checkers :
  CLIGHT_TO_CMINOR_CLOSED = true.
Proof. reflexivity. Qed.

Theorem clight_to_asm_not_closed :
  CLIGHT_TO_ASM_CLOSED = false.
Proof. reflexivity. Qed.

Theorem no_closed_compcert_check_oru :
  closed_compcert_check_oru_established = false.
Proof. reflexivity. Qed.

Theorem task018_does_not_claim_level_5 :
  LEVEL_5_CLAIMED = false /\
  compiler_correctness = false /\
  binary_semantic_equivalence = false /\
  verified_compiler_in_current_tcb = false /\
  REFINEMENT_PROVED = false /\
  CPP_KERNELINPUT_EQUIVALENCE = false.
Proof. repeat split; reflexivity. Qed.

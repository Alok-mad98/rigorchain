(** TASK 3.3A4 documentation flags. Cone 8.15.0.
    Five integer/hash residuals remain PARTIAL. Coq LE specs and
    signed/unsigned MSB-clear lemmas are Closed. C++ ser_readdata
    inhabit remains. *)

Definition FORMAL_MODEL_CLOSED : bool := true.
Definition MODEL_PROPERTIES_PROVED : bool := true.
Definition CLIGHT_TO_CMINOR_CLOSED : bool := true.
Definition CLIGHT_TO_ASM_CLOSED : bool := false.
Definition COMPILER_CLOSED_ROUTE_BLOCKED_BY_TCB : bool := true.
Definition FORWARD_SIMULATION_ROUTE_NOT_FEASIBLE_WITHIN_SCOPE : bool := true.

Definition INTERFACE_PHASE_COMPLETE : bool := true.
Definition PROOF_PHASE_STARTED : bool := true.

Definition CPP_BLOCK_DECODE_REFINEMENT_PARTIAL : bool := true.
Definition CPP_BLOCK_DECODE_LE_SPEC : bool := true.
Definition CPP_BLOCK_DECODE_MSB_CLEAR : bool := true.
Definition CPP_BLOCK_DECODE_REFINEMENT : bool := false.

Definition OBLIGATION_2A_UINT32_LE_PROVED : bool := false.
Definition OBLIGATION_2B_INT32_VERSION_PROVED : bool := false.
Definition OBLIGATION_2C_UINT256_PROVED : bool := false.
Definition OBLIGATION_4A_INT64_LE_PROVED : bool := false.
Definition OBLIGATION_4B_CAMOUNT_MONEYRANGE_PROVED : bool := false.

Definition CPP_KERNELINPUT_EQUIVALENCE : bool := false.
Definition REFINEMENT_PROVED : bool := false.
Definition LEVEL_5_CLAIMED : bool := false.

Theorem task33a4_model_still_closed :
  FORMAL_MODEL_CLOSED = true /\ MODEL_PROPERTIES_PROVED = true.
Proof. split; reflexivity. Qed.

Theorem task33a4_primitives_still_partial :
  INTERFACE_PHASE_COMPLETE = true /\
  PROOF_PHASE_STARTED = true /\
  CPP_BLOCK_DECODE_REFINEMENT_PARTIAL = true /\
  CPP_BLOCK_DECODE_LE_SPEC = true /\
  CPP_BLOCK_DECODE_MSB_CLEAR = true /\
  OBLIGATION_2A_UINT32_LE_PROVED = false /\
  OBLIGATION_2B_INT32_VERSION_PROVED = false /\
  OBLIGATION_2C_UINT256_PROVED = false /\
  OBLIGATION_4A_INT64_LE_PROVED = false /\
  OBLIGATION_4B_CAMOUNT_MONEYRANGE_PROVED = false /\
  CPP_BLOCK_DECODE_REFINEMENT = false /\
  CPP_KERNELINPUT_EQUIVALENCE = false /\
  REFINEMENT_PROVED = false /\
  LEVEL_5_CLAIMED = false.
Proof. repeat split; reflexivity. Qed.

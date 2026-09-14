(** TASK 3.3A3 documentation flags. Cone 8.15.0.
    Four 3.3A2 residuals remain PARTIAL. Coq CAmount / header
    representation is Closed. C++ Unserialize inhabit remains. *)

Definition FORMAL_MODEL_CLOSED : bool := true.
Definition MODEL_PROPERTIES_PROVED : bool := true.
Definition CLIGHT_TO_CMINOR_CLOSED : bool := true.
Definition CLIGHT_TO_ASM_CLOSED : bool := false.
Definition COMPILER_CLOSED_ROUTE_BLOCKED_BY_TCB : bool := true.
Definition FORWARD_SIMULATION_ROUTE_NOT_FEASIBLE_WITHIN_SCOPE : bool := true.

Definition INTERFACE_PHASE_COMPLETE : bool := true.
Definition PROOF_PHASE_STARTED : bool := true.
Definition KERNELINPUT_TEN_FIELDS_COVERED : bool := true.

Definition STRICT_VALID_BLOCK_DOMAIN_DEFINED : bool := true.
Definition CPP_BLOCK_DECODE_REFINEMENT_STATED : bool := true.
Definition CPP_BLOCK_DECODE_REFINEMENT_PARTIAL : bool := true.
Definition CPP_BLOCK_DECODE_COQ_SIDE : bool := true.
Definition CPP_BLOCK_DECODE_CPP_OPERATOR_SHIFT : bool := false.
Definition CPP_BLOCK_DECODE_TIGHT_STREAM_WITNESS : bool := true.
Definition CPP_BLOCK_DECODE_CAMOUNT_REPR : bool := true.
Definition CPP_BLOCK_DECODE_HEADER_REPR : bool := true.
Definition CPP_BLOCK_DECODE_REFINEMENT : bool := false.

Definition OBLIGATION_4_CAMOUNT_PROVED : bool := false.
Definition OBLIGATION_2_HEADER_PROVED : bool := false.
Definition OBLIGATION_3_TX_PROVED : bool := false.
Definition OBLIGATION_1_CBLOCK_PROVED : bool := false.

Definition CPP_UTXO_REPRESENTATION_REFINEMENT : bool := false.
Definition REAL_CPP_INHABIT : bool := false.
Definition CPP_KERNELINPUT_EQUIVALENCE : bool := false.
Definition REFINEMENT_PROVED : bool := false.
Definition LEVEL_5_CLAIMED : bool := false.

Theorem task33a3_model_still_closed :
  FORMAL_MODEL_CLOSED = true /\ MODEL_PROPERTIES_PROVED = true.
Proof. split; reflexivity. Qed.

Theorem task33a3_decode_still_partial :
  INTERFACE_PHASE_COMPLETE = true /\
  PROOF_PHASE_STARTED = true /\
  CPP_BLOCK_DECODE_REFINEMENT_PARTIAL = true /\
  CPP_BLOCK_DECODE_CAMOUNT_REPR = true /\
  CPP_BLOCK_DECODE_HEADER_REPR = true /\
  OBLIGATION_4_CAMOUNT_PROVED = false /\
  OBLIGATION_2_HEADER_PROVED = false /\
  OBLIGATION_3_TX_PROVED = false /\
  OBLIGATION_1_CBLOCK_PROVED = false /\
  CPP_BLOCK_DECODE_REFINEMENT = false /\
  CPP_KERNELINPUT_EQUIVALENCE = false /\
  REFINEMENT_PROVED = false /\
  LEVEL_5_CLAIMED = false.
Proof. repeat split; reflexivity. Qed.

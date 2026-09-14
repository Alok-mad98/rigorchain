(** TASK 3.3A2 documentation flags. Cone 8.15.0.
    remaining_operator_shift_matches_coq_decode is still not Qed.
    Tight stream witness + split lemmas are Closed. C++ Unserialize
    inhabit remains. Decode refinement stays PARTIAL. *)

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
Definition CPP_BLOCK_DECODE_REFINEMENT : bool := false.

Definition CPP_UTXO_REPRESENTATION_REFINEMENT_STATED : bool := true.
Definition CPP_UTXO_REPRESENTATION_REFINEMENT : bool := false.
Definition CPP_NAMED_ACCUMULATORS_REFINEMENT_STATED : bool := true.
Definition CPP_NAMED_ACCUMULATORS_REFINEMENT : bool := false.
Definition CPP_BOOL_FLAGS_ABSTRACTION_STATED : bool := true.
Definition CPP_BOOL_FLAGS_ABSTRACTION : bool := false.
Definition REAL_CPP_INHABIT : bool := false.

Definition CPP_KERNELINPUT_EQUIVALENCE : bool := false.
Definition REFINEMENT_PROVED : bool := false.
Definition LEVEL_5_CLAIMED : bool := false.

Theorem task33a2_model_still_closed :
  FORMAL_MODEL_CLOSED = true /\ MODEL_PROPERTIES_PROVED = true.
Proof. split; reflexivity. Qed.

Theorem task33a2_decode_still_partial :
  INTERFACE_PHASE_COMPLETE = true /\
  PROOF_PHASE_STARTED = true /\
  CPP_BLOCK_DECODE_REFINEMENT_PARTIAL = true /\
  CPP_BLOCK_DECODE_COQ_SIDE = true /\
  CPP_BLOCK_DECODE_TIGHT_STREAM_WITNESS = true /\
  CPP_BLOCK_DECODE_CPP_OPERATOR_SHIFT = false /\
  CPP_BLOCK_DECODE_REFINEMENT = false /\
  CPP_KERNELINPUT_EQUIVALENCE = false /\
  REFINEMENT_PROVED = false /\
  LEVEL_5_CLAIMED = false.
Proof. repeat split; reflexivity. Qed.

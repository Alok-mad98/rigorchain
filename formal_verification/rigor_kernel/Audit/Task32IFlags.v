(** TASK 3.2I documentation flags. Cone 8.15.0.
    CPP_NAMED_ACCUMULATORS_REFINEMENT is STATED, not proved.
    Does not import CompCert. *)

Definition FORMAL_MODEL_CLOSED : bool := true.
Definition MODEL_PROPERTIES_PROVED : bool := true.
Definition CLIGHT_TO_CMINOR_CLOSED : bool := true.
Definition CLIGHT_TO_ASM_CLOSED : bool := false.
Definition COMPILER_CLOSED_ROUTE_BLOCKED_BY_TCB : bool := true.
Definition FORWARD_SIMULATION_ROUTE_NOT_FEASIBLE_WITHIN_SCOPE : bool := true.

Definition COQ_SIDE_PROJECTION : bool := true.
Definition CPP_SIDE_OBSERVABLES_MAPPING_DEFINED : bool := true.
Definition CPP_INCOMING_OUTGOING_EXTRACTION_DEFINED : bool := true.
Definition CPP_TX_ORU_FLOW_INTERFACE_DEFINED : bool := true.
Definition REAL_CPP_INHABIT_STATEMENT_DEFINED : bool := true.
Definition REAL_CPP_INHABIT : bool := false.
Definition STRICT_VALID_BLOCK_DOMAIN_DEFINED : bool := true.
Definition CPP_BLOCK_DECODE_REFINEMENT_STATED : bool := true.
Definition CPP_UTXO_REPRESENTATION_REFINEMENT_STATED : bool := true.
Definition CPP_NAMED_ACCUMULATORS_REFINEMENT_STATED : bool := true.

Definition CPP_KERNELINPUT_EQUIVALENCE : bool := false.
Definition REFINEMENT_PROVED : bool := false.
Definition LEVEL_5_CLAIMED : bool := false.
Definition CPP_BLOCK_DECODE_REFINEMENT : bool := false.
Definition CPP_UTXO_REPRESENTATION_REFINEMENT : bool := false.
Definition CPP_NAMED_ACCUMULATORS_REFINEMENT : bool := false.
Definition CPP_VALIDATION_RESULT_REFINEMENT : bool := false.
Definition CPP_KERNELINPUT_CONSTRUCTION_REFINEMENT : bool := false.
Definition CPP_FEES_ACCUMULATOR_REFINEMENT : bool := false.
Definition CPP_BOOL_FLAGS_ABSTRACTION : bool := false.

Theorem task32i_model_still_closed :
  FORMAL_MODEL_CLOSED = true /\ MODEL_PROPERTIES_PROVED = true.
Proof. split; reflexivity. Qed.

Theorem task32i_compiler_boundary_unchanged :
  CLIGHT_TO_CMINOR_CLOSED = true /\
  CLIGHT_TO_ASM_CLOSED = false /\
  COMPILER_CLOSED_ROUTE_BLOCKED_BY_TCB = true /\
  FORWARD_SIMULATION_ROUTE_NOT_FEASIBLE_WITHIN_SCOPE = true.
Proof. repeat split; reflexivity. Qed.

Theorem task32i_named_acc_stated_not_proved :
  CPP_NAMED_ACCUMULATORS_REFINEMENT_STATED = true /\
  CPP_NAMED_ACCUMULATORS_REFINEMENT = false /\
  CPP_UTXO_REPRESENTATION_REFINEMENT_STATED = true /\
  CPP_UTXO_REPRESENTATION_REFINEMENT = false /\
  CPP_BLOCK_DECODE_REFINEMENT_STATED = true /\
  CPP_BLOCK_DECODE_REFINEMENT = false /\
  REAL_CPP_INHABIT = false /\
  CPP_KERNELINPUT_EQUIVALENCE = false /\
  REFINEMENT_PROVED = false /\
  LEVEL_5_CLAIMED = false.
Proof. repeat split; reflexivity. Qed.

Theorem task32i_other_cpp_bridges_still_missing :
  CPP_VALIDATION_RESULT_REFINEMENT = false /\
  CPP_KERNELINPUT_CONSTRUCTION_REFINEMENT = false /\
  CPP_FEES_ACCUMULATOR_REFINEMENT = false /\
  CPP_BOOL_FLAGS_ABSTRACTION = false.
Proof. repeat split; reflexivity. Qed.

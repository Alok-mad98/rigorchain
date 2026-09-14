(** TASK 3.2D documentation flags. Cone 8.15.0.
    Incoming/outgoing extraction is DEFINED (audit), not C++-proved.
    Does not import CompCert. Does not claim refinement. *)

Definition FORMAL_MODEL_CLOSED : bool := true.
Definition MODEL_PROPERTIES_PROVED : bool := true.
Definition CLIGHT_TO_CMINOR_CLOSED : bool := true.
Definition CLIGHT_TO_ASM_CLOSED : bool := false.
Definition COMPILER_CLOSED_ROUTE_BLOCKED_BY_TCB : bool := true.
Definition FORWARD_SIMULATION_ROUTE_NOT_FEASIBLE_WITHIN_SCOPE : bool := true.

Definition COQ_SIDE_PROJECTION : bool := true.
Definition CPP_SIDE_OBSERVABLES_MAPPING_DEFINED : bool := true.
Definition CPP_INCOMING_OUTGOING_EXTRACTION_DEFINED : bool := true.
Definition CPP_INCOMING_OUTGOING_EXTRACTION_PROVED : bool := false.

Definition CPP_KERNELINPUT_EQUIVALENCE : bool := false.
Definition REFINEMENT_PROVED : bool := false.
Definition LEVEL_5_CLAIMED : bool := false.
Definition CPP_BLOCK_DECODE_REFINEMENT : bool := false.
Definition CPP_UTXO_REPRESENTATION_REFINEMENT : bool := false.
Definition CPP_VALIDATION_RESULT_REFINEMENT : bool := false.
Definition CPP_KERNELINPUT_CONSTRUCTION_REFINEMENT : bool := false.
Definition CPP_FEES_ACCUMULATOR_REFINEMENT : bool := false.
Definition CPP_BOOL_FLAGS_ABSTRACTION : bool := false.

Theorem task32d_model_still_closed :
  FORMAL_MODEL_CLOSED = true /\ MODEL_PROPERTIES_PROVED = true.
Proof. split; reflexivity. Qed.

Theorem task32d_compiler_boundary_unchanged :
  CLIGHT_TO_CMINOR_CLOSED = true /\
  CLIGHT_TO_ASM_CLOSED = false /\
  COMPILER_CLOSED_ROUTE_BLOCKED_BY_TCB = true /\
  FORWARD_SIMULATION_ROUTE_NOT_FEASIBLE_WITHIN_SCOPE = true.
Proof. repeat split; reflexivity. Qed.

Theorem task32d_extraction_defined_not_refinement :
  COQ_SIDE_PROJECTION = true /\
  CPP_SIDE_OBSERVABLES_MAPPING_DEFINED = true /\
  CPP_INCOMING_OUTGOING_EXTRACTION_DEFINED = true /\
  CPP_INCOMING_OUTGOING_EXTRACTION_PROVED = false /\
  CPP_KERNELINPUT_EQUIVALENCE = false /\
  REFINEMENT_PROVED = false /\
  LEVEL_5_CLAIMED = false.
Proof. repeat split; reflexivity. Qed.

Theorem task32d_other_cpp_bridges_still_missing :
  CPP_BLOCK_DECODE_REFINEMENT = false /\
  CPP_UTXO_REPRESENTATION_REFINEMENT = false /\
  CPP_VALIDATION_RESULT_REFINEMENT = false /\
  CPP_KERNELINPUT_CONSTRUCTION_REFINEMENT = false /\
  CPP_FEES_ACCUMULATOR_REFINEMENT = false /\
  CPP_BOOL_FLAGS_ABSTRACTION = false.
Proof. repeat split; reflexivity. Qed.

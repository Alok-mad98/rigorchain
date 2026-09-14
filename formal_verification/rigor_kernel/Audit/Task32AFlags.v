(** TASK 3.2A documentation flags. Cone 8.15.0.
    Does not import CompCert. Does not claim refinement. *)

Definition FORMAL_MODEL_CLOSED : bool := true.
Definition MODEL_PROPERTIES_PROVED : bool := true.
Definition CLIGHT_TO_CMINOR_CLOSED : bool := true.
Definition CLIGHT_TO_ASM_CLOSED : bool := false.
Definition COMPILER_CLOSED_ROUTE_BLOCKED_BY_TCB : bool := true.
Definition CPP_KERNELINPUT_EQUIVALENCE : bool := false.
Definition REFINEMENT_PROVED : bool := false.
Definition LEVEL_5_CLAIMED : bool := false.
Definition FORWARD_SIMULATION_ROUTE_NOT_FEASIBLE_WITHIN_SCOPE : bool := true.
Definition CPP_BLOCK_DECODE_REFINEMENT : bool := false.
Definition CPP_UTXO_REPRESENTATION_REFINEMENT : bool := false.
Definition CPP_VALIDATION_RESULT_REFINEMENT : bool := false.
Definition CPP_KERNELINPUT_CONSTRUCTION_REFINEMENT : bool := false.

Theorem task32a_model_still_closed :
  FORMAL_MODEL_CLOSED = true /\ MODEL_PROPERTIES_PROVED = true.
Proof. split; reflexivity. Qed.

Theorem task32a_compiler_boundary_unchanged :
  CLIGHT_TO_CMINOR_CLOSED = true /\
  CLIGHT_TO_ASM_CLOSED = false /\
  COMPILER_CLOSED_ROUTE_BLOCKED_BY_TCB = true /\
  FORWARD_SIMULATION_ROUTE_NOT_FEASIBLE_WITHIN_SCOPE = true.
Proof. repeat split; reflexivity. Qed.

Theorem task32a_all_cpp_bridges_missing :
  CPP_BLOCK_DECODE_REFINEMENT = false /\
  CPP_UTXO_REPRESENTATION_REFINEMENT = false /\
  CPP_VALIDATION_RESULT_REFINEMENT = false /\
  CPP_KERNELINPUT_CONSTRUCTION_REFINEMENT = false.
Proof. repeat split; reflexivity. Qed.

Theorem task32a_does_not_claim_refinement :
  CPP_KERNELINPUT_EQUIVALENCE = false /\
  REFINEMENT_PROVED = false /\
  LEVEL_5_CLAIMED = false.
Proof. repeat split; reflexivity. Qed.

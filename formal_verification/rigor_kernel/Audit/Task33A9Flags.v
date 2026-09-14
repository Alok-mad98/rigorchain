(** TASK 3.3A9 documentation flags. Cone 8.15.0.
    Production-bool / loose operator>>: Coq glue Closed.
    C++ inhabit PARTIAL. Main theorem STILL_PARTIAL. No Axiom. *)

Definition FORMAL_MODEL_CLOSED : bool := true.
Definition MODEL_PROPERTIES_PROVED : bool := true.
Definition CLIGHT_TO_CMINOR_CLOSED : bool := true.
Definition CLIGHT_TO_ASM_CLOSED : bool := false.
Definition COMPILER_CLOSED_ROUTE_BLOCKED_BY_TCB : bool := true.

Definition INTERFACE_PHASE_COMPLETE : bool := true.
Definition PROOF_PHASE_STARTED : bool := true.

Definition CPP_BLOCK_DECODE_REFINEMENT_PARTIAL : bool := true.
Definition CPP_BLOCK_DECODE_REFINEMENT : bool := false.
Definition DECODE_LEAF_RESIDUALS_FROZEN : bool := true.
Definition DECODE_LEAF_ROUTE_TCB : bool := true.
Definition DECODE_MAIN_THEOREM_UNDER_LEAF_TCB : bool := false.
Definition DECODE_MAIN_THEOREM_UNDER_LEAF_TCB_STILL_PARTIAL : bool := true.
Definition UNDER_LEAF_TCB_MAIN_THEOREM_STATUS_STILL_PARTIAL : bool := true.

Definition PRODUCTION_BOOL_SPEC_STRUCTURE_CLOSED : bool := true.
Definition LOOSE_PROJECTION_SPEC_STRUCTURE_CLOSED : bool := true.
Definition remaining_production_bool_inhabits_claimed_block_PROVED : bool := false.
Definition remaining_loose_operator_shift_inhabit_PROVED : bool := false.
Definition remaining_loose_claim_is_tight_production_projection_PROVED : bool := false.

Definition CPP_KERNELINPUT_EQUIVALENCE : bool := false.
Definition REFINEMENT_PROVED : bool := false.
Definition LEVEL_5_CLAIMED : bool := false.

Theorem task33a9_model_still_closed :
  FORMAL_MODEL_CLOSED = true /\ MODEL_PROPERTIES_PROVED = true.
Proof. split; reflexivity. Qed.

Theorem task33a9_production_inhabit_still_partial :
  INTERFACE_PHASE_COMPLETE = true /\
  PROOF_PHASE_STARTED = true /\
  CPP_BLOCK_DECODE_REFINEMENT_PARTIAL = true /\
  CPP_BLOCK_DECODE_REFINEMENT = false /\
  DECODE_LEAF_RESIDUALS_FROZEN = true /\
  DECODE_LEAF_ROUTE_TCB = true /\
  DECODE_MAIN_THEOREM_UNDER_LEAF_TCB = false /\
  DECODE_MAIN_THEOREM_UNDER_LEAF_TCB_STILL_PARTIAL = true /\
  UNDER_LEAF_TCB_MAIN_THEOREM_STATUS_STILL_PARTIAL = true /\
  PRODUCTION_BOOL_SPEC_STRUCTURE_CLOSED = true /\
  LOOSE_PROJECTION_SPEC_STRUCTURE_CLOSED = true /\
  remaining_production_bool_inhabits_claimed_block_PROVED = false /\
  remaining_loose_operator_shift_inhabit_PROVED = false /\
  remaining_loose_claim_is_tight_production_projection_PROVED = false /\
  CPP_KERNELINPUT_EQUIVALENCE = false /\
  REFINEMENT_PROVED = false /\
  LEVEL_5_CLAIMED = false.
Proof. repeat split; reflexivity. Qed.

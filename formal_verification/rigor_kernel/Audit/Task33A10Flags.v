(** TASK 3.3A10 documentation flags. Cone 8.15.0.
    Decode boundary FROZEN at PARTIAL. Next active = 3.3B.
    No UTXO proof started. No Axiom. *)

Definition FORMAL_MODEL_CLOSED : bool := true.
Definition MODEL_PROPERTIES_PROVED : bool := true.
Definition CLIGHT_TO_CMINOR_CLOSED : bool := true.
Definition CLIGHT_TO_ASM_CLOSED : bool := false.
Definition COMPILER_CLOSED_ROUTE_BLOCKED_BY_TCB : bool := true.

Definition INTERFACE_PHASE_COMPLETE : bool := true.
Definition PROOF_PHASE_STARTED : bool := true.

Definition DECODE_LEAF_RESIDUALS_FROZEN : bool := true.
Definition DECODE_LEAF_ROUTE_TCB : bool := true.
Definition DECODE_MAIN_THEOREM_UNDER_LEAF_TCB : bool := false.
Definition DECODE_MAIN_THEOREM_UNDER_LEAF_TCB_STILL_PARTIAL : bool := true.

Definition CPP_BLOCK_DECODE_REFINEMENT_PARTIAL : bool := true.
Definition CPP_BLOCK_DECODE_REFINEMENT_FROZEN : bool := true.
Definition CPP_BLOCK_DECODE_REFINEMENT : bool := false.
Definition DECODE_BOUNDARY_FROZEN : bool := true.

Definition NEXT_ACTIVE_3_3B_UTXO_REPRESENTATION : bool := true.
Definition UTXO_PROOF_STARTED : bool := false.
Definition CPP_UTXO_REPRESENTATION_REFINEMENT : bool := false.

Definition CPP_KERNELINPUT_EQUIVALENCE : bool := false.
Definition REFINEMENT_PROVED : bool := false.
Definition LEVEL_5_CLAIMED : bool := false.

Theorem task33a10_model_still_closed :
  FORMAL_MODEL_CLOSED = true /\ MODEL_PROPERTIES_PROVED = true.
Proof. split; reflexivity. Qed.

Theorem task33a10_decode_frozen_next_is_utxo :
  INTERFACE_PHASE_COMPLETE = true /\
  PROOF_PHASE_STARTED = true /\
  DECODE_BOUNDARY_FROZEN = true /\
  CPP_BLOCK_DECODE_REFINEMENT_PARTIAL = true /\
  CPP_BLOCK_DECODE_REFINEMENT_FROZEN = true /\
  CPP_BLOCK_DECODE_REFINEMENT = false /\
  DECODE_LEAF_RESIDUALS_FROZEN = true /\
  DECODE_LEAF_ROUTE_TCB = true /\
  DECODE_MAIN_THEOREM_UNDER_LEAF_TCB = false /\
  DECODE_MAIN_THEOREM_UNDER_LEAF_TCB_STILL_PARTIAL = true /\
  NEXT_ACTIVE_3_3B_UTXO_REPRESENTATION = true /\
  UTXO_PROOF_STARTED = false /\
  CPP_UTXO_REPRESENTATION_REFINEMENT = false /\
  CPP_KERNELINPUT_EQUIVALENCE = false /\
  REFINEMENT_PROVED = false /\
  LEVEL_5_CLAIMED = false.
Proof. repeat split; reflexivity. Qed.

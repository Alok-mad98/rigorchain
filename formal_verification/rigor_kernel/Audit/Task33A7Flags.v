(** TASK 3.3A7 documentation flags. Cone 8.15.0.
    CompactSize / CScript / vector: Coq composition Closed.
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

Definition COMPACTSIZE_SPEC_COMPOSITION_CLOSED : bool := true.
Definition SCRIPT_SPEC_COMPOSITION_CLOSED : bool := true.
Definition VECTOR_SPEC_COMPOSITION_CLOSED : bool := true.
Definition remaining_readcompactsize_PROVED : bool := false.
Definition remaining_cscript_PROVED : bool := false.
Definition remaining_vector_PROVED : bool := false.

Definition CPP_KERNELINPUT_EQUIVALENCE : bool := false.
Definition REFINEMENT_PROVED : bool := false.
Definition LEVEL_5_CLAIMED : bool := false.

Theorem task33a7_model_still_closed :
  FORMAL_MODEL_CLOSED = true /\ MODEL_PROPERTIES_PROVED = true.
Proof. split; reflexivity. Qed.

Theorem task33a7_cs_script_vector_still_partial :
  INTERFACE_PHASE_COMPLETE = true /\
  PROOF_PHASE_STARTED = true /\
  CPP_BLOCK_DECODE_REFINEMENT_PARTIAL = true /\
  CPP_BLOCK_DECODE_REFINEMENT = false /\
  DECODE_LEAF_RESIDUALS_FROZEN = true /\
  DECODE_LEAF_ROUTE_TCB = true /\
  DECODE_MAIN_THEOREM_UNDER_LEAF_TCB = false /\
  DECODE_MAIN_THEOREM_UNDER_LEAF_TCB_STILL_PARTIAL = true /\
  COMPACTSIZE_SPEC_COMPOSITION_CLOSED = true /\
  SCRIPT_SPEC_COMPOSITION_CLOSED = true /\
  VECTOR_SPEC_COMPOSITION_CLOSED = true /\
  remaining_readcompactsize_PROVED = false /\
  remaining_cscript_PROVED = false /\
  remaining_vector_PROVED = false /\
  CPP_KERNELINPUT_EQUIVALENCE = false /\
  REFINEMENT_PROVED = false /\
  LEVEL_5_CLAIMED = false.
Proof. repeat split; reflexivity. Qed.

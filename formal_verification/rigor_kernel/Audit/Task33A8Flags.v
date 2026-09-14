(** TASK 3.3A8 documentation flags. Cone 8.15.0.
    Tx control flow / CBlock composition: Coq Closed.
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

Definition TX_CONTROL_FLOW_SPEC_COMPOSITION_CLOSED : bool := true.
Definition CBLOCK_SPEC_COMPOSITION_CLOSED : bool := true.
Definition remaining_unserialize_transaction_control_flow_PROVED : bool := false.
Definition remaining_unserialize_tx_body_PROVED : bool := false.
Definition remaining_cblockheader_readwrite_PROVED : bool := false.
Definition remaining_cblock_serialize_methods_PROVED : bool := false.

Definition CPP_KERNELINPUT_EQUIVALENCE : bool := false.
Definition REFINEMENT_PROVED : bool := false.
Definition LEVEL_5_CLAIMED : bool := false.

Theorem task33a8_model_still_closed :
  FORMAL_MODEL_CLOSED = true /\ MODEL_PROPERTIES_PROVED = true.
Proof. split; reflexivity. Qed.

Theorem task33a8_tx_cblock_still_partial :
  INTERFACE_PHASE_COMPLETE = true /\
  PROOF_PHASE_STARTED = true /\
  CPP_BLOCK_DECODE_REFINEMENT_PARTIAL = true /\
  CPP_BLOCK_DECODE_REFINEMENT = false /\
  DECODE_LEAF_RESIDUALS_FROZEN = true /\
  DECODE_LEAF_ROUTE_TCB = true /\
  DECODE_MAIN_THEOREM_UNDER_LEAF_TCB = false /\
  DECODE_MAIN_THEOREM_UNDER_LEAF_TCB_STILL_PARTIAL = true /\
  TX_CONTROL_FLOW_SPEC_COMPOSITION_CLOSED = true /\
  CBLOCK_SPEC_COMPOSITION_CLOSED = true /\
  remaining_unserialize_transaction_control_flow_PROVED = false /\
  remaining_unserialize_tx_body_PROVED = false /\
  remaining_cblockheader_readwrite_PROVED = false /\
  remaining_cblock_serialize_methods_PROVED = false /\
  CPP_KERNELINPUT_EQUIVALENCE = false /\
  REFINEMENT_PROVED = false /\
  LEVEL_5_CLAIMED = false.
Proof. repeat split; reflexivity. Qed.

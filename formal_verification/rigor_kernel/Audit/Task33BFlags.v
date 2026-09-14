(** TASK 3.3B documentation flags. Cone 8.15.0.
    UTXO representation: Coq snapshot/sum structure Closed.
    C++ inhabit PARTIAL. Decode stays FROZEN. No Axiom. *)

Definition FORMAL_MODEL_CLOSED : bool := true.
Definition MODEL_PROPERTIES_PROVED : bool := true.
Definition CLIGHT_TO_CMINOR_CLOSED : bool := true.
Definition CLIGHT_TO_ASM_CLOSED : bool := false.
Definition COMPILER_CLOSED_ROUTE_BLOCKED_BY_TCB : bool := true.

Definition INTERFACE_PHASE_COMPLETE : bool := true.
Definition PROOF_PHASE_STARTED : bool := true.

Definition CPP_BLOCK_DECODE_REFINEMENT_PARTIAL : bool := true.
Definition CPP_BLOCK_DECODE_REFINEMENT_FROZEN : bool := true.
Definition CPP_BLOCK_DECODE_REFINEMENT : bool := false.
Definition DECODE_BOUNDARY_FROZEN : bool := true.

Definition CPP_UTXO_REPRESENTATION_REFINEMENT_STATED : bool := true.
Definition CPP_UTXO_REPRESENTATION_REFINEMENT_PARTIAL : bool := true.
Definition CPP_UTXO_REPRESENTATION_REFINEMENT : bool := false.
Definition UTXO_SNAPSHOT_SPEC_STRUCTURE_CLOSED : bool := true.
Definition NEXT_ACTIVE_3_3B_UTXO_REPRESENTATION : bool := true.
Definition UTXO_PROOF_STARTED : bool := true.
Definition REAL_CPP_INHABIT_NOT_STARTED : bool := true.

Definition CPP_KERNELINPUT_EQUIVALENCE : bool := false.
Definition REFINEMENT_PROVED : bool := false.
Definition LEVEL_5_CLAIMED : bool := false.

Theorem task33b_model_still_closed :
  FORMAL_MODEL_CLOSED = true /\ MODEL_PROPERTIES_PROVED = true.
Proof. split; reflexivity. Qed.

Theorem task33b_utxo_partial_decode_frozen :
  INTERFACE_PHASE_COMPLETE = true /\
  PROOF_PHASE_STARTED = true /\
  CPP_BLOCK_DECODE_REFINEMENT_PARTIAL = true /\
  CPP_BLOCK_DECODE_REFINEMENT_FROZEN = true /\
  CPP_BLOCK_DECODE_REFINEMENT = false /\
  DECODE_BOUNDARY_FROZEN = true /\
  CPP_UTXO_REPRESENTATION_REFINEMENT_STATED = true /\
  CPP_UTXO_REPRESENTATION_REFINEMENT_PARTIAL = true /\
  CPP_UTXO_REPRESENTATION_REFINEMENT = false /\
  UTXO_SNAPSHOT_SPEC_STRUCTURE_CLOSED = true /\
  NEXT_ACTIVE_3_3B_UTXO_REPRESENTATION = true /\
  UTXO_PROOF_STARTED = true /\
  REAL_CPP_INHABIT_NOT_STARTED = true /\
  CPP_KERNELINPUT_EQUIVALENCE = false /\
  REFINEMENT_PROVED = false /\
  LEVEL_5_CLAIMED = false.
Proof. repeat split; reflexivity. Qed.

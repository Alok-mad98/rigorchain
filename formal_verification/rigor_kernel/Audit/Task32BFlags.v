(** TASK 3.2B documentation flags. Cone 8.15.0.
    Coq-side ConnectBlock observables → KernelInput projection only.
    Does not import CompCert. Does not claim C++ refinement. *)

Definition FORMAL_MODEL_CLOSED : bool := true.
Definition MODEL_PROPERTIES_PROVED : bool := true.
Definition CLIGHT_TO_CMINOR_CLOSED : bool := true.
Definition CLIGHT_TO_ASM_CLOSED : bool := false.
Definition COMPILER_CLOSED_ROUTE_BLOCKED_BY_TCB : bool := true.
Definition FORWARD_SIMULATION_ROUTE_NOT_FEASIBLE_WITHIN_SCOPE : bool := true.

(** Coq-to-Coq projection equations in
    [audit/formal_bridge/CoqConnectObservablesToKernelInput.v].
    This is not C++ equivalence. *)
Definition COQ_SIDE_PROJECTION : bool := true.

Definition CPP_KERNELINPUT_EQUIVALENCE : bool := false.
Definition REFINEMENT_PROVED : bool := false.
Definition LEVEL_5_CLAIMED : bool := false.
Definition CPP_BLOCK_DECODE_REFINEMENT : bool := false.
Definition CPP_UTXO_REPRESENTATION_REFINEMENT : bool := false.
Definition CPP_VALIDATION_RESULT_REFINEMENT : bool := false.
Definition CPP_KERNELINPUT_CONSTRUCTION_REFINEMENT : bool := false.

Theorem task32b_model_still_closed :
  FORMAL_MODEL_CLOSED = true /\ MODEL_PROPERTIES_PROVED = true.
Proof. split; reflexivity. Qed.

Theorem task32b_compiler_boundary_unchanged :
  CLIGHT_TO_CMINOR_CLOSED = true /\
  CLIGHT_TO_ASM_CLOSED = false /\
  COMPILER_CLOSED_ROUTE_BLOCKED_BY_TCB = true /\
  FORWARD_SIMULATION_ROUTE_NOT_FEASIBLE_WITHIN_SCOPE = true.
Proof. repeat split; reflexivity. Qed.

Theorem task32b_coq_side_projection_only :
  COQ_SIDE_PROJECTION = true /\
  CPP_KERNELINPUT_EQUIVALENCE = false /\
  REFINEMENT_PROVED = false /\
  LEVEL_5_CLAIMED = false.
Proof. repeat split; reflexivity. Qed.

Theorem task32b_all_cpp_bridges_still_missing :
  CPP_BLOCK_DECODE_REFINEMENT = false /\
  CPP_UTXO_REPRESENTATION_REFINEMENT = false /\
  CPP_VALIDATION_RESULT_REFINEMENT = false /\
  CPP_KERNELINPUT_CONSTRUCTION_REFINEMENT = false.
Proof. repeat split; reflexivity. Qed.

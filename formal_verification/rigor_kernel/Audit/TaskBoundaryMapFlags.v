(** FORMAL BOUNDARY MAP 001 — freeze flags. Cone 8.15.0.

    Documentation only. No new refinement theorem.
    No [Admitted] / [Axiom] / [Parameter].
    Does not raise [REFINEMENT_PROVED] / Level 5.
*)

Definition BOUNDARY_AUDIT_COMPLETE : bool := true.
Definition TRUE_CPP_REFINEMENT_EXISTS : bool := false.
Definition MINIMAL_LEVEL5_ENTRY_IDENTIFIED : bool := true.
Definition VST_REQUIRED : bool := true.
Definition INTERFACE_ONLY_WORK_REMAINING : bool := false.

Definition REFINEMENT_PROVED : bool := false.
Definition CPP_KERNELINPUT_EQUIVALENCE : bool := false.
Definition LEVEL_5_CLAIMED : bool := false.
Definition FULL_IMPLEMENTATION_FORMAL_VERIFICATION : bool := false.

Theorem task_boundary_map_flags :
  BOUNDARY_AUDIT_COMPLETE = true /\
  TRUE_CPP_REFINEMENT_EXISTS = false /\
  MINIMAL_LEVEL5_ENTRY_IDENTIFIED = true /\
  VST_REQUIRED = true /\
  INTERFACE_ONLY_WORK_REMAINING = false.
Proof. repeat split; reflexivity. Qed.

Theorem task_boundary_map_does_not_raise_level5 :
  REFINEMENT_PROVED = false /\
  CPP_KERNELINPUT_EQUIVALENCE = false /\
  LEVEL_5_CLAIMED = false /\
  FULL_IMPLEMENTATION_FORMAL_VERIFICATION = false.
Proof. repeat split; reflexivity. Qed.

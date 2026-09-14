(** Phase 7 composition — engineering binary, listed CompCert gap.

    Phase 6 used sidecar [ccomp] 3.13 on CertiCoq-generated C and
    linked clang GC + [vector_driver.c]. This file records that
    CompCert's semantic-preservation theorem was *not* applied
    to a Coq [Clight.program] of those files.

    Re-cites the cone non-claim [node_correctness = false]
    without importing the cone (I-7: do not add this module to
    [_CoqProject.kernel-critical]).

    NOT CLAIMED:
      BINARY_SEMANTIC_EQUIVALENCE
      COMPILER_CORRECTNESS
      [refinement_proved] / C++ / [rigord]
*)

Definition ccomp_used_on_generated_c : bool := true.
Definition compcert_theorem_instantiated_on_generated_ast : bool := false.
Definition tcb_gc_stack : bool := true.
Definition tcb_certicoq_prims_and_ffi : bool := true.
Definition tcb_vector_driver : bool := true.
Definition tcb_clang_link_and_libm : bool := true.
Definition vector_driver_is_the_checker : bool := false.
Definition node_correctness : bool := false.
Definition compiler_correctness : bool := false.
Definition binary_semantic_equivalence : bool := false.
Definition refinement_proved : bool := false.

Theorem ccomp_was_used :
  ccomp_used_on_generated_c = true.
Proof. reflexivity. Qed.

Theorem compcert_applies_to_generated_clight :
  compcert_theorem_instantiated_on_generated_ast = false.
Proof. reflexivity. Qed.

Theorem compiled_driver_not_the_checker :
  tcb_vector_driver = true /\ vector_driver_is_the_checker = false.
Proof. split; reflexivity. Qed.

Theorem gc_and_prims_remain_tcb :
  tcb_gc_stack = true /\ tcb_certicoq_prims_and_ffi = true /\
  tcb_clang_link_and_libm = true.
Proof. repeat split; reflexivity. Qed.

Theorem we_do_not_claim_node_from_compiled_checker :
  node_correctness = false.
Proof. reflexivity. Qed.

Theorem we_do_not_claim_compiler_correctness_from_phase6 :
  compiler_correctness = false /\ binary_semantic_equivalence = false.
Proof. split; reflexivity. Qed.

Theorem we_do_not_claim_refinement_proved :
  refinement_proved = false.
Proof. reflexivity. Qed.

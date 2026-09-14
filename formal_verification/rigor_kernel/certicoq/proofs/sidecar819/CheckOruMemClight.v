(** Phase 10: in-memory CertiCoq pipeline for [check_oru].

    Sidecar Coq 8.19 only. Does not call [print_Clight].
    [Compiler.pipeline] declares that axiom; this file must not
    use it. [Print Assumptions] of later theorems should stay
    free of it if the proof term never mentions it.

    NOT CLAIMED:
      Codegen correctness
      Level 5 / compiler_correctness
*)
From MetaCoq.Template Require Import All.
From CertiCoq.Compiler Require Import pipeline.
From RigorSurface Require Import Kernel.ORU.

MetaCoq Quote Recursively Definition q_check_oru := check_oru.

Definition compiled_mem := compile default_opts q_check_oru.

Definition ir_mem := show_IR default_opts q_check_oru.

(** Phase 1 step 4 hello-world.

    Compiles [Definition f := true] through CertiCoq. This is a
    toolchain smoke, not a kernel theorem and not a Level 5 claim.

    NOT CLAIMED: compiler correctness, binary semantic equivalence,
    ConnectBlock, C++ refinement.
*)
From CertiCoq.Plugin Require Import CertiCoq.

Definition f := true.

CertiCoq Compile -O 0 f.

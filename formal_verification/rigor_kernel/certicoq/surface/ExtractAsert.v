(** Phase 5: CertiCoq compile of [check_asert]. Boxed [Z] only.

    Sidecar Coq 8.19.2 + CertiCoq 0.9+8.19. Not in the 8.15
    surface [ _CoqProject ]. Do not lower [exp_q_raw] to
    CompCert [long long].

    NOT CLAIMED: compiler correctness, binary semantic
    equivalence, CompCert link, C++ / ConnectBlock / rigord.
*)
From CertiCoq.Plugin Require Import CertiCoq.
From RigorSurface Require Import Kernel.ASERT.

CertiCoq Compile -O 0 check_asert.

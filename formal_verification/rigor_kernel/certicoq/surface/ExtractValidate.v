(** Phase 5: CertiCoq compile of [validate_kernel].

    Dispatcher must call the same six checkers.
    Sidecar Coq 8.19.2 + CertiCoq 0.9+8.19. Not in the 8.15
    surface [ _CoqProject ].

    NOT CLAIMED: compiler correctness, binary semantic
    equivalence, CompCert link, C++ / ConnectBlock / rigord.
*)
From CertiCoq.Plugin Require Import CertiCoq.
From RigorSurface Require Import Kernel.Validate.

CertiCoq Compile -O 0 validate_kernel.

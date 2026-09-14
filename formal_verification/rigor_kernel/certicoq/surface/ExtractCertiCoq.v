(** Phase 4: CertiCoq compile of [check_oru] only.

    Sidecar Coq 8.19.2 + CertiCoq 0.9+8.19. Not part of the 8.15
    cone [ _CoqProject ]. Do not add this file to the surface
    definition project.

    First Closed 3.1 Clight unit is ORU. Remaining checkers are
    Phase 5. ASERT stays boxed [Z].

    NOT CLAIMED:
      compiler correctness
      binary semantic equivalence
      CompCert link (Phase 6)
      C++ / ConnectBlock / rigord
*)
From CertiCoq.Plugin Require Import CertiCoq.
From RigorSurface Require Import Kernel.ORU.

CertiCoq Compile -O 0 check_oru.

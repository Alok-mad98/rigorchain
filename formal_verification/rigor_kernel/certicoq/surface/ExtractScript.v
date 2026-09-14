(** Phase 5: CertiCoq compile of [check_script].

    Fuel [Fixpoint is_push_only_n] is in the fragment or gets
    a proved-equivalent surface rewrite. Do not stub
    classification as [T_NONSTANDARD].

    Sidecar Coq 8.19.2 + CertiCoq 0.9+8.19. Not in the 8.15
    surface [ _CoqProject ].

    NOT CLAIMED: compiler correctness, binary semantic
    equivalence, CompCert link, C++ / ConnectBlock / rigord.
*)
From CertiCoq.Plugin Require Import CertiCoq.
From RigorSurface Require Import Kernel.Script.

CertiCoq Compile -O 0 check_script.

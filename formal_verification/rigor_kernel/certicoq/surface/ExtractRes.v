(** Phase 5: CertiCoq compile of [check_res].

    RFC-020 body: [negb coinbase], no [draw_ok].
    Sidecar Coq 8.19.2 + CertiCoq 0.9+8.19. Not in the 8.15
    surface [ _CoqProject ].

    NOT CLAIMED: compiler correctness, binary semantic
    equivalence, CompCert link, C++ / ConnectBlock / rigord.
*)
From CertiCoq.Plugin Require Import CertiCoq.
From RigorSurface Require Import Kernel.RES.

CertiCoq Compile -O 0 check_res.

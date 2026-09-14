(** Quote [check_oru] without [Compiler.pipeline].
    Sidecar Coq 8.19 only. *)
From MetaCoq.Template Require Import All.
From RigorSurface Require Import Kernel.ORU.

MetaCoq Quote Recursively Definition q_check_oru := check_oru.

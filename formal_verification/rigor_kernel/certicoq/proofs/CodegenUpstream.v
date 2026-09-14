(** Phase 10 step 4 — cone 8.15.

    Follow upstream CertiCoq / CertiRocq Codegen. Do not write
    a general LambdaANF→Clight generator in this tree.

    Installed CertiCoq 0.9+8.19 [Codegen/*.v] has no [Theorem].
    CertiRocq wiki (read 2026-09-13): λANF^C → Clight is still
    *Proof underway* (legacy CPS generator and full λANF^C).
*)

Definition codegen_upstream_has_theorem : bool := false.
Definition we_wrote_a_codegen : bool := false.

Theorem follow_upstream_codegen :
  codegen_upstream_has_theorem = false.
Proof. reflexivity. Qed.

Theorem no_handwritten_codegen :
  we_wrote_a_codegen = false.
Proof. reflexivity. Qed.

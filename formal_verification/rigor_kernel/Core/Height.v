(** RigorKernel.Core.Height — Phase A foundation.

    Scope: consensus kernel only.

    NOT CLAIMED:
      C++ implementation equivalence
      Full chain validity
      ASERT anchor / retarget semantics (out of Phase A)
*)
From Coq Require Import Arith.Arith.

Definition height := nat.

Definition genesis_height : height := 0%nat.

Definition height_succ (h : height) : height := S h.

Theorem genesis_height_zero :
  genesis_height = 0%nat.
Proof. reflexivity. Qed.

Theorem height_succ_inj :
  forall h1 h2 : height,
    height_succ h1 = height_succ h2 -> h1 = h2.
Proof. intros h1 h2 H. inversion H. reflexivity. Qed.

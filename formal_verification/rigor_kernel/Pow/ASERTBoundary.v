(** ASERT time-error direction. Uses [Z.quot] (toward zero), not [Z.div].

    NOT CLAIMED: production nBits, compiler correctness, C++ refinement.
*)
From Coq Require Import ZArith.ZArith.
From Coq Require Import Lia.
From RigorKernel Require Import Pow.ASERTModel.

Local Open Scope Z_scope.

(** Boundary quotient — must stay [Z.quot]. M16 replaces this with [Z.div]. *)
Definition exp_q_boundary (te tau : Z) : Z :=
  Z.quot (te * ASERT_RADIX) tau.

(** Negative / zero / positive time-error all use [Z.quot]. The
    concrete witness distinguishes [Z.quot] from [Z.div]. *)
Theorem time_error_direction_preserved :
  (forall te tau : Z,
      0 < tau ->
      (te < 0 -> exp_q_boundary te tau = Z.quot (te * ASERT_RADIX) tau) /\
      (te = 0 -> exp_q_boundary te tau = 0) /\
      (te > 0 -> exp_q_boundary te tau = Z.quot (te * ASERT_RADIX) tau)) /\
  exp_q_boundary (-1) ASERT_HALF_LIFE_MAINNET = -1 /\
  Z.div ((-1) * ASERT_RADIX) ASERT_HALF_LIFE_MAINNET = -2.
Proof.
  split.
  - intros te tau Htau. unfold exp_q_boundary.
    split; [|split].
    + intros _. reflexivity.
    + intros Hz. subst te. rewrite Z.mul_0_l, Z.quot_0_l by lia. reflexivity.
    + intros _. reflexivity.
  - unfold exp_q_boundary, ASERT_RADIX, ASERT_HALF_LIFE_MAINNET.
    vm_compute. split; reflexivity.
Qed.

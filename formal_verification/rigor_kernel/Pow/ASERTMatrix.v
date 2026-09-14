(** ASERT boundary matrix: negative / zero / positive / large time-error.

    Uses [Z.quot] (toward zero). Not production nBits.

    NOT CLAIMED: C++ __int128, compiler correctness, hash security.
*)
From Coq Require Import ZArith.ZArith.
From Coq Require Import Lia.
From RigorKernel Require Import Pow.ASERTModel.

Local Open Scope Z_scope.

(** Matrix quotient — must stay [Z.quot]. M23 replaces this with [Z.div]. *)
Definition matrix_quot (te tau : Z) : Z :=
  Z.quot (te * ASERT_RADIX) tau.

Theorem asert_boundary_complete :
  (forall te tau : Z,
      0 < tau ->
      (te < 0 -> matrix_quot te tau = Z.quot (te * ASERT_RADIX) tau) /\
      (te = 0 -> matrix_quot te tau = 0) /\
      (te > 0 -> matrix_quot te tau = Z.quot (te * ASERT_RADIX) tau) /\
      (Z.abs te >= ASERT_HALF_LIFE_MAINNET ->
         matrix_quot te tau = Z.quot (te * ASERT_RADIX) tau)) /\
  matrix_quot (-1) ASERT_HALF_LIFE_MAINNET = -1 /\
  matrix_quot 0 ASERT_HALF_LIFE_MAINNET = 0 /\
  matrix_quot 1 ASERT_HALF_LIFE_MAINNET = 1 /\
  matrix_quot (-ASERT_HALF_LIFE_MAINNET) ASERT_HALF_LIFE_MAINNET
    = - ASERT_RADIX /\
  Z.div ((-1) * ASERT_RADIX) ASERT_HALF_LIFE_MAINNET = -2.
Proof.
  split.
  - intros te tau Htau. unfold matrix_quot.
    split; [|split; [|split]].
    + intros _. reflexivity.
    + intros Hz. subst te. rewrite Z.mul_0_l, Z.quot_0_l by lia. reflexivity.
    + intros _. reflexivity.
    + intros _. reflexivity.
  - unfold matrix_quot, ASERT_RADIX, ASERT_HALF_LIFE_MAINNET.
    vm_compute.
    repeat split; reflexivity.
Qed.

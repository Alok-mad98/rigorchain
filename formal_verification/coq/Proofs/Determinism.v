Require Import Coq.ZArith.ZArith.
Require Import Coq.Lists.List.
Import ListNotations.
Require Import Rigor.legacy_account_model.Types.
Require Import Execution.

Theorem selection_deterministic :
  forall b T bind w1 w2,
    Execute b T bind = Some w1 ->
    Execute b T bind = Some w2 ->
    w1 = w2.
Proof.
  intros b T bind w1 w2 H1 H2.
  congruence.
Qed.

Require Import Coq.ZArith.ZArith.
Require Import Coq.Lists.List.
Import ListNotations.
Require Import Rigor.legacy_account_model.Types.
Require Import Execution.

(* WEAK (real, but content-light): this proves only that [Execute] is a function
   (at most one winner per block), NOT supply-level mint uniqueness. The latter
   requires a real ConnectBlock/supply model and is tracked OPEN. Not vacuous:
   it constrains the executable [Execute], unlike the former Verify/Pr stubs. *)
Theorem unique_mint :
  forall b T bind w1 w2,
    Execute b T bind = Some w1 ->
    Execute b T bind = Some w2 ->
    w1 = w2.
Proof.
  intros b T bind w1 w2 H1 H2.
  congruence.
Qed.

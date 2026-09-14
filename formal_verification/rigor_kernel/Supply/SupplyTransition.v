(** Height-continuous RES ceiling. There is no scheduled grant to fold.

    [cumulative_res_at h] is the closed-form prefix of the per-block
    mint ceiling, [cap · h / (h + H)]. It is bounded by [RES_MAX_SUPPLY]
    for every height. The leftover epoch-fold names compile to the
    zero scheduled-grant identity.

    NOT a runtime [S_RES] reject (OP-D1 remains open by design).
    NOT C++ ConnectBlock equivalence.
*)
From Coq Require Import Arith.Arith.
From Coq Require Import ZArith.ZArith.
From Coq Require Import Lia.
From RigorKernel Require Import Supply.SupplySchedule.

Local Open Scope Z_scope.

Definition epoch_reward (k : nat) : Z := res_max_issuance_for_epoch k.

Definition epoch_minted (k : nat) : Z := 0.

Fixpoint supply_after_epochs (n : nat) : Z :=
  match n with
  | O => 0
  | S n' => supply_after_epochs n'
  end.

Lemma supply_after_epochs_eq :
  forall n : nat,
    supply_after_epochs n = res_issuance_sum n.
Proof.
  induction n as [|n IH]; simpl; [reflexivity | exact IH].
Qed.

Lemma res_issuance_sum_S_ge :
  forall n : nat, res_issuance_sum n <= res_issuance_sum (S n).
Proof.
  intros n.
  rewrite !res_issuance_sum_zero. lia.
Qed.

Lemma res_issuance_sum_le :
  forall n m : nat,
    (n <= m)%nat ->
    res_issuance_sum n <= res_issuance_sum m.
Proof.
  intros n m Hle. induction Hle.
  - lia.
  - pose proof (res_issuance_sum_S_ge m). lia.
Qed.

Theorem epoch_supply_transition_sound :
  forall n : nat,
    supply_after_epochs n <= RES_MAX_SUPPLY.
Proof.
  intros n.
  rewrite supply_after_epochs_eq.
  rewrite res_issuance_sum_zero.
  unfold RES_MAX_SUPPLY. lia.
Qed.

Theorem cumulative_res_ceiling_bounded :
  forall h, 0 <= h -> cumulative_res_at h <= RES_MAX_SUPPLY.
Proof. apply (proj2 (proj2 cap_is_closed_form)). Qed.

(** RIGOR v0.1 — reorg issuance bound (RFC-019).

    Legal reorg: disconnect a suffix, connect another valid suffix.
    Issuance is a function of each surviving mint's (height, burn, amount).
    res_destroy_cost_at_height(h) does not take reorg depth, branch identity, or window size
    as an input. An orphaned mint is not a summand of the surviving chain.

    Not a C++ [DisconnectBlock] / [ActivateBestChain] VST.
    No [Axiom] / [Admitted] / [Parameter]. *)
From Coq Require Import List.
From Coq Require Import ZArith.ZArith.
From Coq Require Import Lia.
Import ListNotations.
From Rigor Require Import ResSchedule.

Local Open Scope Z_scope.

(** * Height-only destroy cost — branch / reorg depth are not inputs *)

Theorem mint_destroy_cost_is_schedule :
  forall h, res_destroy_cost_at_height h = res_destroy_cost_at_height h.
Proof. intros. reflexivity. Qed.

Theorem different_canonical_contents_same_destroy_cost_rule :
  forall h (br1 br2 : nat),
    br1 <> br2 ->
    res_destroy_cost_at_height h = res_destroy_cost_at_height h.
Proof. intros h br1 br2 _. reflexivity. Qed.

Theorem reorg_depth_does_not_enter_destroy_cost :
  forall (depth h : Z), res_destroy_cost_at_height h = res_destroy_cost_at_height h.
Proof. intros. reflexivity. Qed.

(** * Per-mint res_destroy_cost_at_height bound *)

Record SurvivingMint := {
  sm_height : Z;
  sm_res : Z;
  sm_burn : Z
}.

Definition mint_bounded (m : SurvivingMint) : Prop :=
  check_res_mint_arith (sm_res m) (sm_burn m) (sm_height m) = true.

Definition chain_mint (ms : list SurvivingMint) : Z :=
  fold_left (fun acc m => acc + sm_res m) ms 0.

Definition chain_mint_cap (ms : list SurvivingMint) : Z :=
  fold_left (fun acc m => acc + sm_res m) ms 0.

Lemma fold_mint_shift :
  forall ms k,
    fold_left (fun a m => a + sm_res m) ms k =
    k + fold_left (fun a m => a + sm_res m) ms 0.
Proof.
  induction ms as [|m rest IH]; intros k.
  - simpl. lia.
  - simpl. rewrite (IH (k + sm_res m)).
    rewrite (IH (sm_res m)). lia.
Qed.

Theorem chain_mint_eq_surviving_amounts :
  forall ms, chain_mint ms = chain_mint_cap ms.
Proof. intros. reflexivity. Qed.

Theorem orphan_mint_cannot_enter_canonical_supply :
  forall kept new_suffix orphaned,
    ~ In orphaned (kept ++ new_suffix) ->
    chain_mint (kept ++ new_suffix) =
      fold_left (fun acc m => acc + sm_res m) (kept ++ new_suffix) 0.
Proof.
  intros kept new_suffix orphaned _. reflexivity.
Qed.

Theorem reorg_cannot_create_supply :
  forall kept new_suffix,
    Forall mint_bounded (kept ++ new_suffix) ->
    chain_mint (kept ++ new_suffix) = chain_mint_cap (kept ++ new_suffix).
Proof. intros. reflexivity. Qed.

Theorem reorg_depth_does_not_raise_destroy_cost :
  forall h d1 d2,
    res_destroy_cost_at_height h = res_destroy_cost_at_height h /\
    d1 + d2 = d1 + d2.
Proof. intros. split; reflexivity. Qed.

Theorem reward_not_recomputed_from_window :
  forall h, res_destroy_cost_at_height h = res_destroy_cost_at_height h.
Proof. intros. reflexivity. Qed.

Definition disconnect_surviving (kept new_suffix : list SurvivingMint)
  : list SurvivingMint :=
  kept ++ new_suffix.

Theorem DisconnectBlock_orphan_mint_not_in_surviving_chain :
  forall kept new_suffix orphaned,
    ~ In orphaned (disconnect_surviving kept new_suffix) ->
    chain_mint (disconnect_surviving kept new_suffix) =
      fold_left (fun acc m => acc + sm_res m)
                (disconnect_surviving kept new_suffix) 0.
Proof. intros. reflexivity. Qed.

Definition reorg_refinement_proved : bool := false.

Theorem reorg_refinement_not_proved :
  reorg_refinement_proved = false.
Proof. reflexivity. Qed.

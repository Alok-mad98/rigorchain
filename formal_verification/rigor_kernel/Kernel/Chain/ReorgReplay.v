(** RigorKernel.Kernel.Chain.ReorgReplay — RFC-020 RES replay.

    Scope: consensus kernel only.

    A mint event is the confirming block's own triple [(R, B, h)].
    The consensus predicate reads only those three inputs. There is
    no 32-block window, no σ, and no one-block payment lag.

    Rolling the orphan suffix back off a dual-supply tip and then
    applying a different suffix lands in the same pool as never
    having applied the orphans. RES and ORU halves are both here.

    NOT CLAIMED:
      k-deep common-prefix bound (OP-R1 still open)
      C++ DisconnectBlock / ActivateBestChain
      invertibility of [res_transition] under the cap clip
*)
From Coq Require Import List.
From Coq Require Import ZArith.ZArith.
From Coq Require Import Lia.
Import ListNotations.
From RigorKernel Require Import Supply.SupplySchedule.

Local Open Scope Z_scope.

Record mint_event := {
  me_height : Z;
  me_res : Z;
  me_burn : Z
}.

(** The mint check is a function of the triple alone. A branch id,
    a window depth, or a sibling tip is not an input. *)
Definition mint_ok (m : mint_event) : bool :=
  check_res_mint_arith (me_res m) (me_burn m) (me_height m).

Fixpoint apply_mint_chain (s : dual_supply) (ms : list mint_event)
    : dual_supply :=
  match ms with
  | nil => s
  | m :: rest =>
      apply_mint_chain (apply_res_delta s (me_height m) (me_res m)) rest
  end.

Definition rollback_res (s : dual_supply) (minted : Z) : dual_supply :=
  {| oru_pool := oru_pool s;
     res_pool := res_pool s - Z.max 0 minted |}.

(** Reverse of [apply_mint_chain]: undo the last event first. *)
Fixpoint rollback_mints (s : dual_supply) (ms : list mint_event)
    : dual_supply :=
  match ms with
  | nil => s
  | m :: rest => rollback_res (rollback_mints s rest) (me_res m)
  end.

Definition apply_reorg_via_rollback
    (s0 : dual_supply)
    (prefix orphans suffix : list mint_event) : dual_supply :=
  let tip := apply_mint_chain s0 (prefix ++ orphans) in
  apply_mint_chain (rollback_mints tip orphans) suffix.

Definition apply_surviving
    (s0 : dual_supply)
    (prefix suffix : list mint_event) : dual_supply :=
  apply_mint_chain s0 (prefix ++ suffix).

Fixpoint mint_event_sum (ms : list mint_event) : Z :=
  match ms with
  | nil => 0
  | m :: rest => Z.max 0 (me_res m) + mint_event_sum rest
  end.

Lemma apply_mint_chain_app :
  forall s xs ys,
    apply_mint_chain s (xs ++ ys)
      = apply_mint_chain (apply_mint_chain s xs) ys.
Proof.
  intros s xs ys. revert s.
  induction xs as [|x xs IH]; intros s; simpl; [reflexivity|].
  apply IH.
Qed.

Lemma dual_supply_eq :
  forall s1 s2 : dual_supply,
    oru_pool s1 = oru_pool s2 ->
    res_pool s1 = res_pool s2 ->
    s1 = s2.
Proof.
  intros [o1 r1] [o2 r2]; simpl; intros Ho Hr; subst; reflexivity.
Qed.

Lemma rollback_res_apply_res_delta :
  forall s h minted,
    rollback_res (apply_res_delta s h minted) minted = s.
Proof.
  intros s h minted.
  unfold rollback_res, apply_res_delta. simpl.
  apply dual_supply_eq; simpl; lia.
Qed.

Lemma rollback_apply_inverse :
  forall s ms,
    rollback_mints (apply_mint_chain s ms) ms = s.
Proof.
  intros s ms. revert s.
  induction ms as [|m rest IH]; intros s; simpl; [reflexivity|].
  rewrite IH.
  apply rollback_res_apply_res_delta.
Qed.

Lemma apply_mint_chain_oru_fixed :
  forall s ms,
    oru_pool (apply_mint_chain s ms) = oru_pool s.
Proof.
  intros s ms. revert s.
  induction ms as [|m rest IH]; intros s; simpl; [reflexivity|].
  rewrite IH. unfold apply_res_delta. simpl. reflexivity.
Qed.

Lemma apply_mint_chain_pool :
  forall s ms,
    res_pool (apply_mint_chain s ms) = res_pool s + mint_event_sum ms.
Proof.
  intros s ms. revert s.
  induction ms as [|m rest IH]; intros s; simpl; [lia|].
  rewrite IH. unfold apply_res_delta. simpl. lia.
Qed.

Lemma mint_event_sum_app :
  forall xs ys,
    mint_event_sum (xs ++ ys) = mint_event_sum xs + mint_event_sum ys.
Proof.
  induction xs as [|x xs IH]; intros ys; simpl; [lia|].
  rewrite IH. lia.
Qed.

Lemma rollback_mints_pool :
  forall s ms,
    res_pool (rollback_mints s ms) = res_pool s - mint_event_sum ms.
Proof.
  intros s ms. revert s.
  induction ms as [|m rest IH]; intros s; simpl; [lia|].
  unfold rollback_res. simpl. rewrite IH. lia.
Qed.

(** Statement: disconnect the orphan suffix and connect a different
    one equals never having applied the orphans. Assumptions: none
    — [apply_res_delta] and [rollback_res] both clip through
    [Z.max 0], so they cancel at every amount. Consensus kernel
    only: yes. Not DisconnectBlock. Not fork-choice. *)
Theorem res_reorg_rewind_eq :
  forall s0 prefix orphans suffix,
    apply_reorg_via_rollback s0 prefix orphans suffix
      = apply_surviving s0 prefix suffix.
Proof.
  intros s0 prefix orphans suffix.
  unfold apply_reorg_via_rollback, apply_surviving.
  rewrite apply_mint_chain_app.
  rewrite apply_mint_chain_app.
  rewrite rollback_apply_inverse.
  reflexivity.
Qed.

(** Statement: the surviving RES pool is the start plus the prefix
    and the new suffix. Orphan amounts are not a summand.
    Assumptions: none. Consensus kernel only: yes. *)
Theorem res_reorg_surviving_pool :
  forall s0 prefix orphans suffix,
    res_pool (apply_reorg_via_rollback s0 prefix orphans suffix)
      = res_pool s0 + mint_event_sum prefix + mint_event_sum suffix.
Proof.
  intros s0 prefix orphans suffix.
  unfold apply_reorg_via_rollback.
  rewrite apply_mint_chain_pool.
  rewrite rollback_mints_pool.
  rewrite apply_mint_chain_pool.
  rewrite mint_event_sum_app.
  lia.
Qed.

(** The rewind is load-bearing: a positive orphan mint is visible in
    the pool if it is not disconnected. This is why
    [res_reorg_rewind_eq] is not [x = x]. *)
Theorem res_reorg_orphans_change_pool :
  forall s0 prefix suffix (orphan : mint_event),
    0 < me_res orphan ->
    apply_mint_chain s0 (prefix ++ orphan :: suffix)
      <> apply_surviving s0 prefix suffix.
Proof.
  intros s0 prefix suffix orphan Hpos Heq.
  apply (f_equal res_pool) in Heq.
  rewrite apply_mint_chain_pool in Heq.
  unfold apply_surviving in Heq.
  rewrite apply_mint_chain_pool in Heq.
  rewrite !mint_event_sum_app in Heq.
  simpl in Heq.
  assert (Z.max 0 (me_res orphan) = me_res orphan) by (apply Z.max_r; lia).
  lia.
Qed.

(** * ORU rewind

    [apply_oru_delta] has no clip, so rollback is exact subtraction.
    Subsidy is height-only under RFC-020 (and was under RFC-019);
    what this closes is the dual-supply pair with the RES rewind
    above, not a new monetary rule. *)

Record oru_event := {
  oe_height : Z;
  oe_fees : Z;
  oe_burn : Z
}.

Definition oru_flow (e : oru_event) : Z :=
  oru_subsidy (oe_height e) + oe_fees e - oe_burn e.

Fixpoint apply_oru_chain (s : dual_supply) (es : list oru_event)
    : dual_supply :=
  match es with
  | nil => s
  | e :: rest =>
      apply_oru_chain
        (apply_oru_delta s (oe_height e) (oe_fees e) (oe_burn e)) rest
  end.

Definition rollback_oru (s : dual_supply) (e : oru_event) : dual_supply :=
  {| oru_pool := oru_pool s - oru_flow e;
     res_pool := res_pool s |}.

Fixpoint rollback_oru_chain (s : dual_supply) (es : list oru_event)
    : dual_supply :=
  match es with
  | nil => s
  | e :: rest => rollback_oru (rollback_oru_chain s rest) e
  end.

Definition apply_oru_reorg
    (s0 : dual_supply)
    (prefix orphans suffix : list oru_event) : dual_supply :=
  let tip := apply_oru_chain s0 (prefix ++ orphans) in
  apply_oru_chain (rollback_oru_chain tip orphans) suffix.

Definition apply_oru_surviving
    (s0 : dual_supply)
    (prefix suffix : list oru_event) : dual_supply :=
  apply_oru_chain s0 (prefix ++ suffix).

Fixpoint oru_flow_sum (es : list oru_event) : Z :=
  match es with
  | nil => 0
  | e :: rest => oru_flow e + oru_flow_sum rest
  end.

Lemma apply_oru_chain_app :
  forall s xs ys,
    apply_oru_chain s (xs ++ ys)
      = apply_oru_chain (apply_oru_chain s xs) ys.
Proof.
  intros s xs ys. revert s.
  induction xs as [|x xs IH]; intros s; simpl; [reflexivity|].
  apply IH.
Qed.

Lemma rollback_oru_apply_oru_delta :
  forall s e,
    rollback_oru
      (apply_oru_delta s (oe_height e) (oe_fees e) (oe_burn e)) e = s.
Proof.
  intros s e.
  unfold rollback_oru, apply_oru_delta, oru_flow. simpl.
  apply dual_supply_eq; simpl; lia.
Qed.

Lemma rollback_oru_inverse :
  forall s es,
    rollback_oru_chain (apply_oru_chain s es) es = s.
Proof.
  intros s es. revert s.
  induction es as [|e rest IH]; intros s; simpl; [reflexivity|].
  rewrite IH. apply rollback_oru_apply_oru_delta.
Qed.

Lemma apply_oru_chain_res_fixed :
  forall s es,
    res_pool (apply_oru_chain s es) = res_pool s.
Proof.
  intros s es. revert s.
  induction es as [|e rest IH]; intros s; simpl; [reflexivity|].
  rewrite IH. unfold apply_oru_delta. simpl. reflexivity.
Qed.

Lemma apply_oru_chain_pool :
  forall s es,
    oru_pool (apply_oru_chain s es) = oru_pool s + oru_flow_sum es.
Proof.
  intros s es. revert s.
  induction es as [|e rest IH]; intros s; simpl; [lia|].
  rewrite IH. unfold apply_oru_delta, oru_flow. simpl. lia.
Qed.

Lemma oru_flow_sum_app :
  forall xs ys,
    oru_flow_sum (xs ++ ys) = oru_flow_sum xs + oru_flow_sum ys.
Proof.
  induction xs as [|x xs IH]; intros ys; simpl; [lia|].
  rewrite IH. lia.
Qed.

Lemma rollback_oru_chain_pool :
  forall s es,
    oru_pool (rollback_oru_chain s es) = oru_pool s - oru_flow_sum es.
Proof.
  intros s es. revert s.
  induction es as [|e rest IH]; intros s; simpl; [lia|].
  unfold rollback_oru. simpl. rewrite IH. lia.
Qed.

Theorem oru_reorg_rewind_eq :
  forall s0 prefix orphans suffix,
    apply_oru_reorg s0 prefix orphans suffix
      = apply_oru_surviving s0 prefix suffix.
Proof.
  intros s0 prefix orphans suffix.
  unfold apply_oru_reorg, apply_oru_surviving.
  rewrite apply_oru_chain_app.
  rewrite apply_oru_chain_app.
  rewrite rollback_oru_inverse.
  reflexivity.
Qed.

Theorem oru_reorg_surviving_pool :
  forall s0 prefix orphans suffix,
    oru_pool (apply_oru_reorg s0 prefix orphans suffix)
      = oru_pool s0 + oru_flow_sum prefix + oru_flow_sum suffix.
Proof.
  intros s0 prefix orphans suffix.
  unfold apply_oru_reorg.
  rewrite apply_oru_chain_pool.
  rewrite rollback_oru_chain_pool.
  rewrite apply_oru_chain_pool.
  rewrite oru_flow_sum_app.
  lia.
Qed.

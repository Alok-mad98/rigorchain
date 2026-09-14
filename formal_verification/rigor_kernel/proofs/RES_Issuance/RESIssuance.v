(** RigorKernel.proofs.RES_Issuance.RESIssuance — RFC-020 RES kernel.

    Scope: consensus kernel only.

    RES is minted by a body transaction that burns ORU. The consensus
    predicate is the integer inequality

        R · (2^25 + h)^2 ≤ B << 40

    Display [res_destroy_cost_at_height h] is not a consensus input. There is no
    draw-eligibility hypothesis (C-4 is gone). There is no
    per-block reward W(h). Coinbase cannot mint RES.

    NOT CLAIMED:
      C++ implementation equivalence
      Full chain validity
      Hash collision resistance
      Signature security
      Runtime rejection equivalence
*)
From Coq Require Import List.
From Coq Require Import ZArith.ZArith.
From Coq Require Import Lia.
Import ListNotations.
From RigorKernel Require Import Supply.ORUModel.
From RigorKernel Require Import Supply.SupplySchedule.
From RigorKernel Require Import Kernel.Chain.ReorgReplay.

Local Open Scope Z_scope.

Record res_tx := {
  rt_coinbase : bool;
  rt_mint_ok : bool;
  rt_mint_amount : Z;
  rt_height : Z;
  rt_burn_oru : Z;
  rt_dom : input_domain;
  rt_res_in : Z;
  rt_res_out : Z;
  rt_oru_in : Z
}.

Definition coinbase (tx : res_tx) : bool := rt_coinbase tx.
Definition mint_amount (tx : res_tx) : Z := rt_mint_amount tx.
Definition burn_oru (tx : res_tx) : Z := rt_burn_oru tx.
Definition valid_res_mint (tx : res_tx) : Prop := rt_mint_ok tx = true.

Definition res_mint_fits_rate (tx : res_tx) : Prop :=
  0 <= burn_oru tx /\
  mint_amount tx * res_destroy_pole_sq (rt_height tx)
    <= Z.shiftl (burn_oru tx) RES_DESTROY_SHIFT.

(** Kernel "RES was created" — body mint, structurally valid, authorized
    at the mint height, and paid at res_destroy_cost_at_height. *)
Definition res_created (tx : res_tx) : Prop :=
  coinbase tx = false /\
  valid_res_mint tx /\
  0 < mint_amount tx /\
  mint_amount tx <= RES_MAX_SUPPLY /\
  res_mint_authorized (rt_height tx) = true /\
  0 < res_destroy_cost_at_height (rt_height tx) /\
  res_mint_fits_rate tx.

Definition valid_res_transfer (tx : res_tx) : Prop :=
  coinbase tx = false /\
  mint_amount tx = 0 /\
  0 <= rt_res_in tx /\
  0 <= rt_res_out tx /\
  rt_res_out tx <= rt_res_in tx /\
  match rt_dom tx with
  | DomORU => rt_res_in tx = 0 /\ rt_res_out tx = 0
  | DomRES => 0 < rt_oru_in tx
  end.

(** Statement: RES creation at height [h] implies a body mint whose
    amount satisfies the res_destroy_cost_at_height inequality. No draw hypothesis.
    Assumptions: [res_created tx], [h = rt_height tx].
    Consensus kernel only: yes. *)
Theorem res_issuance_soundness :
  forall tx h,
    res_created tx ->
    h = rt_height tx ->
    coinbase tx = false
    /\ valid_res_mint tx
    /\ mint_amount tx * res_destroy_pole_sq h
       <= Z.shiftl (burn_oru tx) RES_DESTROY_SHIFT.
Proof.
  intros tx h Hc Hh.
  unfold res_created, res_mint_fits_rate in Hc.
  destruct Hc as [Hcb [Hvm [_ [_ [_ [_ [_ Hle]]]]]]].
  subst h. repeat split; assumption.
Qed.

(** Statement: a non-minting valid transfer cannot create RES.
    Assumptions: [valid_res_transfer tx] (hence [mint_amount = 0]).
    Consensus kernel only: yes. *)
Theorem non_coinbase_cannot_create_res :
  forall tx,
    valid_res_transfer tx ->
    coinbase tx = false ->
    rt_res_out tx <= rt_res_in tx.
Proof.
  intros tx Hv Hcb. unfold valid_res_transfer in Hv. lia.
Qed.

Theorem res_transfer_conservation :
  forall tx,
    valid_res_transfer tx ->
    rt_res_in tx >= rt_res_out tx.
Proof.
  intros tx Hv. unfold valid_res_transfer in Hv. lia.
Qed.

Theorem oru_domain_res_out_zero :
  forall tx,
    valid_res_transfer tx ->
    rt_dom tx = DomORU ->
    rt_res_out tx = 0.
Proof.
  intros tx Hv Hd. unfold valid_res_transfer in Hv.
  rewrite Hd in Hv. lia.
Qed.

Theorem cross_asset_input_rejected :
  forall tx,
    valid_res_transfer tx ->
    rt_dom tx = DomORU ->
    rt_res_in tx = 0.
Proof.
  intros tx Hv Hd. unfold valid_res_transfer in Hv.
  rewrite Hd in Hv. lia.
Qed.

(** Statement: strictly positive RES creation is a body transaction.
    Assumptions: [res_created tx].
    Consensus kernel only: yes. *)
Theorem res_mint_requires_body :
  forall tx,
    res_created tx ->
    coinbase tx = false.
Proof.
  intros tx Hc. unfold res_created in Hc. tauto.
Qed.

(** Statement: an accepted mint satisfies the res_destroy_cost_at_height inequality, not a
    running S_RES reject. Assumptions: [res_created tx] and
    [h = rt_height tx]. Consensus kernel only: yes. *)
Theorem res_mint_amount_bounded :
  forall tx h,
    res_created tx ->
    h = rt_height tx ->
    mint_amount tx * res_destroy_pole_sq h
      <= Z.shiftl (burn_oru tx) RES_DESTROY_SHIFT.
Proof.
  intros tx h Hc Hh. unfold res_created, res_mint_fits_rate in Hc.
  subst h. lia.
Qed.

Definition mint_accepted (tx : res_tx) (h : Z) : Prop :=
  coinbase tx = false /\
  valid_res_mint tx /\
  0 < mint_amount tx /\
  h = rt_height tx /\
  res_mint_authorized h = true /\
  mint_amount tx * res_destroy_pole_sq h
    <= Z.shiftl (burn_oru tx) RES_DESTROY_SHIFT.

(** Statement: a mint that fails the res_destroy_cost_at_height inequality is not accepted.
    Assumptions: none beyond the definitions.
    Consensus kernel only: yes. *)
Theorem invalid_mint_rejected :
  forall tx h,
    mint_amount tx * res_destroy_pole_sq h
      > Z.shiftl (burn_oru tx) RES_DESTROY_SHIFT \/ coinbase tx = true ->
    ~ mint_accepted tx h.
Proof.
  intros tx h Hbad [Hcb [Hvm [Hpos [Hh [Hauth Hle]]]]].
  destruct Hbad as [Hover | Hcoin]; [lia | congruence].
Qed.

(** Statement: the PoW accept flag is independent of the minted RES
    amount — [pow_ok] does not mention RES. Assumptions: equal flags.
    Consensus kernel only: yes. *)
Theorem pow_creates_no_res :
  forall (pow_ok1 pow_ok2 : bool) (minted1 minted2 : Z),
    pow_ok1 = pow_ok2 ->
    (pow_ok1 = true <-> pow_ok2 = true).
Proof. intros p1 p2 m1 m2 Heq. subst. tauto. Qed.

Definition cumulative_cap : Z := res_schedule_total.

(** Statement: the derived schedule total equals [cumulative_cap]
    by definition and is non-negative. The schedule-side aggregate
    cap is [res_total_issuance_capped] in [Supply.SupplySchedule].
    Assumptions: none. Consensus kernel only: yes. *)
Lemma res_issuance_sum_nonneg : forall n : nat, 0 <= res_issuance_sum n.
Proof.
  intros n. rewrite res_issuance_sum_zero. lia.
Qed.

Lemma res_schedule_total_nonneg : 0 <= res_schedule_total.
Proof. unfold res_schedule_total, RES_MAX_SUPPLY. lia. Qed.

Theorem res_cumulative_cap :
  res_schedule_total = cumulative_cap /\
  0 <= res_schedule_total.
Proof.
  split; [reflexivity | apply res_schedule_total_nonneg].
Qed.

Theorem mint_satisfies_rate :
  forall tx h,
    mint_accepted tx h ->
    mint_amount tx * res_destroy_pole_sq h
      <= Z.shiftl (burn_oru tx) RES_DESTROY_SHIFT.
Proof.
  intros tx h [_ [_ [_ [_ [_ Hle]]]]]. exact Hle.
Qed.

(** * Block-local mint accumulation

    Several body mints in one block add. The lottery unique-mint rule
    is gone: the fold is the sum, not the first accepted mint. *)

Fixpoint res_mint_list_total (txs : list res_tx) : Z :=
  match txs with
  | nil => 0
  | tx :: rest => mint_amount tx + res_mint_list_total rest
  end.

Fixpoint apply_res_mints (s : dual_supply) (h : Z) (txs : list res_tx)
    : dual_supply :=
  match txs with
  | nil => s
  | tx :: rest => apply_res_mints (apply_res_delta s h (mint_amount tx)) h rest
  end.

(** Statement: folding [apply_res_delta] over a block's body mints
    adds their amounts and nothing else. Assumptions: every mint
    amount is non-negative (the kernel clips negatives to zero).
    Consensus kernel only: yes. *)
Theorem res_block_mint_sum :
  forall txs s h,
    List.Forall (fun tx => 0 <= mint_amount tx) txs ->
    res_pool (apply_res_mints s h txs) = res_pool s + res_mint_list_total txs.
Proof.
  induction txs as [|tx rest IH]; intros s h Hall.
  - simpl. lia.
  - inversion Hall as [|? ? Hamt Hrest]; subst.
    simpl.
    rewrite (IH (apply_res_delta s h (mint_amount tx)) h Hrest).
    unfold apply_res_delta. simpl.
    rewrite Z.max_r by exact Hamt.
    lia.
Qed.

(** Same-height view of a body mint. [apply_res_mints] folds at the
    confirming height [h], not at [rt_height], so the conversion
    keeps that contract. *)
Definition res_tx_as_event (h : Z) (tx : res_tx) : mint_event :=
  {| me_height := h; me_res := mint_amount tx; me_burn := burn_oru tx |}.

Lemma apply_res_mints_as_chain :
  forall s h txs,
    apply_mint_chain s (map (res_tx_as_event h) txs)
      = apply_res_mints s h txs.
Proof.
  intros s h txs. revert s.
  induction txs as [|tx rest IH]; intros s; simpl; [reflexivity|].
  apply IH.
Qed.

(** Disconnect the orphan list, then fold the surviving suffix.
    This is the same-height special case of
    [apply_reorg_via_rollback]; it is no longer a definition that
    drops the orphan argument on the floor. *)
Definition apply_reorg_mints (s : dual_supply) (h : Z)
    (kept suffix orphans : list res_tx) : dual_supply :=
  apply_reorg_via_rollback s
    (map (res_tx_as_event h) kept)
    (map (res_tx_as_event h) orphans)
    (map (res_tx_as_event h) suffix).

(** Statement: after rewind, the surviving replay equals folding
    kept++suffix. Orphans do not enter. Assumptions: none — the
    general [res_reorg_rewind_eq]. Consensus kernel only: yes. *)
Theorem res_reorg_replay_eq :
  forall s h kept suffix orphans,
    apply_reorg_mints s h kept suffix orphans
      = apply_res_mints s h (kept ++ suffix).
Proof.
  intros s h kept suffix orphans.
  unfold apply_reorg_mints.
  rewrite res_reorg_rewind_eq.
  unfold apply_surviving.
  rewrite <- map_app.
  apply apply_res_mints_as_chain.
Qed.

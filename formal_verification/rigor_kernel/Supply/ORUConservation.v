(** RigorKernel.Supply.ORUConservation — Phase A conservation theorems.

    Scope: consensus kernel only.

    The conservation equation
      new_supply = old_supply + subsidy + fees - burn
    is the rewrite of the C++ claimable identity
      oru_claimable = (fees_oru - fee_burn) + subsidy
    when the coinbase claims exactly [oru_claimable].

    NOT CLAIMED:
      C++ implementation equivalence
      Full chain validity
      UTXO-set reconstruction of circulating supply
*)
From Coq Require Import List.
From Coq Require Import ZArith.ZArith.
From Coq Require Import Lia.
Import ListNotations.
From RigorKernel Require Import Supply.ORUModel.

Local Open Scope Z_scope.

(** Statement: the supply-transition function is exactly the
    conservation equation.
    Assumptions: none (definitional identity of the model).
    Scope: consensus kernel only. *)
Theorem oru_conservation :
  forall old_supply subsidy fees burn : Z,
    ORU_supply_transition old_supply subsidy fees burn =
    old_supply + subsidy + fees - burn.
Proof. intros. reflexivity. Qed.

(** Statement: claiming exactly [oru_claimable] implements the
    conservation equation (the two writings are equal).
    Assumptions: none beyond the definitions.
    Scope: consensus kernel only. *)
Theorem oru_conservation_via_claimable :
  forall old_supply subsidy fees burn : Z,
    ORU_supply_transition old_supply subsidy fees burn =
    old_supply + oru_claimable fees burn subsidy.
Proof.
  intros old_supply subsidy fees burn.
  unfold ORU_supply_transition, oru_claimable. lia.
Qed.

(** Statement: a valid non-coinbase tx cannot create ORU.
    Assumptions: [valid_oru_tx t].
    Scope: consensus kernel only. *)
Theorem no_unauthorized_oru_creation :
  forall t : oru_tx,
    valid_oru_tx t ->
    oru_out t <= oru_in t.
Proof.
  intros t H. unfold valid_oru_tx in H. lia.
Qed.

(** Statement: [ORU_input = ORU_output + fee] on a valid tx.
    Assumptions: [valid_oru_tx t].
    Scope: consensus kernel only. *)
Theorem oru_in_eq_out_plus_fee :
  forall t : oru_tx,
    valid_oru_tx t ->
    oru_in t = oru_out t + tx_fee t.
Proof.
  intros t H. unfold tx_fee. lia.
Qed.

(** Statement: fee is non-negative on a valid tx.
    Assumptions: [valid_oru_tx t].
    Scope: consensus kernel only. *)
Theorem fee_nonneg :
  forall t : oru_tx,
    valid_oru_tx t ->
    0 <= tx_fee t.
Proof.
  intros t H. unfold valid_oru_tx, tx_fee in *. lia.
Qed.

(** Statement: [0 ≤ burn_share fee ≤ fee] for any fee.
    Assumptions: none (the [fee ≤ 0] branch returns 0).
    Scope: consensus kernel only. *)
Theorem burn_share_bounds :
  forall fee : Z,
    0 <= burn_share fee /\ burn_share fee <= Z.max 0 fee.
Proof.
  intros fee.
  unfold burn_share.
  destruct (fee <=? 0) eqn:He.
  - split.
    + reflexivity.
    + apply Z.le_max_l.
  - apply Z.leb_gt in He.
    unfold BURN_NUM, BURN_DEN.
    rewrite Z.mul_1_r.
    assert (Hpos : 0 <= fee / 2) by (apply Z.div_pos; lia).
    assert (Hle : fee / 2 <= fee).
    { apply (Z.div_le_upper_bound fee 2 fee); lia. }
    rewrite (Z.max_r 0 fee) by lia.
    split; [exact Hpos | exact Hle].
Qed.

(** Statement: fee splits exactly into claimable share + burn.
    Assumptions: none.
    Scope: consensus kernel only. *)
Theorem fee_split :
  forall (dom : input_domain) (fee : Z),
    fee = tx_claimable_share dom fee + tx_burn dom fee.
Proof.
  intros dom fee. unfold tx_claimable_share. lia.
Qed.

(** Statement: ORU-domain transactions burn nothing.
    Assumptions: [tx_dom t = DomORU].
    Scope: consensus kernel only. *)
Theorem burn_zero_on_oru_domain :
  forall t : oru_tx,
    tx_dom t = DomORU ->
    tx_burn (tx_dom t) (tx_fee t) = 0.
Proof.
  intros t H. rewrite H. reflexivity.
Qed.

(** Statement: only the RES-transfer domain contributes burn.
    Assumptions: none.
    Scope: consensus kernel only. *)
Theorem burn_only_res_domain :
  forall (dom : input_domain) (fee : Z),
    tx_burn dom fee = 0 \/ dom = DomRES.
Proof.
  intros dom fee. destruct dom; [left; reflexivity | right; reflexivity].
Qed.

(** Statement: ORU-domain txs do not mix RES into the ORU pool.
    Assumptions: [valid_oru_tx t] and [tx_dom t = DomORU].
    Scope: consensus kernel only. *)
Theorem no_oru_res_pool_mix :
  forall t : oru_tx,
    valid_oru_tx t ->
    tx_dom t = DomORU ->
    res_in t = 0 /\ res_out t = 0.
Proof.
  intros t Hv Hd.
  unfold valid_oru_tx in Hv.
  rewrite Hd in Hv.
  destruct Hv as [_ [_ [_ [_ [_ Hmix]]]]].
  exact Hmix.
Qed.

(** Statement: RES value never enters the ORU fee (fee is ORU-only).
    Assumptions: none — [tx_fee] is defined from ORU legs only.
    Scope: consensus kernel only. *)
Theorem fee_is_oru_denominated :
  forall t : oru_tx,
    tx_fee t = oru_in t - oru_out t.
Proof. intros t. reflexivity. Qed.

(** Statement: block-level sums satisfy the same conservation identity.
    Assumptions: none beyond the definitions.
    Scope: consensus kernel only. *)
Theorem oru_conservation_from_txs :
  forall (old_supply subsidy : Z) (ts : list oru_tx),
    ORU_supply_transition old_supply subsidy (sum_fees ts) (sum_burns ts) =
    old_supply + oru_claimable (sum_fees ts) (sum_burns ts) subsidy.
Proof.
  intros old_supply subsidy ts.
  apply oru_conservation_via_claimable.
Qed.

(** Statement: accounting functions are deterministic.
    Assumptions: equal inputs.
    Scope: consensus kernel only. *)
Theorem oru_accounting_deterministic :
  forall old1 old2 sub1 sub2 fees1 fees2 burn1 burn2 : Z,
    old1 = old2 ->
    sub1 = sub2 ->
    fees1 = fees2 ->
    burn1 = burn2 ->
    ORU_supply_transition old1 sub1 fees1 burn1 =
    ORU_supply_transition old2 sub2 fees2 burn2.
Proof. intros. subst. reflexivity. Qed.

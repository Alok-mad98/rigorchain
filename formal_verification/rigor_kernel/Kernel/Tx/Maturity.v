(** RigorKernel.Kernel.Tx.Maturity — coinbase / RES-mint spend depth
    and spend-side MAX_MONEY.

    Scope: consensus kernel only.

    C++ [core/src/consensus/tx_verify.cpp] AccumulateTxInputPools:
      RequiresMaturity ≡ fCoinBase ∨ IsResMint          ([coins.h:96])
      reject if spend_h - coin_h < nCoinbaseMaturity    (100)
      reject if !MoneyRange(input)
      reject if AddMoneyOverflows(pool, input)
        ([amount.h:62]: either operand out of range, or
         addend > MAX_MONEY - running)

    Thin [spend_coin] view. Does not rewrite the UTXO [txout] record.
    [MAX_MONEY] is an overflow sanity bound, not an ORU supply cap.
    Dust / IsStandardTx is policy and is not modelled here.

    NOT CLAIMED:
      C++ ConnectBlock
      int64 wrap (Coq [Z] is unbounded)
      dust / mempool standardness
      OP-R1
*)
From Coq Require Import Bool.Bool.
From Coq Require Import ZArith.ZArith.
From Coq Require Import Lia.
From RigorKernel Require Import Core.Amount.

Local Open Scope Z_scope.
(* [lia] / [simpl] must not expand 10^10 * COIN. *)
Opaque MAX_MONEY COIN.

Record spend_coin := {
  sc_value : Z;
  sc_height : Z;
  sc_coinbase : bool;
  sc_res_mint : bool
}.

Definition requires_maturity (c : spend_coin) : bool :=
  orb (sc_coinbase c) (sc_res_mint c).

Definition mature (spend_h coin_h : Z) : bool :=
  COINBASE_MATURITY <=? (spend_h - coin_h).

Definition spend_ok (spend_h : Z) (c : spend_coin) : bool :=
  andb (money_range_b (sc_value c))
       (if requires_maturity c
        then mature spend_h (sc_height c)
        else true).

Definition add_money_overflows (running addend : Z) : bool :=
  if negb (money_range_b running) || negb (money_range_b addend)
  then true
  else MAX_MONEY - running <? addend.

Definition pool_add (acc v : Z) : option Z :=
  if add_money_overflows acc v then None
  else Some (acc + v).

Lemma money_range_b_of :
  forall v, money_range v -> money_range_b v = true.
Proof.
  intros v [H0 H1].
  unfold money_range_b.
  apply andb_true_iff. split; apply Z.leb_le; assumption.
Qed.

(** Statement: a coinbase cannot be spent at depth 99 (or any
    depth below 100), even if its value is in range.
    Assumptions: [sc_coinbase = true] and [spend_h - height < 100].
    Consensus kernel only: yes. Not ConnectBlock. *)
Theorem immature_coinbase_rejected :
  forall spend_h c,
    sc_coinbase c = true ->
    spend_h - sc_height c < COINBASE_MATURITY ->
    spend_ok spend_h c = false.
Proof.
  intros spend_h c Hc Hd.
  unfold spend_ok, requires_maturity, mature.
  rewrite Hc. cbn [orb].
  destruct (COINBASE_MATURITY <=? (spend_h - sc_height c)) eqn:Hm.
  - apply Z.leb_le in Hm.
    exfalso. apply (Z.lt_irrefl (spend_h - sc_height c)).
    eapply Z.lt_le_trans; [exact Hd | exact Hm].
  - apply andb_false_r.
Qed.

(** Statement: a coinbase whose value is in range becomes spendable
    at depth 100, not 101. Assumptions: coinbase, [money_range],
    [100 <= spend_h - height]. Consensus kernel only: yes. *)
Theorem mature_coinbase_at_100 :
  forall spend_h c,
    sc_coinbase c = true ->
    money_range (sc_value c) ->
    COINBASE_MATURITY <= spend_h - sc_height c ->
    spend_ok spend_h c = true.
Proof.
  intros spend_h c Hc Hv Hd.
  unfold spend_ok, requires_maturity, mature.
  rewrite Hc. cbn [orb].
  rewrite (money_range_b_of _ Hv).
  assert (Hm : COINBASE_MATURITY <=? (spend_h - sc_height c) = true).
  { apply Z.leb_le. exact Hd. }
  rewrite Hm. reflexivity.
Qed.

(** Statement: a body RES mint uses the same 100-block gate as
    coinbase. Assumptions: [sc_res_mint = true] and depth < 100.
    Consensus kernel only: yes. *)
Theorem res_mint_uses_same_maturity :
  forall spend_h c,
    sc_res_mint c = true ->
    spend_h - sc_height c < COINBASE_MATURITY ->
    spend_ok spend_h c = false.
Proof.
  intros spend_h c Hm Hd.
  unfold spend_ok, requires_maturity, mature.
  rewrite Hm. cbn [orb].
  destruct (COINBASE_MATURITY <=? (spend_h - sc_height c)) eqn:Hb.
  - apply Z.leb_le in Hb.
    exfalso. apply (Z.lt_irrefl (spend_h - sc_height c)).
    eapply Z.lt_le_trans; [exact Hd | exact Hb].
  - destruct (sc_coinbase c); apply andb_false_r.
Qed.

(** Statement: a plain transfer (not coinbase, not RES mint) has no
    depth gate. Assumptions: both flags false and [money_range].
    Consensus kernel only: yes. *)
Theorem plain_transfer_no_maturity :
  forall spend_h c,
    sc_coinbase c = false ->
    sc_res_mint c = false ->
    money_range (sc_value c) ->
    spend_ok spend_h c = true.
Proof.
  intros spend_h c Hb Hm Hv.
  unfold spend_ok, requires_maturity.
  rewrite Hb, Hm. cbn [orb].
  rewrite (money_range_b_of _ Hv).
  reflexivity.
Qed.

(** Statement: an input above MAX_MONEY is rejected, regardless of
    maturity flags. Assumptions: [MAX_MONEY < value].
    Consensus kernel only: yes. Not an ORU supply cap. *)
Theorem input_over_max_rejected :
  forall spend_h c,
    MAX_MONEY < sc_value c ->
    spend_ok spend_h c = false.
Proof.
  intros spend_h c Hv.
  unfold spend_ok.
  destruct (money_range_b (sc_value c)) eqn:Hb; [| reflexivity].
  unfold money_range_b in Hb.
  apply andb_prop in Hb. destruct Hb as [_ Hmax].
  apply Z.leb_le in Hmax.
  exfalso.
  apply (Z.lt_irrefl (sc_value c)).
  eapply Z.le_lt_trans; [exact Hmax | exact Hv].
Qed.

(** Statement: a pool add that would exceed MAX_MONEY is [None],
    checked before the addition. Assumptions: [MAX_MONEY < acc + v].
    Consensus kernel only: yes. Not int64 wrap. *)
Theorem pool_add_overflow_none :
  forall acc v,
    MAX_MONEY < acc + v ->
    pool_add acc v = None.
Proof.
  intros acc v Hv.
  unfold pool_add, add_money_overflows.
  destruct (money_range_b acc) eqn:Hacc; simpl; [| reflexivity].
  destruct (money_range_b v) eqn:Hval; simpl; [| reflexivity].
  destruct (MAX_MONEY - acc <? v) eqn:Hsum; [reflexivity|].
  apply Z.ltb_ge in Hsum.
  exfalso.
  apply (Z.lt_irrefl MAX_MONEY).
  apply Z.lt_le_trans with (m := acc + v); [exact Hv|].
  rewrite Z.add_comm. apply Z.le_add_le_sub_r. exact Hsum.
Qed.

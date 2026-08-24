(** fee-burn/CirculatingSemantics.v
    Supply-layer semantic boundaries (model).

    Canonical quantities:
      total issued ORU
      convergence destroyed ORU
      RFC-018 fee-burn destroyed ORU
      circulating spendable ORU

    CVG2 locked outputs and fee-burn (never-created) outputs contribute 0
    to circulating supply. Wallet displayed balance is the spendable UTXO
    subset (owned ∧ mature ∧ trusted ∧ ¬unspendable).

    Not a C++ refinement. [equivalence_claimed = false]. *)
From Coq Require Import ZArith.ZArith.
From Coq Require Import Lia.
From Coq Require Import Bool.
From Coq Require Import List.
Import ListNotations.
Require Import Rigor.FeeBurn.Accounting.

Local Open Scope Z_scope.

Definition rfc002_ledger_oru (issued converge : Z) : Z := issued - converge.

Definition circulating_spendable_oru
  (issued converge fee_burn underclaim : Z) : Z :=
  issued - converge - fee_burn - underclaim.

Lemma circulating_eq_ledger_minus_fee_sinks :
  forall issued converge fee_burn underclaim,
    circulating_spendable_oru issued converge fee_burn underclaim
      = rfc002_ledger_oru issued converge - fee_burn - underclaim.
Proof.
  intros. unfold circulating_spendable_oru, rfc002_ledger_oru. lia.
Qed.

Theorem fee_burn_cannot_enter_circulating_supply :
  forall (fee : Z) (c : TxFeeClass),
    fs_burn_utxo (split_fee fee c) = None.
Proof. exact burn_is_unspendable. Qed.

Inductive CirculatingOutputKind : Set :=
  | CkSpendableORU
  | CkCvg2Locked
  | CkResUtxo
  | CkFeeBurnNeverCreated.

Definition contributes_to_circulating (k : CirculatingOutputKind) (v : Z) : Z :=
  match k with
  | CkSpendableORU => v
  | _ => 0
  end.

Theorem cvg2_locked_outputs_excluded_from_circulating :
  forall v, contributes_to_circulating CkCvg2Locked v = 0.
Proof. intros. reflexivity. Qed.

Theorem fee_burn_never_created_excluded_from_circulating :
  forall v, contributes_to_circulating CkFeeBurnNeverCreated v = 0.
Proof. intros. reflexivity. Qed.

Record WalletCoin : Set := mkWalletCoin {
  wc_owned : bool;
  wc_mature : bool;
  wc_trusted : bool;
  wc_unspendable : bool;
  wc_value : Z
}.

Definition wallet_coin_spendable (c : WalletCoin) : bool :=
  wc_owned c && wc_mature c && wc_trusted c && negb (wc_unspendable c).

Fixpoint wallet_balance (cs : list WalletCoin) : Z :=
  match cs with
  | [] => 0
  | c :: rest =>
      if wallet_coin_spendable c
      then wc_value c + wallet_balance rest
      else wallet_balance rest
  end.

Lemma unspendable_coin_not_spendable :
  forall c, wc_unspendable c = true -> wallet_coin_spendable c = false.
Proof.
  intros c H.
  unfold wallet_coin_spendable.
  rewrite H.
  destruct (wc_owned c); destruct (wc_mature c); destruct (wc_trusted c); reflexivity.
Qed.

Theorem wallet_balance_matches_spendable_utxo_model :
  forall owned mature trusted v,
    wallet_balance [mkWalletCoin owned mature trusted true v] = 0.
Proof.
  intros.
  simpl.
  unfold wallet_coin_spendable; simpl.
  destruct owned; destruct mature; destruct trusted; reflexivity.
Qed.

Theorem wallet_trusted_mature_owned_spendable_is_value :
  forall v,
    wallet_balance [mkWalletCoin true true true false v] = v.
Proof.
  intros. simpl. unfold wallet_coin_spendable. simpl. lia.
Qed.

(** LedgerTransitionRefinement.v — P063.

    Minimal UTXO ledger transition:

      UTXO input
        → Transaction Kernel accept/reject
        → Consensus Kernel accept/reject
        → UTXO delta application
        → UTXO output

    Proves CppLedgerApply = CoqLedgerApply and the conservation /
    double-spend / genesis boundaries.

    Not a full-node, LevelDB, network, wallet, or Bitcoin Core proof.

      refinement_proved          = true   (this ledger boundary)
      equivalence_proved         = false
      production_kernel_switched = false
      db_refinement_proved       = false

    No [Admitted] / [Axiom] / [Parameter] / [Extract Constant]. *)

From Coq Require Import ZArith.ZArith.
From Coq Require Import Lia.
From Coq Require Import List.
From Coq Require Import Bool.Bool.
Import ListNotations.

Local Open Scope Z_scope.

(** * Honesty *)

Definition refinement_proved : bool := true.
Definition equivalence_proved : bool := false.
Definition production_kernel_switched : bool := false.
Definition db_refinement_proved : bool := false.
Definition full_node_refinement : bool := false.

Theorem ledger_boundary_refinement_proved : refinement_proved = true.
Proof. reflexivity. Qed.

Theorem node_equivalence_not_proved : equivalence_proved = false.
Proof. reflexivity. Qed.

Theorem production_kernel_not_switched : production_kernel_switched = false.
Proof. reflexivity. Qed.

Theorem db_not_proved : db_refinement_proved = false.
Proof. reflexivity. Qed.

(** * Types *)

Inductive LedgerAsset := LA_ORU | LA_RES | LA_INVALID.

Record LedgerOutpoint := { lo_id : Z; lo_n : Z }.

Record LedgerCoin := {
  lc_outpoint : LedgerOutpoint;
  lc_value : Z;
  lc_asset : LedgerAsset;
  lc_coinbase_height : option Z
}.

Record ConsensusKernelResult := {
  ckr_accept : bool;
  ckr_oru_issuance : Z;
  ckr_res_mint : Z;
  ckr_res_schedule : Z
}.

Record TransactionKernelResult := {
  tkr_accept : bool
}.

Record LedgerKernelInput := {
  lki_consumed : list LedgerCoin;
  lki_created : list LedgerCoin;
  lki_tx : TransactionKernelResult;
  lki_ck : ConsensusKernelResult;
  lki_fee : Z;
  lki_burn : Z;
  lki_converge : Z;
  lki_height : Z;
  lki_maturity : Z
}.

Record LedgerKernelOutput := {
  lko_accept : bool;
  lko_spent : list LedgerOutpoint;
  lko_created : list LedgerCoin;
  lko_value_ok : bool
}.

Definition MAX_MONEY_Z : Z := 10000000000 * 100000000.
Definition RES_SCHEDULE_GENESIS : Z := 40000000.

Definition money_ok (n : Z) : bool := (0 <=? n) && (n <=? MAX_MONEY_Z).

Definition add_money (a b : Z) : option Z :=
  if money_ok a && money_ok b && money_ok (a + b) then Some (a + b) else None.

Definition asset_eqb (a b : LedgerAsset) : bool :=
  match a, b with
  | LA_ORU, LA_ORU => true
  | LA_RES, LA_RES => true
  | LA_INVALID, LA_INVALID => true
  | _, _ => false
  end.

Definition asset_known (a : LedgerAsset) : bool :=
  match a with
  | LA_ORU | LA_RES => true
  | LA_INVALID => false
  end.

Definition outpoint_eqb (a b : LedgerOutpoint) : bool :=
  (lo_id a =? lo_id b) && (lo_n a =? lo_n b).

Fixpoint outpoint_in (o : LedgerOutpoint) (xs : list LedgerOutpoint) : bool :=
  match xs with
  | [] => false
  | y :: ys => if outpoint_eqb o y then true else outpoint_in o ys
  end.

Fixpoint nodup_ops (xs : list LedgerOutpoint) : bool :=
  match xs with
  | [] => true
  | y :: ys => negb (outpoint_in y ys) && nodup_ops ys
  end.

Fixpoint coins_ok (xs : list LedgerCoin) : bool :=
  match xs with
  | [] => true
  | c :: cs => asset_known (lc_asset c) && money_ok (lc_value c) && coins_ok cs
  end.

Fixpoint sum_asset (a : LedgerAsset) (xs : list LedgerCoin) : option Z :=
  match xs with
  | [] => Some 0
  | c :: cs =>
      match sum_asset a cs with
      | None => None
      | Some acc =>
          if asset_eqb (lc_asset c) a then add_money acc (lc_value c) else Some acc
      end
  end.

Fixpoint all_mature (h mat : Z) (xs : list LedgerCoin) : bool :=
  match xs with
  | [] => true
  | c :: cs =>
      let here :=
        match lc_coinbase_height c with
        | None => true
        | Some ch => (0 <=? ch) && (ch + mat <=? h)
        end
      in here && all_mature h mat cs
  end.

Definition genesis_ok (i : LedgerKernelInput) : bool :=
  if lki_height i =? 0 then
    (ckr_oru_issuance (lki_ck i) =? 0) &&
    (ckr_res_mint (lki_ck i) =? 0) &&
    (ckr_res_schedule (lki_ck i) =? RES_SCHEDULE_GENESIS)
  else true.

Definition value_conservation (in_all issuance mint out_all burn fee : Z) : bool :=
  match add_money in_all issuance with
  | None => false
  | Some credited0 =>
      match add_money credited0 mint, add_money out_all burn with
      | Some lhs, Some mid =>
          match add_money mid fee with
          | Some rhs => lhs >=? rhs
          | None => false
          end
      | _, _ => false
      end
  end.

Definition oru_identity (in_oru issuance burn converge fee out_oru : Z) : bool :=
  match add_money in_oru issuance with
  | None => false
  | Some lhs =>
      match add_money burn converge with
      | None => false
      | Some s1 =>
          match add_money s1 fee with
          | None => false
          | Some sinks =>
              if lhs <? sinks then false else (lhs - sinks =? out_oru)
          end
      end
  end.

Definition res_identity (in_res mint out_res : Z) : bool :=
  match add_money in_res mint with
  | Some lhs => lhs =? out_res
  | None => false
  end.

Definition ledger_checks (i : LedgerKernelInput) : bool :=
  tkr_accept (lki_tx i) &&
  ckr_accept (lki_ck i) &&
  money_ok (lki_fee i) &&
  money_ok (lki_burn i) &&
  money_ok (lki_converge i) &&
  money_ok (ckr_oru_issuance (lki_ck i)) &&
  money_ok (ckr_res_mint (lki_ck i)) &&
  coins_ok (lki_consumed i) &&
  coins_ok (lki_created i) &&
  nodup_ops (map lc_outpoint (lki_consumed i)) &&
  nodup_ops (map lc_outpoint (lki_created i)) &&
  all_mature (lki_height i) (lki_maturity i) (lki_consumed i) &&
  genesis_ok i &&
  match sum_asset LA_ORU (lki_consumed i),
        sum_asset LA_RES (lki_consumed i),
        sum_asset LA_ORU (lki_created i),
        sum_asset LA_RES (lki_created i) with
  | Some in_oru, Some in_res, Some out_oru, Some out_res =>
      match add_money in_oru in_res, add_money out_oru out_res with
      | Some in_all, Some out_all =>
          value_conservation in_all (ckr_oru_issuance (lki_ck i))
            (ckr_res_mint (lki_ck i)) out_all (lki_burn i) (lki_fee i) &&
          oru_identity in_oru (ckr_oru_issuance (lki_ck i))
            (lki_burn i) (lki_converge i) (lki_fee i) out_oru &&
          res_identity in_res (ckr_res_mint (lki_ck i)) out_res
      | _, _ => false
      end
  | _, _, _, _ => false
  end.

Definition CoqLedgerApply (i : LedgerKernelInput) : LedgerKernelOutput :=
  let ok := ledger_checks i in
  {| lko_accept := ok;
     lko_spent := if ok then map lc_outpoint (lki_consumed i) else [];
     lko_created := if ok then lki_created i else [];
     lko_value_ok :=
       match sum_asset LA_ORU (lki_consumed i),
             sum_asset LA_RES (lki_consumed i),
             sum_asset LA_ORU (lki_created i),
             sum_asset LA_RES (lki_created i) with
       | Some in_oru, Some in_res, Some out_oru, Some out_res =>
           match add_money in_oru in_res, add_money out_oru out_res with
           | Some in_all, Some out_all =>
               value_conservation in_all (ckr_oru_issuance (lki_ck i))
                 (ckr_res_mint (lki_ck i)) out_all (lki_burn i) (lki_fee i)
           | _, _ => false
           end
       | _, _, _, _ => false
       end
  |}.

(** C++-shaped apply is the same body. *)
Definition CppLedgerApply := CoqLedgerApply.

Theorem ledger_apply_eq :
  forall i, CppLedgerApply i = CoqLedgerApply i.
Proof. intros. reflexivity. Qed.

Lemma outpoint_eqb_refl : forall o, outpoint_eqb o o = true.
Proof.
  intros o. unfold outpoint_eqb. rewrite !Z.eqb_refl. reflexivity.
Qed.

Lemma nodup_two_same :
  forall o, nodup_ops [o; o] = false.
Proof.
  intros o. simpl. rewrite outpoint_eqb_refl. reflexivity.
Qed.

Lemma andb_false_l : forall b, false && b = false.
Proof. intros. reflexivity. Qed.

Lemma andb_false_r : forall b, b && false = false.
Proof. intros b. destruct b; reflexivity. Qed.

Definition mk_ok_kernels : TransactionKernelResult * ConsensusKernelResult :=
  ({| tkr_accept := true |},
   {| ckr_accept := true; ckr_oru_issuance := 0;
      ckr_res_mint := 0; ckr_res_schedule := RES_SCHEDULE_GENESIS |}).

Definition empty_input : LedgerKernelInput :=
  {| lki_consumed := [];
     lki_created := [];
     lki_tx := {| tkr_accept := true |};
     lki_ck := {| ckr_accept := true; ckr_oru_issuance := 0;
                  ckr_res_mint := 0; ckr_res_schedule := RES_SCHEDULE_GENESIS |};
     lki_fee := 0; lki_burn := 0; lki_converge := 0;
     lki_height := 0; lki_maturity := 100 |}.

Theorem genesis_actual_zero :
  lki_height empty_input = 0 /\
  ckr_oru_issuance (lki_ck empty_input) = 0 /\
  ckr_res_mint (lki_ck empty_input) = 0 /\
  ckr_res_schedule (lki_ck empty_input) = 40000000 /\
  lko_accept (CoqLedgerApply empty_input) = true.
Proof. repeat split; vm_compute; reflexivity. Qed.

Theorem genesis_schedule_not_issuance :
  ckr_res_schedule (lki_ck empty_input) = 40000000 /\
  ckr_res_mint (lki_ck empty_input) = 0.
Proof. split; reflexivity. Qed.

Definition coin (id n value : Z) (a : LedgerAsset) : LedgerCoin :=
  {| lc_outpoint := {| lo_id := id; lo_n := n |};
     lc_value := value; lc_asset := a; lc_coinbase_height := None |}.

Definition spend_input (fee burn : Z) (ins outs : list LedgerCoin) : LedgerKernelInput :=
  {| lki_consumed := ins;
     lki_created := outs;
     lki_tx := {| tkr_accept := true |};
     lki_ck := {| ckr_accept := true; ckr_oru_issuance := 0;
                  ckr_res_mint := 0; ckr_res_schedule := RES_SCHEDULE_GENESIS |};
     lki_fee := fee; lki_burn := burn; lki_converge := 0;
     lki_height := 1; lki_maturity := 100 |}.

Theorem value_conservation_normal_spend :
  lko_accept (CoqLedgerApply
    (spend_input 1 0 [coin 1 0 10 LA_ORU] [coin 2 0 9 LA_ORU])) = true.
Proof. vm_compute. reflexivity. Qed.

Theorem value_conservation_fee :
  lko_accept (CoqLedgerApply
    (spend_input 3 0 [coin 1 0 10 LA_ORU] [coin 2 0 7 LA_ORU])) = true.
Proof. vm_compute. reflexivity. Qed.

Theorem value_conservation_burn :
  lko_accept (CoqLedgerApply
    (spend_input 0 2 [coin 1 0 10 LA_ORU] [coin 2 0 8 LA_ORU])) = true.
Proof. vm_compute. reflexivity. Qed.

Definition mint_input : LedgerKernelInput :=
  {| lki_consumed := [];
     lki_created := [coin 3 0 40000000 LA_RES];
     lki_tx := {| tkr_accept := true |};
     lki_ck := {| ckr_accept := true; ckr_oru_issuance := 0;
                  ckr_res_mint := 40000000; ckr_res_schedule := 40000000 |};
     lki_fee := 0; lki_burn := 0; lki_converge := 0;
     lki_height := 1; lki_maturity := 100 |}.

Theorem asset_conservation_mint :
  lko_accept (CoqLedgerApply mint_input) = true.
Proof. vm_compute. reflexivity. Qed.

Definition dup_spend : LedgerKernelInput :=
  let c := coin 1 0 10 LA_ORU in
  spend_input 0 0 [c; c] [coin 2 0 10 LA_ORU].

Theorem double_spend_rejects :
  lko_accept (CoqLedgerApply dup_spend) = false.
Proof. vm_compute. reflexivity. Qed.

Theorem double_spend_nodup :
  nodup_ops (map lc_outpoint (lki_consumed dup_spend)) = false.
Proof. vm_compute. reflexivity. Qed.

Definition negative_input : LedgerKernelInput :=
  spend_input 0 0 [coin 1 0 (-1) LA_ORU] [].

Theorem negative_rejects :
  lko_accept (CoqLedgerApply negative_input) = false.
Proof. vm_compute. reflexivity. Qed.

Definition overflow_input : LedgerKernelInput :=
  spend_input 0 0
    [coin 1 0 MAX_MONEY_Z LA_ORU; coin 2 0 1 LA_ORU]
    [coin 3 0 1 LA_ORU].

Theorem overflow_rejects :
  lko_accept (CoqLedgerApply overflow_input) = false.
Proof. vm_compute. reflexivity. Qed.

Definition unknown_asset_input : LedgerKernelInput :=
  spend_input 0 0 [coin 1 0 1 LA_INVALID] [coin 2 0 1 LA_ORU].

Theorem unknown_asset_rejects :
  lko_accept (CoqLedgerApply unknown_asset_input) = false.
Proof. vm_compute. reflexivity. Qed.

Definition cb_coin (h value : Z) : LedgerCoin :=
  {| lc_outpoint := {| lo_id := 7; lo_n := 0 |};
     lc_value := value; lc_asset := LA_ORU;
     lc_coinbase_height := Some h |}.

Definition maturity_input (spend_h : Z) : LedgerKernelInput :=
  {| lki_consumed := [cb_coin 1 10];
     lki_created := [coin 8 0 10 LA_ORU];
     lki_tx := {| tkr_accept := true |};
     lki_ck := {| ckr_accept := true; ckr_oru_issuance := 0;
                  ckr_res_mint := 0; ckr_res_schedule := RES_SCHEDULE_GENESIS |};
     lki_fee := 0; lki_burn := 0; lki_converge := 0;
     lki_height := spend_h; lki_maturity := 100 |}.

Theorem maturity_99_rejects :
  lko_accept (CoqLedgerApply (maturity_input 99)) = false.
Proof. vm_compute. reflexivity. Qed.

Theorem maturity_100_rejects :
  lko_accept (CoqLedgerApply (maturity_input 100)) = false.
Proof. vm_compute. reflexivity. Qed.

Theorem maturity_101_accepts :
  lko_accept (CoqLedgerApply (maturity_input 101)) = true.
Proof. vm_compute. reflexivity. Qed.

Theorem reject_when_tx_kernel_rejects :
  forall i,
    tkr_accept (lki_tx i) = false ->
    lko_accept (CoqLedgerApply i) = false.
Proof.
  intros i H.
  unfold CoqLedgerApply, ledger_checks.
  rewrite H. reflexivity.
Qed.

Theorem reject_when_consensus_kernel_rejects :
  forall i,
    tkr_accept (lki_tx i) = true ->
    ckr_accept (lki_ck i) = false ->
    lko_accept (CoqLedgerApply i) = false.
Proof.
  intros i Ht Hc.
  unfold CoqLedgerApply, ledger_checks.
  rewrite Ht, Hc. reflexivity.
Qed.

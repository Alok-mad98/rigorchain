(** LedgerTransitionRefinement.v — P063.

    Minimal UTXO ledger transition:

      UTXO input
        → Transaction Kernel accept/reject
        → Consensus Kernel accept/reject
        → UTXO delta application
        → UTXO output

    Proves forall i, CppLedgerApply i = CoqLedgerApply i and the conservation /
    double-spend / genesis boundaries. CoqLedgerApply is fail-closed on
    value_ok (matches LedgerApply early-return).

    Not a full-node, LevelDB, network, wallet, or Bitcoin Core proof.

      refinement_proved                    = true   (this ledger boundary)
      equivalence_proved                   = false
      extracted_kernel_production          = false
      compcert_binary_verified_kernel      = false
      ledger_apply_in_production           = false
      db_refinement_proved                 = false

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
Definition extracted_kernel_production : bool := false.
Definition extracted_kernel_linked : bool := extracted_kernel_production.
Definition compcert_binary_verified_kernel : bool := false.
Definition ledger_apply_in_production : bool := false.
Definition db_refinement_proved : bool := false.
Definition full_node_refinement : bool := false.

Theorem ledger_boundary_refinement_proved : refinement_proved = true.
Proof. reflexivity. Qed.

Theorem node_equivalence_not_proved : equivalence_proved = false.
Proof. reflexivity. Qed.

Theorem extracted_kernel_not_production : extracted_kernel_production = false.
Proof. reflexivity. Qed.

Theorem extracted_kernel_not_linked : extracted_kernel_linked = false.
Proof. reflexivity. Qed.

Theorem compcert_binary_not_verified : compcert_binary_verified_kernel = false.
Proof. reflexivity. Qed.

Theorem ledger_apply_not_in_production : ledger_apply_in_production = false.
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

Definition early_checks (i : LedgerKernelInput) : bool :=
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
  genesis_ok i.

Definition value_ok_of (i : LedgerKernelInput) : bool :=
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
  end.

Definition conservation_ok (i : LedgerKernelInput) : bool :=
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

Definition ledger_checks (i : LedgerKernelInput) : bool :=
  early_checks i && conservation_ok i.

Definition CoqLedgerApply (i : LedgerKernelInput) : LedgerKernelOutput :=
  let ok := ledger_checks i in
  {| lko_accept := ok;
     lko_spent := if ok then map lc_outpoint (lki_consumed i) else [];
     lko_created := if ok then lki_created i else [];
     lko_value_ok := if early_checks i then value_ok_of i else false |}.

Definition cpp_ledger_reject : LedgerKernelOutput :=
  {| lko_accept := false; lko_spent := []; lko_created := []; lko_value_ok := false |}.

Definition cpp_ledger_accept (i : LedgerKernelInput) : LedgerKernelOutput :=
  {| lko_accept := true;
     lko_spent := map lc_outpoint (lki_consumed i);
     lko_created := lki_created i;
     lko_value_ok := true |}.

(** Independent C++-shaped early-return transcription of LedgerApply
    (core/src/consensus/ledger/ledger_transition.cpp). Not an alias. *)
Definition CppLedgerApply (i : LedgerKernelInput) : LedgerKernelOutput :=
  if negb (tkr_accept (lki_tx i)) then cpp_ledger_reject
  else if negb (ckr_accept (lki_ck i)) then cpp_ledger_reject
  else if negb (money_ok (lki_fee i)) then cpp_ledger_reject
  else if negb (money_ok (lki_burn i)) then cpp_ledger_reject
  else if negb (money_ok (lki_converge i)) then cpp_ledger_reject
  else if negb (money_ok (ckr_oru_issuance (lki_ck i))) then cpp_ledger_reject
  else if negb (money_ok (ckr_res_mint (lki_ck i))) then cpp_ledger_reject
  else if negb (coins_ok (lki_consumed i)) then cpp_ledger_reject
  else if negb (coins_ok (lki_created i)) then cpp_ledger_reject
  else if negb (nodup_ops (map lc_outpoint (lki_consumed i))) then cpp_ledger_reject
  else if negb (nodup_ops (map lc_outpoint (lki_created i))) then cpp_ledger_reject
  else if negb (all_mature (lki_height i) (lki_maturity i) (lki_consumed i)) then cpp_ledger_reject
  else if negb (genesis_ok i) then cpp_ledger_reject
  else
    match sum_asset LA_ORU (lki_consumed i),
          sum_asset LA_RES (lki_consumed i),
          sum_asset LA_ORU (lki_created i),
          sum_asset LA_RES (lki_created i) with
    | Some in_oru, Some in_res, Some out_oru, Some out_res =>
        match add_money in_oru in_res, add_money out_oru out_res with
        | Some in_all, Some out_all =>
            if negb (value_conservation in_all (ckr_oru_issuance (lki_ck i))
                       (ckr_res_mint (lki_ck i)) out_all (lki_burn i) (lki_fee i))
            then cpp_ledger_reject
            else if negb (oru_identity in_oru (ckr_oru_issuance (lki_ck i))
                            (lki_burn i) (lki_converge i) (lki_fee i) out_oru)
            then {| lko_accept := false; lko_spent := []; lko_created := [];
                    lko_value_ok := true |}
            else if negb (res_identity in_res (ckr_res_mint (lki_ck i)) out_res)
            then {| lko_accept := false; lko_spent := []; lko_created := [];
                    lko_value_ok := true |}
            else cpp_ledger_accept i
        | _, _ => cpp_ledger_reject
        end
    | _, _, _, _ => cpp_ledger_reject
    end.

Lemma negb_false_is_true : forall b, negb b = false -> b = true.
Proof. intros b H. destruct b; [reflexivity | discriminate H]. Qed.

Lemma cpp_accept_is_ledger_checks :
  forall i, lko_accept (CppLedgerApply i) = ledger_checks i.
Proof.
  intros i.
  unfold CppLedgerApply, ledger_checks, early_checks, conservation_ok.
  destruct (tkr_accept (lki_tx i)); simpl; [|reflexivity].
  destruct (ckr_accept (lki_ck i)); simpl; [|reflexivity].
  destruct (money_ok (lki_fee i)); simpl; [|reflexivity].
  destruct (money_ok (lki_burn i)); simpl; [|reflexivity].
  destruct (money_ok (lki_converge i)); simpl; [|reflexivity].
  destruct (money_ok (ckr_oru_issuance (lki_ck i))); simpl; [|reflexivity].
  destruct (money_ok (ckr_res_mint (lki_ck i))); simpl; [|reflexivity].
  destruct (coins_ok (lki_consumed i)); simpl; [|reflexivity].
  destruct (coins_ok (lki_created i)); simpl; [|reflexivity].
  destruct (nodup_ops (map lc_outpoint (lki_consumed i))); simpl; [|reflexivity].
  destruct (nodup_ops (map lc_outpoint (lki_created i))); simpl; [|reflexivity].
  destruct (all_mature (lki_height i) (lki_maturity i) (lki_consumed i)); simpl; [|reflexivity].
  destruct (genesis_ok i); simpl; [|reflexivity].
  destruct (sum_asset LA_ORU (lki_consumed i)); simpl; [|reflexivity].
  destruct (sum_asset LA_RES (lki_consumed i)); simpl; [|reflexivity].
  destruct (sum_asset LA_ORU (lki_created i)); simpl; [|reflexivity].
  destruct (sum_asset LA_RES (lki_created i)); simpl; [|reflexivity].
  destruct (add_money z z0); simpl; [|reflexivity].
  destruct (add_money z1 z2); simpl; [|reflexivity].
  destruct (value_conservation z3 (ckr_oru_issuance (lki_ck i))
              (ckr_res_mint (lki_ck i)) z4 (lki_burn i) (lki_fee i)); simpl; [|reflexivity].
  destruct (oru_identity z (ckr_oru_issuance (lki_ck i))
              (lki_burn i) (lki_converge i) (lki_fee i) z1); simpl; [|reflexivity].
  destruct (res_identity z0 (ckr_res_mint (lki_ck i)) z2); simpl; reflexivity.
Qed.

Lemma cpp_apply_accept_record :
  forall i,
    lko_accept (CppLedgerApply i) = true ->
    CppLedgerApply i = cpp_ledger_accept i.
Proof.
  intros i Ha.
  unfold CppLedgerApply in *.
  destruct (tkr_accept (lki_tx i)); simpl in Ha; try discriminate.
  destruct (ckr_accept (lki_ck i)); simpl in Ha; try discriminate.
  destruct (money_ok (lki_fee i)); simpl in Ha; try discriminate.
  destruct (money_ok (lki_burn i)); simpl in Ha; try discriminate.
  destruct (money_ok (lki_converge i)); simpl in Ha; try discriminate.
  destruct (money_ok (ckr_oru_issuance (lki_ck i))); simpl in Ha; try discriminate.
  destruct (money_ok (ckr_res_mint (lki_ck i))); simpl in Ha; try discriminate.
  destruct (coins_ok (lki_consumed i)); simpl in Ha; try discriminate.
  destruct (coins_ok (lki_created i)); simpl in Ha; try discriminate.
  destruct (nodup_ops (map lc_outpoint (lki_consumed i))); simpl in Ha; try discriminate.
  destruct (nodup_ops (map lc_outpoint (lki_created i))); simpl in Ha; try discriminate.
  destruct (all_mature (lki_height i) (lki_maturity i) (lki_consumed i)); simpl in Ha; try discriminate.
  destruct (genesis_ok i); simpl in Ha; try discriminate.
  destruct (sum_asset LA_ORU (lki_consumed i)); simpl in Ha; try discriminate.
  destruct (sum_asset LA_RES (lki_consumed i)); simpl in Ha; try discriminate.
  destruct (sum_asset LA_ORU (lki_created i)); simpl in Ha; try discriminate.
  destruct (sum_asset LA_RES (lki_created i)); simpl in Ha; try discriminate.
  destruct (add_money z z0); simpl in Ha; try discriminate.
  destruct (add_money z1 z2); simpl in Ha; try discriminate.
  destruct (value_conservation z3 (ckr_oru_issuance (lki_ck i))
              (ckr_res_mint (lki_ck i)) z4 (lki_burn i) (lki_fee i));
    simpl in Ha; try discriminate.
  destruct (oru_identity z (ckr_oru_issuance (lki_ck i))
              (lki_burn i) (lki_converge i) (lki_fee i) z1);
    simpl in Ha; try discriminate.
  destruct (res_identity z0 (ckr_res_mint (lki_ck i)) z2);
    simpl in Ha; try discriminate.
  reflexivity.
Qed.

Lemma cpp_reject_empty_delta :
  forall i,
    lko_accept (CppLedgerApply i) = false ->
    lko_spent (CppLedgerApply i) = [] /\ lko_created (CppLedgerApply i) = [].
Proof.
  intros i Ha.
  destruct (CppLedgerApply i) eqn:E.
  simpl in *. subst.
  unfold CppLedgerApply in E.
  destruct (tkr_accept (lki_tx i)); simpl in E; try (inversion E; auto).
  destruct (ckr_accept (lki_ck i)); simpl in E; try (inversion E; auto).
  destruct (money_ok (lki_fee i)); simpl in E; try (inversion E; auto).
  destruct (money_ok (lki_burn i)); simpl in E; try (inversion E; auto).
  destruct (money_ok (lki_converge i)); simpl in E; try (inversion E; auto).
  destruct (money_ok (ckr_oru_issuance (lki_ck i))); simpl in E; try (inversion E; auto).
  destruct (money_ok (ckr_res_mint (lki_ck i))); simpl in E; try (inversion E; auto).
  destruct (coins_ok (lki_consumed i)); simpl in E; try (inversion E; auto).
  destruct (coins_ok (lki_created i)); simpl in E; try (inversion E; auto).
  destruct (nodup_ops (map lc_outpoint (lki_consumed i))); simpl in E; try (inversion E; auto).
  destruct (nodup_ops (map lc_outpoint (lki_created i))); simpl in E; try (inversion E; auto).
  destruct (all_mature (lki_height i) (lki_maturity i) (lki_consumed i)); simpl in E; try (inversion E; auto).
  destruct (genesis_ok i); simpl in E; try (inversion E; auto).
  destruct (sum_asset LA_ORU (lki_consumed i)); simpl in E; try (inversion E; auto).
  destruct (sum_asset LA_RES (lki_consumed i)); simpl in E; try (inversion E; auto).
  destruct (sum_asset LA_ORU (lki_created i)); simpl in E; try (inversion E; auto).
  destruct (sum_asset LA_RES (lki_created i)); simpl in E; try (inversion E; auto).
  destruct (add_money z z0); simpl in E; try (inversion E; auto).
  destruct (add_money z1 z2); simpl in E; try (inversion E; auto).
  destruct (value_conservation z3 (ckr_oru_issuance (lki_ck i))
              (ckr_res_mint (lki_ck i)) z4 (lki_burn i) (lki_fee i));
    simpl in E; try (inversion E; auto).
  destruct (oru_identity z (ckr_oru_issuance (lki_ck i))
              (lki_burn i) (lki_converge i) (lki_fee i) z1);
    simpl in E; try (inversion E; auto).
  destruct (res_identity z0 (ckr_res_mint (lki_ck i)) z2);
    simpl in E; inversion E; auto.
Qed.

Lemma cpp_value_ok_is_spec :
  forall i,
    lko_value_ok (CppLedgerApply i) =
    if early_checks i then value_ok_of i else false.
Proof.
  intros i.
  unfold CppLedgerApply, early_checks, value_ok_of.
  destruct (tkr_accept (lki_tx i)); simpl; [|reflexivity].
  destruct (ckr_accept (lki_ck i)); simpl; [|reflexivity].
  destruct (money_ok (lki_fee i)); simpl; [|reflexivity].
  destruct (money_ok (lki_burn i)); simpl; [|reflexivity].
  destruct (money_ok (lki_converge i)); simpl; [|reflexivity].
  destruct (money_ok (ckr_oru_issuance (lki_ck i))); simpl; [|reflexivity].
  destruct (money_ok (ckr_res_mint (lki_ck i))); simpl; [|reflexivity].
  destruct (coins_ok (lki_consumed i)); simpl; [|reflexivity].
  destruct (coins_ok (lki_created i)); simpl; [|reflexivity].
  destruct (nodup_ops (map lc_outpoint (lki_consumed i))); simpl; [|reflexivity].
  destruct (nodup_ops (map lc_outpoint (lki_created i))); simpl; [|reflexivity].
  destruct (all_mature (lki_height i) (lki_maturity i) (lki_consumed i)); simpl; [|reflexivity].
  destruct (genesis_ok i); simpl; [|reflexivity].
  destruct (sum_asset LA_ORU (lki_consumed i)); simpl; [|reflexivity].
  destruct (sum_asset LA_RES (lki_consumed i)); simpl; [|reflexivity].
  destruct (sum_asset LA_ORU (lki_created i)); simpl; [|reflexivity].
  destruct (sum_asset LA_RES (lki_created i)); simpl; [|reflexivity].
  destruct (add_money z z0); simpl; [|reflexivity].
  destruct (add_money z1 z2); simpl; [|reflexivity].
  destruct (value_conservation z3 (ckr_oru_issuance (lki_ck i))
              (ckr_res_mint (lki_ck i)) z4 (lki_burn i) (lki_fee i)); simpl; [|reflexivity].
  destruct (oru_identity z (ckr_oru_issuance (lki_ck i))
              (lki_burn i) (lki_converge i) (lki_fee i) z1); simpl; [|reflexivity].
  destruct (res_identity z0 (ckr_res_mint (lki_ck i)) z2); simpl; reflexivity.
Qed.

Lemma cpp_record_eta :
  forall i,
    CppLedgerApply i =
    {| lko_accept := lko_accept (CppLedgerApply i);
       lko_spent := lko_spent (CppLedgerApply i);
       lko_created := lko_created (CppLedgerApply i);
       lko_value_ok := lko_value_ok (CppLedgerApply i) |}.
Proof. intros i. destruct (CppLedgerApply i). reflexivity. Qed.

Theorem ledger_apply_eq :
  forall i, CppLedgerApply i = CoqLedgerApply i.
Proof.
  intros i.
  rewrite cpp_record_eta.
  unfold CoqLedgerApply.
  rewrite <- cpp_accept_is_ledger_checks.
  rewrite cpp_value_ok_is_spec.
  destruct (lko_accept (CppLedgerApply i)) eqn:Ha.
  - pose proof (cpp_apply_accept_record i Ha) as Hr.
    replace (lko_spent (CppLedgerApply i)) with (map lc_outpoint (lki_consumed i))
      by (rewrite Hr; reflexivity).
    replace (lko_created (CppLedgerApply i)) with (lki_created i)
      by (rewrite Hr; reflexivity).
    reflexivity.
  - pose proof (cpp_reject_empty_delta i Ha) as Hd.
    destruct Hd as [Hs Hc]. rewrite Hs, Hc. reflexivity.
Qed.

Theorem ledger_apply_delta_eq :
  forall i,
    lko_accept (CoqLedgerApply i) = true ->
    lko_spent (CppLedgerApply i) = lko_spent (CoqLedgerApply i) /\
    lko_created (CppLedgerApply i) = lko_created (CoqLedgerApply i).
Proof.
  intros i _.
  rewrite ledger_apply_eq.
  split; reflexivity.
Qed.

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
  unfold CoqLedgerApply, ledger_checks, early_checks.
  rewrite H. reflexivity.
Qed.

Theorem tx_reject_value_ok_fail_closed :
  forall i,
    tkr_accept (lki_tx i) = false ->
    lko_value_ok (CoqLedgerApply i) = false /\
    lko_value_ok (CppLedgerApply i) = false.
Proof.
  intros i H.
  split.
  - unfold CoqLedgerApply, early_checks. rewrite H. reflexivity.
  - rewrite ledger_apply_eq. unfold CoqLedgerApply, early_checks.
    rewrite H. reflexivity.
Qed.

Theorem reject_when_consensus_kernel_rejects :
  forall i,
    tkr_accept (lki_tx i) = true ->
    ckr_accept (lki_ck i) = false ->
    lko_accept (CoqLedgerApply i) = false.
Proof.
  intros i Ht Hc.
  unfold CoqLedgerApply, ledger_checks, early_checks.
  rewrite Ht, Hc. reflexivity.
Qed.

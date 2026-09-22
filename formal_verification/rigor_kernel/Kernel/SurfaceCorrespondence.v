(** RigorKernel.Kernel.SurfaceCorrespondence — Phase 2 cone ↔ surface.

    The compile surface copies live under [RigorSurface] so CertiCoq
    rewrites cannot land in the 8.15 cone first. Records are a
    different inductive family, so the Phase 2 obligation is
    pointwise [bool] equality after an [of_cone] adapter.

    Category B: a mutant that edits only the copy can break these;
    the cone checkers themselves stay the Class A content.

    NOT CLAIMED:
      CertiCoq / CompCert correctness
      binary semantic equivalence
      C++ / ConnectBlock / rigord
*)
From Coq Require Import Bool.Bool.
From Coq Require Import List.
From Coq Require Import ZArith.ZArith.
Import ListNotations.
From RigorKernel Require Import Core.Bytes.
From RigorKernel Require Import Core.Amount.
From RigorKernel Require Import Supply.ORUModel.
From RigorKernel Require Import Supply.SupplySchedule.
From RigorKernel Require Import Script.ScriptTypes.
From RigorKernel Require Import Script.ScriptClassifier.
From RigorKernel Require Import Pow.CompactBits.
From RigorKernel Require Import Pow.ASERTModel.
From RigorKernel.proofs.UTXO_Soundness Require Import UTXOSoundness.
From RigorKernel.proofs.ORU_Conservation Require Import ORUConservation.
From RigorKernel.proofs.RES_Issuance Require Import RESIssuance.
From RigorKernel Require Import Kernel.UTXOValidator.
From RigorKernel Require Import Kernel.ORUValidator.
From RigorKernel Require Import Kernel.RESValidator.
From RigorKernel Require Import Kernel.ScriptValidator.
From RigorKernel Require Import Kernel.PowValidator.
From RigorKernel Require Import Kernel.ASERTValidator.
From RigorKernel Require Import Kernel.KernelSoundness.
From RigorSurface Require Core.Bytes.
From RigorSurface Require Core.Amount.
From RigorSurface Require Supply.ORUModel.
From RigorSurface Require Supply.KernelTx.
From RigorSurface Require Supply.Poles.
From RigorSurface Require Supply.RESTx.
From RigorSurface Require Script.ScriptTypes.
From RigorSurface Require Script.Classify.
From RigorSurface Require Pow.Compact.
From RigorSurface Require Pow.ASERTQ.
From RigorSurface Require Kernel.TxBool.
From RigorSurface Require Kernel.UTXO.
From RigorSurface Require Kernel.ORU.
From RigorSurface Require Kernel.RES.
From RigorSurface Require Kernel.Script.
From RigorSurface Require Kernel.Pow.
From RigorSurface Require Kernel.ASERT.
From RigorSurface Require Kernel.Validate.

Local Open Scope Z_scope.

Module SurfBytes := RigorSurface.Core.Bytes.
Module SurfAmount := RigorSurface.Core.Amount.
Module SurfORU := RigorSurface.Supply.ORUModel.
Module SurfKTx := RigorSurface.Supply.KernelTx.
Module SurfPoles := RigorSurface.Supply.Poles.
Module SurfRESTx := RigorSurface.Supply.RESTx.
Module SurfST := RigorSurface.Script.ScriptTypes.
Module SurfCl := RigorSurface.Script.Classify.
Module SurfCpt := RigorSurface.Pow.Compact.
Module SurfAQ := RigorSurface.Pow.ASERTQ.
Module SurfTx := RigorSurface.Kernel.TxBool.
Module SurfUTXO := RigorSurface.Kernel.UTXO.
Module SurfORUChk := RigorSurface.Kernel.ORU.
Module SurfRES := RigorSurface.Kernel.RES.
Module SurfScr := RigorSurface.Kernel.Script.
Module SurfPow := RigorSurface.Kernel.Pow.
Module SurfASERT := RigorSurface.Kernel.ASERT.
Module SurfVal := RigorSurface.Kernel.Validate.

(** * Adapters *)

Definition domain_of_cone (d : input_domain) : SurfORU.input_domain :=
  match d with
  | DomORU => SurfORU.DomORU
  | DomRES => SurfORU.DomRES
  end.

Definition outpoint_of_cone (o : outpoint) : SurfTx.outpoint :=
  {| SurfTx.op_txid := op_txid o; SurfTx.op_index := op_index o |}.

Definition txout_of_cone (o : txout) : SurfTx.txout :=
  {| SurfTx.txo_value := txo_value o; SurfTx.txo_spk := txo_spk o |}.

Definition utxo_of_cone (s : UTXOState) : SurfUTXO.UTXOState :=
  {| SurfTx.tx_id := tx_id s;
     SurfTx.tx_in := map outpoint_of_cone (tx_in s);
     SurfTx.tx_out := map txout_of_cone (tx_out s);
     SurfTx.tx_coinbase := tx_coinbase s |}.

Definition oru_of_cone (s : ORUState) : SurfORUChk.ORUState :=
  {| SurfKTx.kt_dom := domain_of_cone (kt_dom s);
     SurfKTx.kt_oru_in := kt_oru_in s;
     SurfKTx.kt_oru_out := kt_oru_out s;
     SurfKTx.kt_res_in := kt_res_in s;
     SurfKTx.kt_res_out := kt_res_out s;
     SurfKTx.kt_old_supply := kt_old_supply s;
     SurfKTx.kt_subsidy := kt_subsidy s |}.

Definition res_of_cone (s : RESState) : SurfRES.RESState :=
  {| SurfRESTx.rt_coinbase := rt_coinbase s;
     SurfRESTx.rt_mint_ok := rt_mint_ok s;
     SurfRESTx.rt_mint_amount := rt_mint_amount s;
     SurfRESTx.rt_height := rt_height s;
     SurfRESTx.rt_burn_oru := rt_burn_oru s;
     SurfRESTx.rt_dom := domain_of_cone (rt_dom s);
     SurfRESTx.rt_res_in := rt_res_in s;
     SurfRESTx.rt_res_out := rt_res_out s;
     SurfRESTx.rt_oru_in := rt_oru_in s |}.

Definition script_type_of_cone (t : ScriptType) : SurfST.ScriptType :=
  match t with
  | T_P2PKH => SurfST.T_P2PKH
  | T_P2SH => SurfST.T_P2SH
  | T_WITNESS_V0 => SurfST.T_WITNESS_V0
  | T_WITNESS_V1 => SurfST.T_WITNESS_V1
  | T_NONSTANDARD => SurfST.T_NONSTANDARD
  | T_MINT => SurfST.T_MINT
  | T_CONVERGE => SurfST.T_CONVERGE
  | T_NULL_DATA => SurfST.T_NULL_DATA
  end.

Definition script_of_cone (s : ScriptState) : SurfScr.ScriptState :=
  {| SurfScr.ss_script := ss_script s;
     SurfScr.ss_expected := script_type_of_cone (ss_expected s) |}.

Definition pow_of_cone (h : PowHeader) : SurfPow.PowHeader :=
  {| SurfPow.pow_digest_z := pow_digest_z h;
     SurfPow.pow_target := pow_target h;
     SurfPow.pow_compact_t := pow_compact_t h |}.

Definition asert_of_cone (i : ASERTInput) : SurfASERT.ASERTInput :=
  {| SurfASERT.asert_te := asert_te i;
     SurfASERT.asert_tau := asert_tau i;
     SurfASERT.asert_expected_q := asert_expected_q i |}.

Definition input_of_cone (input : KernelValidationRequest) : SurfVal.KernelValidationRequest :=
  match input with
  | KI_UTXO s => SurfVal.KI_UTXO (utxo_of_cone s)
  | KI_ORU s => SurfVal.KI_ORU (oru_of_cone s)
  | KI_RES s => SurfVal.KI_RES (res_of_cone s)
  | KI_Script s => SurfVal.KI_Script (script_of_cone s)
  | KI_Pow h => SurfVal.KI_Pow (pow_of_cone h)
  | KI_ASERT i => SurfVal.KI_ASERT (asert_of_cone i)
  end.

(** * Copied helpers agree *)

Lemma split_at_eq :
  forall n xs, SurfBytes.split_at n xs = split_at n xs.
Proof. intros n xs. reflexivity. Qed.

Lemma money_range_b_eq :
  forall v, SurfAmount.money_range_b v = money_range_b v.
Proof. intros v. reflexivity. Qed.

Lemma bytes_eqb_eq :
  forall a b, SurfTx.bytes_eqb a b = bytes_eqb a b.
Proof. intros a b. reflexivity. Qed.

Lemma outpoint_eqb_eq :
  forall o1 o2,
    SurfTx.outpoint_eqb (outpoint_of_cone o1) (outpoint_of_cone o2)
    = outpoint_eqb o1 o2.
Proof.
  intros o1 o2. unfold SurfTx.outpoint_eqb, outpoint_eqb, outpoint_of_cone.
  simpl. rewrite bytes_eqb_eq. reflexivity.
Qed.

Lemma is_output_op_b_eq :
  forall t o,
    SurfTx.is_output_op_b (utxo_of_cone t) (outpoint_of_cone o)
    = is_output_op_b t o.
Proof.
  intros t o.
  unfold SurfTx.is_output_op_b, is_output_op_b, utxo_of_cone, outpoint_of_cone.
  simpl. rewrite bytes_eqb_eq, map_length. reflexivity.
Qed.

Lemma forallb_map :
  forall A B (f : B -> bool) (g : A -> B) (l : list A),
    forallb f (map g l) = forallb (fun x => f (g x)) l.
Proof.
  intros A B f g l. induction l as [|x xs IH]; simpl; [reflexivity|].
  rewrite IH. reflexivity.
Qed.

Lemma existsb_map :
  forall A B (f : B -> bool) (g : A -> B) (l : list A),
    existsb f (map g l) = existsb (fun x => f (g x)) l.
Proof.
  intros A B f g l. induction l as [|x xs IH]; simpl; [reflexivity|].
  rewrite IH. reflexivity.
Qed.

Lemma forallb_ext :
  forall A (f g : A -> bool) (l : list A),
    (forall x, f x = g x) -> forallb f l = forallb g l.
Proof.
  intros A f g l H. induction l as [|x xs IH]; simpl; [reflexivity|].
  rewrite H, IH. reflexivity.
Qed.

Lemma existsb_ext :
  forall A (f g : A -> bool) (l : list A),
    (forall x, f x = g x) -> existsb f l = existsb g l.
Proof.
  intros A f g l H. induction l as [|x xs IH]; simpl; [reflexivity|].
  rewrite H, IH. reflexivity.
Qed.

Lemma inputs_fresh_b_eq :
  forall t, SurfTx.inputs_fresh_b (utxo_of_cone t) = inputs_fresh_b t.
Proof.
  intros t.
  unfold SurfTx.inputs_fresh_b, inputs_fresh_b.
  change (SurfTx.tx_in (utxo_of_cone t)) with (map outpoint_of_cone (tx_in t)).
  rewrite forallb_map.
  apply forallb_ext. intros o.
  rewrite is_output_op_b_eq. reflexivity.
Qed.

Lemma sum_values_eq :
  forall outs, SurfTx.sum_values (map txout_of_cone outs) = sum_values outs.
Proof.
  induction outs as [|o os IH]; simpl; [reflexivity|].
  rewrite IH. reflexivity.
Qed.

Lemma nodupb_eq :
  forall l, SurfTx.nodupb (map outpoint_of_cone l) = nodupb l.
Proof.
  induction l as [|x xs IH]; simpl; [reflexivity|].
  rewrite existsb_map, IH.
  rewrite (existsb_ext _ _ _ xs (outpoint_eqb_eq x)).
  reflexivity.
Qed.

Lemma nonemptyb_map :
  forall A B (f : A -> B) (l : list A),
    SurfTx.nonemptyb (map f l) = nonemptyb l.
Proof. intros A B f l. destruct l; reflexivity. Qed.

Lemma valid_tx_b_eq :
  forall t, SurfTx.valid_tx_b (utxo_of_cone t) = valid_tx_b t.
Proof.
  intros t.
  unfold SurfTx.valid_tx_b, valid_tx_b.
  rewrite inputs_fresh_b_eq.
  unfold utxo_of_cone. simpl.
  rewrite nonemptyb_map, nonemptyb_map, nodupb_eq, sum_values_eq,
          money_range_b_eq, forallb_map.
  rewrite (forallb_ext _ _ _ (tx_out t)
             (fun o => money_range_b_eq (SurfTx.txo_value (txout_of_cone o)))).
  reflexivity.
Qed.

Lemma poles_eq :
  SurfPoles.RES_MAX_SUPPLY = RES_MAX_SUPPLY
  /\ SurfPoles.RES_DESTROY_SHIFT = RES_DESTROY_SHIFT
  /\ (forall h, SurfPoles.res_mint_authorized h = res_mint_authorized h)
  /\ (forall h, SurfPoles.res_destroy_pole_sq h = res_destroy_pole_sq h).
Proof. repeat split; intros; reflexivity. Qed.

Lemma compact_eq :
  forall t nbits,
    SurfCpt.compact_encode t = compact_encode t
    /\ SurfCpt.compact_decode nbits = compact_decode nbits.
Proof. intros t nbits. split; reflexivity. Qed.

Lemma exp_q_raw_eq :
  forall te tau, SurfAQ.exp_q_raw te tau = exp_q_raw te tau.
Proof. intros te tau. reflexivity. Qed.

Lemma script_type_eqb_eq :
  forall a b,
    SurfST.script_type_eqb (script_type_of_cone a) (script_type_of_cone b)
    = script_type_eqb a b.
Proof. intros a b; destruct a, b; reflexivity. Qed.

Lemma witness_version_of_eq :
  forall b, SurfCl.witness_version_of b = witness_version_of b.
Proof. intros b. reflexivity. Qed.

Lemma parse_witness_program_eq :
  forall s, SurfCl.parse_witness_program s = parse_witness_program s.
Proof. intros s. reflexivity. Qed.

Lemma prefix4_eq_eq :
  forall prog tag, SurfCl.prefix4_eq prog tag = prefix4_eq prog tag.
Proof. intros prog tag. reflexivity. Qed.

Lemma is_native_res_program_eq :
  forall prog, SurfCl.is_native_res_program prog = is_native_res_program prog.
Proof. intros prog. reflexivity. Qed.

Lemma is_mint_script_eq :
  forall s, SurfCl.is_mint_script s = is_mint_script s.
Proof. intros s. reflexivity. Qed.

Lemma is_converge_script_eq :
  forall s, SurfCl.is_converge_script s = is_converge_script s.
Proof. intros s. reflexivity. Qed.

Lemma is_push_only_n_eq :
  forall fuel rest, SurfCl.is_push_only_n fuel rest = is_push_only_n fuel rest.
Proof. intros fuel rest. reflexivity. Qed.

Lemma is_push_only_eq :
  forall rest, SurfCl.is_push_only rest = is_push_only rest.
Proof. intros rest. reflexivity. Qed.

Lemma is_null_data_eq :
  forall s, SurfCl.is_null_data s = is_null_data s.
Proof. intros s. reflexivity. Qed.

Lemma script_type_of_if :
  forall (c : bool) a b,
    script_type_of_cone (if c then a else b)
    = if c then script_type_of_cone a else script_type_of_cone b.
Proof. intros c a b; destruct c; reflexivity. Qed.

Lemma classify_script_eq :
  forall s, SurfCl.classify_script s = script_type_of_cone (classify_script s).
Proof.
  intros s.
  unfold SurfCl.classify_script, classify_script.
  replace (SurfCl.parse_witness_program s) with (parse_witness_program s)
    by apply parse_witness_program_eq.
  change Bytes.byte with byte in *.
  destruct (parse_witness_program s) as [[v prog]|].
  - destruct v as [|[|]].
    + rewrite script_type_of_if.
      destruct (Nat.eqb (length prog) 20); reflexivity.
    + replace (SurfCl.is_native_res_program prog) with (is_native_res_program prog)
        by apply is_native_res_program_eq.
      rewrite script_type_of_if.
      destruct (is_native_res_program prog); reflexivity.
    + reflexivity.
  - replace (SurfCl.is_mint_script s) with (is_mint_script s)
      by apply is_mint_script_eq.
    rewrite script_type_of_if.
    destruct (is_mint_script s); [reflexivity|].
    replace (SurfCl.is_converge_script s) with (is_converge_script s)
      by apply is_converge_script_eq.
    rewrite script_type_of_if.
    destruct (is_converge_script s); [reflexivity|].
    replace (SurfCl.is_null_data s) with (is_null_data s)
      by apply is_null_data_eq.
    rewrite script_type_of_if.
    destruct (is_null_data s); reflexivity.
Qed.

Lemma valid_oru_tx_b_eq :
  forall t,
    SurfORUChk.valid_oru_tx_b
      {| SurfORU.tx_dom := domain_of_cone (tx_dom t);
         SurfORU.oru_in := oru_in t;
         SurfORU.oru_out := oru_out t;
         SurfORU.res_in := res_in t;
         SurfORU.res_out := res_out t |}
    = valid_oru_tx_b t.
Proof.
  intros t.
  unfold SurfORUChk.valid_oru_tx_b, valid_oru_tx_b. simpl.
  destruct (tx_dom t); reflexivity.
Qed.

(** * Phase 2 correspondence *)

Theorem surface_check_utxo_eq :
  forall s, SurfUTXO.check_utxo (utxo_of_cone s) = check_utxo s.
Proof.
  intros s. unfold SurfUTXO.check_utxo, check_utxo. apply valid_tx_b_eq.
Qed.

Theorem surface_check_oru_eq :
  forall s, SurfORUChk.check_oru (oru_of_cone s) = check_oru s.
Proof.
  intros s.
  unfold SurfORUChk.check_oru, check_oru, SurfKTx.to_oru, oru_of_cone.
  simpl. apply (valid_oru_tx_b_eq (to_oru s)).
Qed.

Theorem surface_check_res_eq :
  forall s, SurfRES.check_res (res_of_cone s) = check_res s.
Proof.
  intros s. unfold SurfRES.check_res, check_res, res_of_cone.
  unfold SurfRESTx.coinbase, SurfRESTx.mint_amount, SurfRESTx.burn_oru.
  unfold coinbase, mint_amount, burn_oru. simpl.
  reflexivity.
Qed.

Theorem surface_check_script_eq :
  forall s, SurfScr.check_script (script_of_cone s) = check_script s.
Proof.
  intros s.
  unfold SurfScr.check_script, check_script, script_of_cone. simpl.
  rewrite classify_script_eq. apply script_type_eqb_eq.
Qed.

Theorem surface_check_pow_eq :
  forall h, SurfPow.check_pow (pow_of_cone h) = check_pow h.
Proof.
  intros h. unfold SurfPow.check_pow, check_pow, pow_of_cone. simpl.
  reflexivity.
Qed.

Theorem surface_check_asert_eq :
  forall i, SurfASERT.check_asert (asert_of_cone i) = check_asert i.
Proof.
  intros i. unfold SurfASERT.check_asert, check_asert, asert_of_cone. simpl.
  reflexivity.
Qed.

Theorem surface_validate_kernel_eq :
  forall input,
    SurfVal.validate_kernel (input_of_cone input) = validate_kernel input.
Proof.
  intros input. destruct input; simpl.
  - apply surface_check_utxo_eq.
  - apply surface_check_oru_eq.
  - apply surface_check_res_eq.
  - apply surface_check_script_eq.
  - apply surface_check_pow_eq.
  - apply surface_check_asert_eq.
Qed.

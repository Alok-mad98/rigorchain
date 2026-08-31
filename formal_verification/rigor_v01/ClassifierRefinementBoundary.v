(** RIGOR v0.1 — classifier refinement boundary (P0-040).

      C++ ClassifyRigorTxout / AssetTypeFromScriptPubKey
            ↓  finite golden bytes + reusable parser lemmas
      Coq [classify]
            ↓  unique mint parser
      Coq [parse_canonical_mint]

    Live [classify] uses the unique GetResMintScript encoding
    (OP_PUSHBYTES_1 0x05 accepted; OP_5 rejected).

    P0-040: uniqueness / OP_1..OP_16 prefix / classify control-flow lemmas.
    Still not ∀-byte VST of the C++ function.

      classifier_refinement_proved = false

    No [Axiom] / [Admitted]. Production classifier C++ is not modified. *)
From Coq Require Import List.
From Coq Require Import Bool.Bool.
From Coq Require Import Arith.PeanoNat.
From Coq Require Import ZArith.ZArith.
Import ListNotations.
Require Import RigorV01.AssetClassification.
Require Import RigorV01.CanonicalMintEncoding.
Require Import RigorV01.ScriptAssetBoundary.
Require Import RigorV01.ResMintCorrectness.

Definition classifier_refinement_proved : bool := false.

Theorem classifier_refinement_not_proved :
  classifier_refinement_proved = false.
Proof. reflexivity. Qed.

Definition CoqAsset := AssetType.

(** Model-side C++ image: [classify] after unique mint encoding. *)
Definition classify_cpp_aligned (spk : list byte) : AssetType :=
  classify spk.

Definition classifier_refinement_obligation
    (cpp : list byte -> AssetType) : Prop :=
  forall spk, cpp spk = classify spk.

Theorem classify_eq_cpp_aligned :
  forall spk, classify spk = classify_cpp_aligned spk.
Proof. intros. reflexivity. Qed.

(** C++ ladder in rigor_classify.cpp: witness → mint → converge → OP_RETURN. *)
Theorem classify_is_cpp_decision_tree :
  forall spk,
    classify spk =
      match parse_witness_program spk with
      | Some (0%nat, prog) =>
          if Nat.eqb (length prog) 20%nat then AssetORU else AssetInvalid
      | Some (1%nat, prog) =>
          if is_native_res_program prog then AssetRES else AssetInvalid
      | Some _ => AssetInvalid
      | None =>
          if is_res_mint_script spk then AssetRES
          else if is_converge_script spk then AssetORU
          else if is_op_return spk then AssetNone
          else AssetInvalid
      end.
Proof. intros. reflexivity. Qed.

Theorem live_parse_canonical_mint_is_encoder :
  forall spk m pkh,
    AssetClassification.parse_canonical_mint spk = Some (m, pkh) ->
    spk = AssetClassification.encode_canonical_mint m pkh.
Proof. exact parse_canonical_mint_means_encoder. Qed.

Theorem op1_through_op16_cannot_be_canonical_mint_prefix :
  forall op rest,
    Nat.leb 81%nat op = true ->
    Nat.leb op 96%nat = true ->
    AssetClassification.parse_canonical_mint (op :: rest) = None.
Proof. exact op1_to_op16_not_canonical_mint. Qed.

Theorem canonical_mint_0_is_aligned_res :
  classify_cpp_aligned (encode_canonical_mint 0%Z pkh20) = AssetRES.
Proof. vm_compute. reflexivity. Qed.

Theorem pushbytes_1_05_is_res_not_op5 :
  classify (encode_canonical_mint 5%Z pkh20) = AssetRES /\
  classify (85%nat :: 20%nat :: app_bytes pkh20 [RIGOR_MARKER_RES_MINT])
    = AssetInvalid.
Proof. split; vm_compute; reflexivity. Qed.

Theorem nonminimal_mint_is_not_aligned_res :
  classify_cpp_aligned
    (85%nat :: 20%nat :: app_bytes pkh20 [RIGOR_MARKER_RES_MINT])
    <> AssetRES.
Proof. vm_compute. discriminate. Qed.

Theorem malformed_script_cannot_classify :
  classify nil = AssetInvalid /\
  classify_cpp_aligned nil = AssetInvalid.
Proof. split; [apply empty_is_invalid | apply empty_is_invalid]. Qed.

Theorem p2wpkh_aligned_is_oru :
  classify_cpp_aligned canonical_p2wpkh = AssetORU.
Proof. vm_compute. reflexivity. Qed.

Theorem p2wsh_aligned_is_invalid :
  classify_cpp_aligned canonical_p2wsh = AssetInvalid.
Proof. vm_compute. reflexivity. Qed.

Theorem native_res_aligned_is_res :
  classify_cpp_aligned canonical_native_res = AssetRES.
Proof. vm_compute. reflexivity. Qed.

Theorem converge_aligned_is_oru :
  classify_cpp_aligned canonical_converge = AssetORU.
Proof. vm_compute. reflexivity. Qed.

Theorem create_eq_spend_still :
  forall l, mint_create_predicate l = mint_spend_predicate l.
Proof. exact mint_create_eq_spend. Qed.

Definition CLASSIFIER_REFINEMENT_STATUS : bool := classifier_refinement_proved.

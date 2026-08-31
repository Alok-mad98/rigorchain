(** RIGOR v0.1 — canonical mint ScriptPubKey encoding (P0-036).

    C++ [GetResMintScript] / [ParseResMintScript] accept only the unique
    encoding the encoder emits (RIG-04). Live C++ [CScript << CScriptNum]
    is [<< getvch()]: locktimes 1–16 are a 1-byte data push, NOT OP_1..OP_16.
    OP_n is a second encoding and fails uniqueness.

    P0-041: the encoder, the parser and their uniqueness proof live in
    [AssetClassification.v] only — the same terms [classify] dispatches on.
    This module is the fixture / fail-closed catalogue over those terms and
    defines no parser of its own, so the two can no longer drift apart.

    Not a C++ refinement. No [Axiom] / [Admitted]. HASH160 is opaque. *)
From Coq Require Import List.
From Coq Require Import Bool.Bool.
From Coq Require Import Arith.PeanoNat.
From Coq Require Import ZArith.ZArith.
Import ListNotations.
Require Import RigorV01.AssetClassification.
Require Import RigorV01.ResMintCorrectness.

Local Open Scope Z_scope.

Lemma app_bytes_assoc :
  forall a b c, app_bytes a (app_bytes b c) = app_bytes (app_bytes a b) c.
Proof.
  induction a as [|x xs IH]; intros; [reflexivity|].
  simpl. rewrite IH. reflexivity.
Qed.

Lemma app_bytes_nil_r : forall a, app_bytes a nil = a.
Proof. induction a; simpl; [reflexivity|]. rewrite IHa. reflexivity. Qed.

(** * Create = spend (re-export) *)

Theorem mint_create_field_eq_spend_field :
  forall l, mint_create_predicate l = mint_spend_predicate l.
Proof. exact mint_create_eq_spend. Qed.

(** * Fixtures: unique encoding *)

Definition pkh20 : list byte := repeat_byte 20 1%nat.

Lemma encode_scriptnum_0 : encode_scriptnum 0 = [RIGOR_PUSH_0].
Proof. reflexivity. Qed.

Lemma encode_scriptnum_5 : encode_scriptnum 5 = [1%nat; 5%nat].
Proof. vm_compute. reflexivity. Qed.

Lemma encode_scriptnum_16 : encode_scriptnum 16 = [1%nat; 16%nat].
Proof. vm_compute. reflexivity. Qed.

Lemma encode_scriptnum_100 :
  encode_scriptnum 100 = [1%nat; 100%nat].
Proof. vm_compute. reflexivity. Qed.

Theorem canonical_mint_0_roundtrip :
  parse_canonical_mint (encode_canonical_mint 0 pkh20) = Some (0, pkh20).
Proof. vm_compute. reflexivity. Qed.

Theorem canonical_mint_5_roundtrip :
  parse_canonical_mint (encode_canonical_mint 5 pkh20) = Some (5, pkh20).
Proof. vm_compute. reflexivity. Qed.

Theorem canonical_mint_100_roundtrip :
  parse_canonical_mint (encode_canonical_mint 100 pkh20) = Some (100, pkh20).
Proof. vm_compute. reflexivity. Qed.

Theorem canonical_mint_150_roundtrip :
  parse_canonical_mint (encode_canonical_mint 150 pkh20) = Some (150, pkh20).
Proof. vm_compute. reflexivity. Qed.

(** * Uniqueness: successful parse implies the bytes *are* the encoder output.

    Re-exported from [AssetClassification] so that this catalogue and the
    live [classify] dispatch quantify over one and the same parser. *)

Theorem parse_canonical_mint_unique :
  forall spk maturity pkh,
    parse_canonical_mint spk = Some (maturity, pkh) ->
    spk = encode_canonical_mint maturity pkh.
Proof. exact parse_canonical_mint_means_encoder. Qed.

Theorem two_encodings_cannot_be_same_canonical_mint :
  forall s1 s2 m pkh,
    parse_canonical_mint s1 = Some (m, pkh) ->
    parse_canonical_mint s2 = Some (m, pkh) ->
    s1 = s2.
Proof. exact two_scripts_same_mint_parse_are_byte_equal. Qed.

(** * Negative: malformed / non-minimal / trailing / PUSHDATA *)

Theorem nonminimal_small_push_is_not_canonical :
  parse_canonical_mint (85%nat :: 20%nat :: app_bytes pkh20 [RIGOR_MARKER_RES_MINT])
    = None.
Proof. vm_compute. reflexivity. Qed.

Theorem pushdata1_is_not_canonical_mint :
  parse_canonical_mint
    (RIGOR_PUSHDATA1 :: 1%nat :: 5%nat :: 20%nat
      :: app_bytes pkh20 [RIGOR_MARKER_RES_MINT])
    = None.
Proof. vm_compute. reflexivity. Qed.

Theorem trailing_bytes_cannot_be_canonical_mint :
  parse_canonical_mint
    (app_bytes (encode_canonical_mint 0 pkh20) [0%nat]) = None.
Proof. vm_compute. reflexivity. Qed.

Theorem wrong_marker_cannot_be_canonical_mint :
  parse_canonical_mint
    (app_bytes (encode_scriptnum 0)
       (20%nat :: app_bytes pkh20 [RIGOR_MARKER_CONVERGE])) = None.
Proof. vm_compute. reflexivity. Qed.

Theorem empty_cannot_be_canonical_mint :
  parse_canonical_mint nil = None.
Proof. vm_compute. reflexivity. Qed.

Theorem wrong_pkh_length_cannot_be_canonical_mint :
  parse_canonical_mint
    (app_bytes (encode_scriptnum 0)
       (20%nat :: app_bytes (repeat_byte 19 1%nat) [RIGOR_MARKER_RES_MINT]))
    = None.
Proof. vm_compute. reflexivity. Qed.

(** A script the mint parser rejects, and that is not a witness program,
    cannot reach [AssetRES] through any other branch of [classify]. *)
Theorem unparseable_mint_is_not_res :
  forall spk,
    parse_witness_program spk = None ->
    parse_canonical_mint spk = None ->
    classify spk <> AssetRES.
Proof. exact classify_not_res_unless_witness_or_mint. Qed.

Theorem ambiguous_script_cannot_classify :
  parse_canonical_mint (85%nat :: 20%nat :: app_bytes pkh20 [RIGOR_MARKER_RES_MINT])
    = None /\
  parse_canonical_mint (encode_canonical_mint 5 pkh20) = Some (5, pkh20).
Proof. split; [apply nonminimal_small_push_is_not_canonical | apply canonical_mint_5_roundtrip]. Qed.

(** * Witness program fail-closed (length / trailing) *)

Theorem witness_wrong_length_fail_closed :
  classify canonical_p2wsh = AssetInvalid.
Proof. apply p2wsh_is_invalid. Qed.

Theorem witness_empty_fail_closed :
  classify nil = AssetInvalid.
Proof. apply empty_is_invalid. Qed.

Theorem trailing_on_p2wpkh_fail_closed :
  classify (app_bytes canonical_p2wpkh [0%nat]) = AssetInvalid.
Proof. vm_compute. reflexivity. Qed.

Theorem trailing_on_native_res_fail_closed :
  classify (app_bytes canonical_native_res [0%nat]) = AssetInvalid.
Proof. vm_compute. reflexivity. Qed.

Theorem wrong_witness_version_v2_fail_closed :
  classify (82%nat :: 20%nat :: repeat_byte 20 0%nat) = AssetInvalid.
Proof. vm_compute. reflexivity. Qed.

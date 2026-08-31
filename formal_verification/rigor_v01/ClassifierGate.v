(** RIGOR v0.1 — classifier first-byte gates (formalization level 1).

    [classify] inspects a finite set of first-byte *gates*. Any other first
    byte is [AssetInvalid] for every continuation. Bounded exhaustive search
    therefore only has to enumerate the gate set; the complement is this
    theorem, not a vector.

    Not a C++ VST. No [Axiom] / [Admitted]. *)
From Coq Require Import List.
From Coq Require Import Bool.Bool.
From Coq Require Import Arith.PeanoNat.
From Coq Require Import ZArith.ZArith.
From Coq Require Import Lia.
Import ListNotations.
Require Import RigorV01.AssetClassification.

(** First bytes that can open a witness, mint, converge, or OP_RETURN branch. *)
Definition byte_is_classify_gate (b : byte) : bool :=
  Nat.eqb b RIGOR_PUSH_0
  || (Nat.leb 1%nat b && Nat.leb b 4%nat)
  || Nat.eqb b RIGOR_PUSHBYTES_20
  || (Nat.leb RIGOR_PUSH_1 b && Nat.leb b RIGOR_PUSH_16)
  || Nat.eqb b RIGOR_MARKER_RETURN.

Lemma gate_false_not_push0 :
  forall b, byte_is_classify_gate b = false -> Nat.eqb b RIGOR_PUSH_0 = false.
Proof.
  intros b H. unfold byte_is_classify_gate in H.
  destruct (Nat.eqb b RIGOR_PUSH_0); [discriminate|reflexivity].
Qed.

Lemma gate_false_not_mint_push :
  forall b,
    byte_is_classify_gate b = false ->
    Nat.leb 1%nat b && Nat.leb b 4%nat = false.
Proof.
  intros b H. unfold byte_is_classify_gate in H.
  destruct (Nat.eqb b RIGOR_PUSH_0); [discriminate|].
  destruct (Nat.leb 1%nat b && Nat.leb b 4%nat); [discriminate|reflexivity].
Qed.

Lemma gate_false_not_push20 :
  forall b, byte_is_classify_gate b = false -> Nat.eqb b RIGOR_PUSHBYTES_20 = false.
Proof.
  intros b H. unfold byte_is_classify_gate in H.
  destruct (Nat.eqb b RIGOR_PUSH_0); [discriminate|].
  destruct (Nat.leb 1%nat b && Nat.leb b 4%nat); [discriminate|].
  destruct (Nat.eqb b RIGOR_PUSHBYTES_20); [discriminate|reflexivity].
Qed.

Lemma gate_false_not_op1_16 :
  forall b,
    byte_is_classify_gate b = false ->
    Nat.leb RIGOR_PUSH_1 b && Nat.leb b RIGOR_PUSH_16 = false.
Proof.
  intros b H. unfold byte_is_classify_gate in H.
  destruct (Nat.eqb b RIGOR_PUSH_0); [discriminate|].
  destruct (Nat.leb 1%nat b && Nat.leb b 4%nat); [discriminate|].
  destruct (Nat.eqb b RIGOR_PUSHBYTES_20); [discriminate|].
  destruct (Nat.leb RIGOR_PUSH_1 b && Nat.leb b RIGOR_PUSH_16); [discriminate|reflexivity].
Qed.

Lemma gate_false_not_return :
  forall b, byte_is_classify_gate b = false -> Nat.eqb b RIGOR_MARKER_RETURN = false.
Proof.
  intros b H. unfold byte_is_classify_gate in H.
  destruct (Nat.eqb b RIGOR_PUSH_0); [discriminate|].
  destruct (Nat.leb 1%nat b && Nat.leb b 4%nat); [discriminate|].
  destruct (Nat.eqb b RIGOR_PUSHBYTES_20); [discriminate|].
  destruct (Nat.leb RIGOR_PUSH_1 b && Nat.leb b RIGOR_PUSH_16); [discriminate|].
  destruct (Nat.eqb b RIGOR_MARKER_RETURN); [discriminate|reflexivity].
Qed.

Lemma nongate_not_witness_version :
  forall b, byte_is_classify_gate b = false -> witness_version_of b = None.
Proof.
  intros b H.
  unfold witness_version_of.
  rewrite (gate_false_not_push0 b H).
  rewrite (gate_false_not_op1_16 b H).
  reflexivity.
Qed.

Lemma parse_witness_nongate_head :
  forall b rest,
    witness_version_of b = None ->
    parse_witness_program (b :: rest) = None.
Proof.
  intros b rest Hv.
  unfold parse_witness_program.
  destruct (orb (Nat.ltb (length (b :: rest)) 4%nat)
                (Nat.ltb 42%nat (length (b :: rest)))); [reflexivity|].
  destruct rest as [|plen rest2]; [reflexivity|].
  simpl. rewrite Hv. reflexivity.
Qed.

Lemma decode_scriptnum_nongate_head :
  forall b rest,
    byte_is_classify_gate b = false ->
    decode_scriptnum_prefix (b :: rest) = None.
Proof.
  intros b rest H.
  unfold decode_scriptnum_prefix.
  rewrite (gate_false_not_push0 b H).
  rewrite (gate_false_not_mint_push b H).
  reflexivity.
Qed.

Lemma parse_mint_nongate_head :
  forall b rest,
    byte_is_classify_gate b = false ->
    parse_canonical_mint (b :: rest) = None.
Proof.
  intros b rest H.
  unfold parse_canonical_mint.
  rewrite (decode_scriptnum_nongate_head b rest H).
  reflexivity.
Qed.

Lemma converge_nongate_head :
  forall b rest,
    byte_is_classify_gate b = false ->
    is_converge_script (b :: rest) = false.
Proof.
  intros b rest H.
  unfold is_converge_script.
  rewrite (gate_false_not_push20 b H).
  reflexivity.
Qed.

Lemma opreturn_nongate_head :
  forall b rest,
    byte_is_classify_gate b = false ->
    is_op_return (b :: rest) = false.
Proof.
  intros b rest H.
  unfold is_op_return.
  rewrite (gate_false_not_return b H).
  reflexivity.
Qed.

(** Non-gate first byte: classification is independent of the rest of the
    script. This is the byte-space complement of the bounded exhaustive
    search over the 23 gate values. *)
Theorem classify_nongate_first_byte :
  forall b rest,
    byte_is_classify_gate b = false ->
    classify (b :: rest) = AssetInvalid.
Proof.
  intros b rest H.
  unfold classify, is_res_mint_script.
  rewrite (parse_witness_nongate_head b rest (nongate_not_witness_version b H)).
  rewrite (parse_mint_nongate_head b rest H).
  rewrite (converge_nongate_head b rest H).
  rewrite (opreturn_nongate_head b rest H).
  reflexivity.
Qed.

Theorem empty_script_is_invalid :
  classify nil = AssetInvalid.
Proof. apply empty_is_invalid. Qed.

Lemma app_bytes_length :
  forall a b, length (app_bytes a b) = (length a + length b)%nat.
Proof.
  induction a as [|x xs IH]; intros; [reflexivity|].
  simpl. rewrite IH. reflexivity.
Qed.

Lemma encode_scriptnum_nonnil :
  forall n, encode_scriptnum n <> nil.
Proof.
  intros n. unfold encode_scriptnum.
  destruct (Z.ltb n 0); [discriminate|].
  destruct (scriptnum_payload n); discriminate.
Qed.

Lemma encode_canonical_mint_len_ge_23 :
  forall m pkh,
    length pkh = 20%nat ->
    Nat.leb 23%nat (length (encode_canonical_mint m pkh)) = true.
Proof.
  intros m pkh Hp.
  unfold encode_canonical_mint.
  rewrite app_bytes_length. simpl.
  rewrite app_bytes_length. simpl.
  rewrite Hp.
  destruct (encode_scriptnum m) as [|x xs] eqn:He.
  - exfalso. eapply encode_scriptnum_nonnil. exact He.
  - apply (proj2 (Nat.leb_le 23%nat _)). simpl. lia.
Qed.

Lemma parse_canonical_mint_len_ge_23 :
  forall spk m pkh,
    parse_canonical_mint spk = Some (m, pkh) ->
    Nat.leb 23%nat (length spk) = true.
Proof.
  intros spk m pkh H.
  assert (Hk : length pkh = 20%nat).
  { unfold parse_canonical_mint in H.
    destruct (decode_scriptnum_prefix spk) as [[m' rest]|]; [|discriminate].
    destruct rest as [|plen body]; [discriminate|].
    destruct (Nat.eqb plen RIGOR_PUSHBYTES_20); [|discriminate].
    destruct (split_at 20%nat body) as [[pkh' marker_tl]|] eqn:Hs; [|discriminate].
    destruct marker_tl as [|op tl]; [discriminate|].
    destruct tl; [|discriminate].
    destruct (Nat.eqb op RIGOR_MARKER_RES_MINT) eqn:Hop; [|discriminate].
    apply Nat.eqb_eq in Hop. subst op.
    inversion H. subst pkh.
    pose proof (mint_split_at_app 20%nat body pkh' [RIGOR_MARKER_RES_MINT] Hs) as [_ Hlen].
    exact Hlen. }
  rewrite (parse_canonical_mint_means_encoder spk m pkh H).
  apply encode_canonical_mint_len_ge_23. exact Hk.
Qed.

Lemma parse_witness_length_lt_4 :
  forall spk,
    Nat.ltb (length spk) 4%nat = true ->
    parse_witness_program spk = None.
Proof.
  intros spk H.
  unfold parse_witness_program. rewrite H. reflexivity.
Qed.

(** Length < 4 cannot be RES: witness programs need ≥ 4 bytes and a canonical
    mint is ≥ 23 bytes. *)
Theorem classify_length_lt_4_not_res :
  forall spk,
    Nat.ltb (length spk) 4%nat = true ->
    classify spk <> AssetRES.
Proof.
  intros spk Hlen.
  unfold classify, is_res_mint_script.
  rewrite (parse_witness_length_lt_4 spk Hlen).
  destruct (parse_canonical_mint spk) as [[m pkh]|] eqn:Hm.
  - pose proof (parse_canonical_mint_len_ge_23 spk m pkh Hm) as Hge.
    apply Nat.leb_le in Hge.
    apply Nat.ltb_lt in Hlen.
    lia.
  - destruct (is_converge_script spk); [discriminate|].
    destruct (is_op_return spk); discriminate.
Qed.

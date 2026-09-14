(** ConsensusModel.CompactSizeModel

    Bitcoin-shaped CompactSize. Rejects non-canonical encodings.
    Not C++ ReadCompactSize. Not extraction.

    Counts are [N]. They range up to 2^64, so a [nat] count could not
    be evaluated and the bound constants could not even be built.

    CPP_KERNELINPUT_EQUIVALENCE = NOT PROVED
*)
From Coq Require Import Arith.PeanoNat.
From Coq Require Import Bool.Bool.
From Coq Require Import List.
From Coq Require Import NArith.NArith.
From Coq Require Import Lia.
From ConsensusModel Require Import ByteModel.
Import ListNotations.

Definition two16 : N := 65536%N.
Definition two32 : N := 4294967296%N.
Definition two64 : N := 18446744073709551616%N.

Lemma two16_pow : two16 = npow_base 2. Proof. reflexivity. Qed.
Lemma two32_pow : two32 = npow_base 4. Proof. reflexivity. Qed.
Lemma two64_pow : two64 = npow_base 8. Proof. reflexivity. Qed.

Lemma byte_base_gt_1 : 1 < byte_base.
Proof.
  unfold byte_base. apply Nat.ltb_lt. reflexivity.
Qed.

Lemma lt_253_byte_base : 253 < byte_base.
Proof.
  unfold byte_base. apply Nat.ltb_lt. reflexivity.
Qed.

Lemma lt_253_two16 : (253 < two16)%N.
Proof. unfold two16. lia. Qed.

Lemma lt_two16_two32 : (two16 < two32)%N.
Proof. unfold two16, two32. lia. Qed.

Lemma lt_two32_two64 : (two32 < two64)%N.
Proof. unfold two32, two64. lia. Qed.

Definition compactsize_ok (n : N) : Prop := (n < two64)%N.

Lemma cons_app :
  forall (x : Byte) (xs rest : byte_list),
    (x :: xs) ++ rest = x :: (xs ++ rest).
Proof.
  intros x xs rest. reflexivity.
Qed.

Definition encode_compactsize (n : N) : byte_list :=
  if (n <? 253)%N then [N.to_nat n]
  else if (n <? two16)%N then 253 :: encode_le 2 n
  else if (n <? two32)%N then 254 :: encode_le 4 n
  else 255 :: encode_le 8 n.

Definition decode_compactsize (bs : byte_list) : option (N * byte_list) :=
  match bs with
  | nil => None
  | b :: rest =>
      if b <? 253 then Some (N.of_nat b, rest)
      else if Nat.eqb b 253 then
        match decode_le 2 rest with
        | Some (n, rest') => if (n <? 253)%N then None else Some (n, rest')
        | None => None
        end
      else if Nat.eqb b 254 then
        match decode_le 4 rest with
        | Some (n, rest') => if (n <? two16)%N then None else Some (n, rest')
        | None => None
        end
      else if Nat.eqb b 255 then
        match decode_le 8 rest with
        | Some (n, rest') => if (n <? two32)%N then None else Some (n, rest')
        | None => None
        end
      else None
  end.

Lemma decode_encode_compactsize_app :
  forall n rest,
    compactsize_ok n ->
    decode_compactsize (encode_compactsize n ++ rest) = Some (n, rest).
Proof.
  intros n rest Hok.
  unfold compactsize_ok in Hok.
  unfold encode_compactsize.
  destruct (n <? 253)%N eqn:Hd.
  - apply N.ltb_lt in Hd.
    rewrite cons_app. unfold decode_compactsize.
    replace (N.to_nat n <? 253) with true
      by (symmetry; apply Nat.ltb_lt; lia).
    rewrite N2Nat.id. reflexivity.
  - destruct (n <? two16)%N eqn:H16.
    + rewrite cons_app.
      unfold decode_compactsize.
      assert (Ht : (253 <? 253) = false) by reflexivity.
      rewrite Ht, Nat.eqb_refl.
      apply N.ltb_lt in H16. rewrite two16_pow in H16.
      rewrite (decode_encode_le_app 2 n rest H16).
      rewrite Hd. reflexivity.
    + destruct (n <? two32)%N eqn:H32.
      * rewrite cons_app.
        unfold decode_compactsize.
        assert (Ht : (254 <? 253) = false) by reflexivity.
        assert (He : (254 =? 253) = false) by reflexivity.
        rewrite Ht, He, Nat.eqb_refl.
        apply N.ltb_lt in H32. rewrite two32_pow in H32.
        rewrite (decode_encode_le_app 4 n rest H32).
        rewrite H16. reflexivity.
      * rewrite cons_app.
        unfold decode_compactsize.
        assert (Ht : (255 <? 253) = false) by reflexivity.
        assert (He1 : (255 =? 253) = false) by reflexivity.
        assert (He2 : (255 =? 254) = false) by reflexivity.
        rewrite Ht, He1, He2, Nat.eqb_refl.
        rewrite two64_pow in Hok.
        rewrite (decode_encode_le_app 8 n rest Hok).
        rewrite H32. reflexivity.
Qed.

Lemma decode_encode_compactsize :
  forall n,
    compactsize_ok n ->
    decode_compactsize (encode_compactsize n) = Some (n, nil).
Proof.
  intros n Hok.
  pose proof (decode_encode_compactsize_app n nil Hok) as H.
  rewrite app_nil_r in H.
  exact H.
Qed.

Lemma encode_decode_compactsize :
  forall bs n rest,
    decode_compactsize bs = Some (n, rest) ->
    encode_compactsize n ++ rest = bs.
Proof.
  intros bs n rest H.
  destruct bs as [|b rest0]; [discriminate|].
  unfold decode_compactsize in H.
  unfold encode_compactsize.
  destruct (b <? 253) eqn:Hd.
  - injection H as Hn Hr. subst n rest.
    apply Nat.ltb_lt in Hd.
    replace ((N.of_nat b <? 253)%N) with true
      by (symmetry; apply N.ltb_lt; lia).
    rewrite Nat2N.id. reflexivity.
  - destruct (Nat.eqb b 253) eqn:H253.
    + apply Nat.eqb_eq in H253. subst b.
      destruct (decode_le 2 rest0) as [[n2 rest']|] eqn:Hd2; [|discriminate].
      destruct (n2 <? 253)%N eqn:Hn2; [discriminate|].
      injection H as Hn Hr. subst n rest.
      rewrite Hn2.
      destruct (n2 <? two16)%N eqn:H16.
      * rewrite cons_app.
        rewrite (decode_le_2_split rest0 n2 rest' Hd2).
        reflexivity.
      * exfalso.
        pose proof (decode_le_bound 2 rest0 n2 rest' Hd2) as Hb.
        rewrite <- two16_pow in Hb.
        apply N.ltb_ge in H16. lia.
    + destruct (Nat.eqb b 254) eqn:H254.
      * apply Nat.eqb_eq in H254. subst b.
        destruct (decode_le 4 rest0) as [[n4 rest']|] eqn:Hd4; [|discriminate].
        destruct (n4 <? two16)%N eqn:Hn4; [discriminate|].
        injection H as Hn Hr. subst n rest.
        apply N.ltb_ge in Hn4.
        pose proof lt_253_two16 as H253'.
        replace ((n4 <? 253)%N) with false
          by (symmetry; apply N.ltb_ge; lia).
        replace ((n4 <? two16)%N) with false
          by (symmetry; apply N.ltb_ge; lia).
        destruct (n4 <? two32)%N eqn:H32.
        -- rewrite cons_app.
           rewrite (decode_le_4_split rest0 n4 rest' Hd4).
           reflexivity.
        -- exfalso.
           pose proof (decode_le_bound 4 rest0 n4 rest' Hd4) as Hb.
           rewrite <- two32_pow in Hb.
           apply N.ltb_ge in H32. lia.
      * destruct (Nat.eqb b 255) eqn:H255; [|discriminate].
        apply Nat.eqb_eq in H255. subst b.
        destruct (decode_le 8 rest0) as [[n8 rest']|] eqn:Hd8; [|discriminate].
        destruct (n8 <? two32)%N eqn:Hn8; [discriminate|].
        injection H as Hn Hr. subst n rest.
        apply N.ltb_ge in Hn8.
        pose proof lt_253_two16 as H253'.
        pose proof lt_two16_two32 as H1632.
        replace ((n8 <? 253)%N) with false
          by (symmetry; apply N.ltb_ge; lia).
        replace ((n8 <? two16)%N) with false
          by (symmetry; apply N.ltb_ge; lia).
        replace ((n8 <? two32)%N) with false
          by (symmetry; apply N.ltb_ge; lia).
        rewrite cons_app.
        rewrite (decode_le_8_split rest0 n8 rest' Hd8).
        reflexivity.
Qed.

Theorem reject_noncanonical_encoding :
  forall n rest,
    (n < 253)%N ->
    decode_compactsize (253 :: encode_le 2 n ++ rest) = None.
Proof.
  intros n rest Hn.
  unfold decode_compactsize.
  assert (H253 : (253 <? 253) = false) by reflexivity.
  rewrite H253, Nat.eqb_refl.
  assert (Hb : (n < npow_base 2)%N).
  { rewrite <- two16_pow. pose proof lt_253_two16. lia. }
  rewrite (decode_encode_le_app 2 n rest Hb).
  destruct (n <? 253)%N eqn:E; [reflexivity|].
  exfalso. apply N.ltb_ge in E. lia.
Qed.

Lemma compactsize_canonical_unique :
  forall bs n,
    decode_compactsize bs = Some (n, nil) ->
    bs = encode_compactsize n.
Proof.
  intros bs n H.
  pose proof (encode_decode_compactsize bs n nil H) as Hs.
  rewrite app_nil_r in Hs.
  symmetry.
  exact Hs.
Qed.

(** A CompactSize field never reports more than the bytes behind it can
    hold, and it consumes at least one byte. Both facts let a caller
    bound work by the input length rather than by the decoded count,
    which is what keeps an adversarial count from being materialised. *)

Lemma decode_compactsize_consumes :
  forall bs n rest,
    decode_compactsize bs = Some (n, rest) ->
    List.length rest < List.length bs.
Proof.
  intros bs n rest H.
  destruct bs as [|b rest0]; [discriminate|].
  cbn [List.length].
  unfold decode_compactsize in H.
  destruct (b <? 253).
  - injection H as _ Hr. subst rest. lia.
  - destruct (Nat.eqb b 253).
    + destruct (decode_le 2 rest0) as [[n2 r2]|] eqn:D; [|discriminate].
      destruct (n2 <? 253)%N; [discriminate|].
      injection H as _ Hr. subst r2.
      pose proof (decode_le_consumes 2 rest0 n2 rest D). lia.
    + destruct (Nat.eqb b 254).
      * destruct (decode_le 4 rest0) as [[n4 r4]|] eqn:D; [|discriminate].
        destruct (n4 <? two16)%N; [discriminate|].
        injection H as _ Hr. subst r4.
        pose proof (decode_le_consumes 4 rest0 n4 rest D). lia.
      * destruct (Nat.eqb b 255); [|discriminate].
        destruct (decode_le 8 rest0) as [[n8 r8]|] eqn:D; [|discriminate].
        destruct (n8 <? two32)%N; [discriminate|].
        injection H as _ Hr. subst r8.
        pose proof (decode_le_consumes 8 rest0 n8 rest D). lia.
Qed.

Lemma encode_compactsize_nonempty : forall n, encode_compactsize n <> nil.
Proof.
  intros n. unfold encode_compactsize.
  destruct (n <? 253)%N; [discriminate|].
  destruct (n <? two16)%N; [discriminate|].
  destruct (n <? two32)%N; discriminate.
Qed.

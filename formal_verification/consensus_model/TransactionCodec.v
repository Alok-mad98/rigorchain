(** ConsensusModel.TransactionCodec

    Byte codec for TransactionModel. Structure only; no script interpreter.
    Not C++ UnserializeTransaction. Not extraction.

    CPP_KERNELINPUT_EQUIVALENCE = NOT PROVED
*)
From Coq Require Import Arith.PeanoNat.
From Coq Require Import Bool.Bool.
From Coq Require Import List.
From Coq Require Import ZArith.ZArith.
From Coq Require Import NArith.NArith.
From Coq Require Import Lia.
From ConsensusModel Require Import ByteModel.
From ConsensusModel Require Import CompactSizeModel.
From ConsensusModel Require Import TransactionModel.
Import ListNotations.

Definition z_nonneg (z : Z) : Prop := (0 <= z)%Z.
Definition z_u32 (z : Z) : Prop := z_nonneg z /\ (Z.to_N z < two32)%N.
Definition z_u64 (z : Z) : Prop := z_nonneg z /\ (Z.to_N z < two64)%N.

Lemma z_of_to_id :
  forall z, z_nonneg z -> Z.of_N (Z.to_N z) = z.
Proof.
  intros z H.
  apply Z2N.id.
  exact H.
Qed.

Definition encode_u32 (n : N) : byte_list := encode_le 4 n.
Definition decode_u32 (bs : byte_list) : option (N * byte_list) := decode_le 4 bs.
Definition encode_u64 (n : N) : byte_list := encode_le 8 n.
Definition decode_u64 (bs : byte_list) : option (N * byte_list) := decode_le 8 bs.

Lemma encode_u32_nonempty : forall n, encode_u32 n <> nil.
Proof. intros n. apply encode_le_nonempty. discriminate. Qed.

Lemma encode_u64_nonempty : forall n, encode_u64 n <> nil.
Proof. intros n. apply encode_le_nonempty. discriminate. Qed.

Lemma decode_encode_u32_app :
  forall n rest, (n < two32)%N -> decode_u32 (encode_u32 n ++ rest) = Some (n, rest).
Proof.
  intros n rest Hn.
  rewrite two32_pow in Hn.
  apply (decode_encode_le_app 4 n rest Hn).
Qed.

Lemma decode_encode_u64_app :
  forall n rest, (n < two64)%N -> decode_u64 (encode_u64 n ++ rest) = Some (n, rest).
Proof.
  intros n rest Hn.
  rewrite two64_pow in Hn.
  apply (decode_encode_le_app 8 n rest Hn).
Qed.

Lemma encode_decode_u32 :
  forall bs n rest, decode_u32 bs = Some (n, rest) -> encode_u32 n ++ rest = bs.
Proof.
  intros bs n rest H. apply decode_le_4_split. exact H.
Qed.

Lemma encode_decode_u64 :
  forall bs n rest, decode_u64 bs = Some (n, rest) -> encode_u64 n ++ rest = bs.
Proof.
  intros bs n rest H. apply decode_le_8_split. exact H.
Qed.

Definition encode_u32z (z : Z) : byte_list := encode_u32 (Z.to_N z).
Definition encode_u64z (z : Z) : byte_list := encode_u64 (Z.to_N z).

Definition decode_u32z (bs : byte_list) : option (Z * byte_list) :=
  match decode_u32 bs with
  | None => None
  | Some (n, rest) => Some (Z.of_N n, rest)
  end.

Definition decode_u64z (bs : byte_list) : option (Z * byte_list) :=
  match decode_u64 bs with
  | None => None
  | Some (n, rest) => Some (Z.of_N n, rest)
  end.

Lemma decode_encode_u32z_app :
  forall z rest, z_u32 z -> decode_u32z (encode_u32z z ++ rest) = Some (z, rest).
Proof.
  intros z rest [Hz Hn].
  unfold decode_u32z, encode_u32z.
  rewrite (decode_encode_u32_app (Z.to_N z) rest Hn).
  rewrite (z_of_to_id z Hz).
  reflexivity.
Qed.

Lemma decode_encode_u64z_app :
  forall z rest, z_u64 z -> decode_u64z (encode_u64z z ++ rest) = Some (z, rest).
Proof.
  intros z rest [Hz Hn].
  unfold decode_u64z, encode_u64z.
  rewrite (decode_encode_u64_app (Z.to_N z) rest Hn).
  rewrite (z_of_to_id z Hz).
  reflexivity.
Qed.

Lemma encode_decode_u32z :
  forall bs z rest, decode_u32z bs = Some (z, rest) -> encode_u32z z ++ rest = bs.
Proof.
  intros bs z rest H.
  unfold decode_u32z in H.
  destruct (decode_u32 bs) as [[n rest']|] eqn:Hd; [|discriminate].
  injection H as Hz Hr. subst z rest.
  unfold encode_u32z.
  rewrite N2Z.id.
  apply encode_decode_u32.
  exact Hd.
Qed.

Lemma encode_decode_u64z :
  forall bs z rest, decode_u64z bs = Some (z, rest) -> encode_u64z z ++ rest = bs.
Proof.
  intros bs z rest H.
  unfold decode_u64z in H.
  destruct (decode_u64 bs) as [[n rest']|] eqn:Hd; [|discriminate].
  injection H as Hz Hr. subst z rest.
  unfold encode_u64z.
  rewrite N2Z.id.
  apply encode_decode_u64.
  exact Hd.
Qed.

Definition bytes32_ok (h : bytes32) : Prop :=
  List.length h = 32 /\ bytes_ok h.

Fixpoint bytes_ok_bool (xs : byte_list) : bool :=
  match xs with
  | nil => true
  | b :: rest => andb (b <? byte_base) (bytes_ok_bool rest)
  end.

Lemma bytes_ok_bool_true :
  forall xs, bytes_ok_bool xs = true -> bytes_ok xs.
Proof.
  induction xs as [|b rest IH]; simpl; intros H.
  - exact I.
  - apply andb_true_iff in H.
    destruct H as [Hb Hr].
    apply Nat.ltb_lt in Hb.
    split; [exact Hb | apply IH; exact Hr].
Qed.

Lemma bytes_ok_bool_spec :
  forall xs, bytes_ok xs -> bytes_ok_bool xs = true.
Proof.
  induction xs as [|b rest IH]; simpl; intros H.
  - reflexivity.
  - destruct H as [Hb Hr].
    apply Nat.ltb_lt in Hb.
    rewrite Hb.
    apply IH.
    exact Hr.
Qed.

Definition script_ok (s : bytes) : Prop :=
  bytes_ok s /\ compactsize_ok (N.of_nat (List.length s)).

Definition encode_script (s : bytes) : byte_list :=
  encode_compactsize (N.of_nat (List.length s)) ++ s.

Lemma enc_script_nonempty : forall s, encode_script s <> nil.
Proof.
  intros s H. unfold encode_script in H.
  apply app_eq_nil in H. destruct H as [H _].
  exact (encode_compactsize_nonempty _ H).
Qed.

Definition decode_script (bs : byte_list) : option (bytes * byte_list) :=
  match decode_compactsize bs with
  | None => None
  | Some (n, rest) =>
      match take_n_N n rest with
      | None => None
      | Some (s, rest') =>
          if bytes_ok_bool s then Some (s, rest') else None
      end
  end.

Lemma decode_encode_script_app :
  forall s rest,
    script_ok s ->
    decode_script (encode_script s ++ rest) = Some (s, rest).
Proof.
  intros s rest [Hok Hlen].
  replace (encode_script s ++ rest)
    with (encode_compactsize (N.of_nat (List.length s)) ++ s ++ rest).
  2: { unfold encode_script. rewrite <- app_assoc. reflexivity. }
  unfold decode_script.
  rewrite (decode_encode_compactsize_app (N.of_nat (List.length s)) (s ++ rest) Hlen).
  rewrite take_n_N_spec, Nat2N.id, take_n_app.
  rewrite (bytes_ok_bool_spec s Hok).
  reflexivity.
Qed.

Lemma encode_decode_script :
  forall bs s rest,
    decode_script bs = Some (s, rest) ->
    encode_script s ++ rest = bs.
Proof.
  intros bs s rest H.
  unfold decode_script in H.
  destruct (decode_compactsize bs) as [[n rest0]|] eqn:Hc; [|discriminate].
  rewrite take_n_N_spec in H.
  destruct (take_n (N.to_nat n) rest0) as [[s0 rest']|] eqn:Ht; [|discriminate].
  destruct (bytes_ok_bool s0) eqn:Hb; [|discriminate].
  injection H as Hs Hr. subst s rest.
  unfold encode_script.
  pose proof (encode_decode_compactsize bs n rest0 Hc) as Hcs.
  pose proof (take_n_split (N.to_nat n) rest0 s0 rest' Ht) as Hsp.
  pose proof (take_n_length (N.to_nat n) rest0 s0 rest' Ht) as Hln.
  assert (Hn : N.of_nat (List.length s0) = n) by (rewrite Hln; apply N2Nat.id).
  subst rest0.
  rewrite <- app_assoc, <- Hcs.
  (* [exact] rather than [rewrite]: the length here is at [bytes] and
     Hn is at [byte_list], convertible but not syntactically equal. *)
  f_equal. f_equal. exact Hn.
Qed.

Definition outpoint_ok (o : OutPoint) : Prop :=
  bytes32_ok (op_hash o) /\ (op_index o < two32)%N.

Definition encode_outpoint (o : OutPoint) : byte_list :=
  op_hash o ++ encode_u32 (op_index o).

Definition decode_outpoint (bs : byte_list) : option (OutPoint * byte_list) :=
  match take_n 32 bs with
  | None => None
  | Some (h, rest) =>
      if bytes_ok_bool h then
        match decode_u32 rest with
        | None => None
        | Some (idx, rest') =>
            Some ({| op_hash := h; op_index := idx |}, rest')
        end
      else None
  end.

Lemma decode_encode_outpoint_app :
  forall o rest,
    outpoint_ok o ->
    decode_outpoint (encode_outpoint o ++ rest) = Some (o, rest).
Proof.
  intros o rest [[Hlen Hok] Hidx].
  replace (encode_outpoint o ++ rest)
    with (op_hash o ++ encode_u32 (op_index o) ++ rest).
  2: { unfold encode_outpoint. rewrite <- app_assoc. reflexivity. }
  unfold decode_outpoint.
  rewrite <- Hlen.
  rewrite take_n_app.
  rewrite (bytes_ok_bool_spec (op_hash o) Hok).
  rewrite (decode_encode_u32_app (op_index o) rest Hidx).
  destruct o as [h i]. simpl in *.
  reflexivity.
Qed.

Lemma encode_decode_outpoint :
  forall bs o rest,
    decode_outpoint bs = Some (o, rest) ->
    encode_outpoint o ++ rest = bs.
Proof.
  intros bs o rest H.
  unfold decode_outpoint in H.
  destruct (take_n 32 bs) as [[h rest0]|] eqn:Ht; [|discriminate].
  destruct (bytes_ok_bool h) eqn:Hb; [|discriminate].
  destruct (decode_u32 rest0) as [[idx rest']|] eqn:Hu; [|discriminate].
  injection H as Ho Hr. subst o rest.
  unfold encode_outpoint. simpl.
  pose proof (take_n_split 32 bs h rest0 Ht) as Hsp.
  pose proof (encode_decode_u32 rest0 idx rest' Hu) as Hu2.
  subst bs rest0.
  rewrite <- app_assoc.
  reflexivity.
Qed.

Definition input_ok (i : TxInput) : Prop :=
  outpoint_ok (previous_outpoint i) /\
  script_ok (script_data i) /\
  z_u32 (sequence i).

Definition encode_input (i : TxInput) : byte_list :=
  encode_outpoint (previous_outpoint i) ++
  encode_script (script_data i) ++
  encode_u32z (sequence i).

Lemma enc_input_nonempty : forall i, encode_input i <> nil.
Proof.
  intros i H. unfold encode_input in H.
  apply app_eq_nil in H. destruct H as [_ H].
  apply app_eq_nil in H. destruct H as [H _].
  exact (enc_script_nonempty _ H).
Qed.

Definition decode_input (bs : byte_list) : option (TxInput * byte_list) :=
  match decode_outpoint bs with
  | None => None
  | Some (op, rest) =>
      match decode_script rest with
      | None => None
      | Some (sc, rest1) =>
          match decode_u32z rest1 with
          | None => None
          | Some (seq, rest2) =>
              Some ({| previous_outpoint := op; script_data := sc; sequence := seq |}, rest2)
          end
      end
  end.

Lemma decode_encode_input_app :
  forall i rest,
    input_ok i ->
    decode_input (encode_input i ++ rest) = Some (i, rest).
Proof.
  intros i rest [Hop [Hsc Hseq]].
  unfold decode_input, encode_input.
  rewrite <- app_assoc.
  rewrite (decode_encode_outpoint_app (previous_outpoint i) _ Hop).
  rewrite <- app_assoc.
  rewrite (decode_encode_script_app (script_data i) _ Hsc).
  rewrite (decode_encode_u32z_app (sequence i) rest Hseq).
  destruct i as [op sc seq]. reflexivity.
Qed.

Lemma encode_decode_input :
  forall bs i rest,
    decode_input bs = Some (i, rest) ->
    encode_input i ++ rest = bs.
Proof.
  intros bs i rest H.
  unfold decode_input in H.
  destruct (decode_outpoint bs) as [[op rest0]|] eqn:Ho; [|discriminate].
  destruct (decode_script rest0) as [[sc rest1]|] eqn:Hs; [|discriminate].
  destruct (decode_u32z rest1) as [[seq rest2]|] eqn:Hz; [|discriminate].
  injection H as Hi Hr. subst i rest.
  unfold encode_input. simpl.
  rewrite <- (encode_decode_outpoint bs op rest0 Ho).
  rewrite <- (encode_decode_script rest0 sc rest1 Hs).
  rewrite <- (encode_decode_u32z rest1 seq rest2 Hz).
  rewrite <- app_assoc.
  rewrite <- app_assoc.
  reflexivity.
Qed.

Definition output_ok (o : TxOutput) : Prop :=
  z_u64 (amount o) /\ script_ok (script_pubkey o).

Definition encode_output (o : TxOutput) : byte_list :=
  encode_u64z (amount o) ++ encode_script (script_pubkey o).

Lemma enc_output_nonempty : forall o, encode_output o <> nil.
Proof.
  intros o H. unfold encode_output in H.
  apply app_eq_nil in H. destruct H as [_ H].
  exact (enc_script_nonempty _ H).
Qed.

Definition decode_output (bs : byte_list) : option (TxOutput * byte_list) :=
  match decode_u64z bs with
  | None => None
  | Some (amt, rest) =>
      match decode_script rest with
      | None => None
      | Some (pk, rest') =>
          Some ({| amount := amt; script_pubkey := pk |}, rest')
      end
  end.

Lemma decode_encode_output_app :
  forall o rest,
    output_ok o ->
    decode_output (encode_output o ++ rest) = Some (o, rest).
Proof.
  intros o rest [Ha Hs].
  unfold decode_output, encode_output.
  rewrite <- app_assoc.
  rewrite (decode_encode_u64z_app (amount o) _ Ha).
  rewrite (decode_encode_script_app (script_pubkey o) rest Hs).
  destruct o as [amt pk]. reflexivity.
Qed.

Lemma encode_decode_output :
  forall bs o rest,
    decode_output bs = Some (o, rest) ->
    encode_output o ++ rest = bs.
Proof.
  intros bs o rest H.
  unfold decode_output in H.
  destruct (decode_u64z bs) as [[amt rest0]|] eqn:Ha; [|discriminate].
  destruct (decode_script rest0) as [[pk rest']|] eqn:Hs; [|discriminate].
  injection H as Ho Hr. subst o rest.
  unfold encode_output. simpl.
  rewrite <- (encode_decode_u64z bs amt rest0 Ha).
  rewrite <- (encode_decode_script rest0 pk rest' Hs).
  reflexivity.
Qed.

Section Vec.
Variable A : Type.
Variable enc : A -> byte_list.
Variable dec : byte_list -> option (A * byte_list).
Variable ok : A -> Prop.
Hypothesis dec_enc : forall x rest, ok x -> dec (enc x ++ rest) = Some (x, rest).
Hypothesis enc_dec : forall bs x rest, dec bs = Some (x, rest) -> enc x ++ rest = bs.
Hypothesis enc_nonempty : forall x, enc x <> nil.

Fixpoint encode_vec (xs : list A) : byte_list :=
  match xs with
  | nil => nil
  | x :: t => enc x ++ encode_vec t
  end.

Fixpoint decode_vec (n : nat) (bs : byte_list) : option (list A * byte_list) :=
  match n with
  | 0 => Some (nil, bs)
  | S n' =>
      match dec bs with
      | None => None
      | Some (x, rest) =>
          match decode_vec n' rest with
          | None => None
          | Some (xs, rest') => Some (x :: xs, rest')
          end
      end
  end.

Lemma decode_encode_vec_app :
  forall xs rest,
    Forall ok xs ->
    decode_vec (List.length xs) (encode_vec xs ++ rest) = Some (xs, rest).
Proof.
  induction xs as [|x t IH]; intros rest Hall; simpl.
  - reflexivity.
  - inversion Hall as [|? ? Hx Ht]. subst.
    rewrite <- app_assoc.
    rewrite (dec_enc x (encode_vec t ++ rest) Hx).
    rewrite (IH rest Ht).
    reflexivity.
Qed.

Lemma encode_decode_vec :
  forall n bs xs rest,
    decode_vec n bs = Some (xs, rest) ->
    encode_vec xs ++ rest = bs /\ List.length xs = n.
Proof.
  induction n as [|n IH]; intros bs xs rest H; simpl in H.
  - injection H as Hxs Hr. subst xs rest. split; reflexivity.
  - destruct (dec bs) as [[x rest0]|] eqn:Hd; [|discriminate].
    destruct (decode_vec n rest0) as [[t rest']|] eqn:Ht; [|discriminate].
    injection H as Hxs Hr. subst xs rest.
    destruct (IH rest0 t rest' Ht) as [He Hlen].
    split.
    + simpl. rewrite <- (enc_dec bs x rest0 Hd). rewrite <- He. rewrite <- app_assoc. reflexivity.
    + simpl. rewrite Hlen. reflexivity.
Qed.

(** Every element costs at least one byte, so a vector cannot report
    more elements than the bytes behind it. That is what lets the
    guarded decoder below reject an oversized count without first
    materialising it as a unary index. *)

Lemma dec_consumes :
  forall bs x rest, dec bs = Some (x, rest) -> List.length rest < List.length bs.
Proof.
  intros bs x rest H.
  pose proof (enc_dec bs x rest H) as He.
  rewrite <- He, app_length.
  destruct (enc x) as [|b l] eqn:E; [exfalso; exact (enc_nonempty x E)|].
  cbn [List.length]. lia.
Qed.

Theorem decode_vec_none_short :
  forall n bs, List.length bs < n -> decode_vec n bs = None.
Proof.
  induction n as [|n IH]; intros bs Hlen; [exfalso; lia|].
  cbn [decode_vec].
  destruct (dec bs) as [[x rest]|] eqn:Hd; [|reflexivity].
  pose proof (dec_consumes bs x rest Hd) as Hc.
  rewrite (IH rest ltac:(lia)). reflexivity.
Qed.

End Vec.

Definition decode_vec_N (A : Type) (dec : byte_list -> option (A * byte_list))
                        (n : N) (bs : byte_list) : option (list A * byte_list) :=
  if (N.of_nat (List.length bs) <? n)%N then None
  else decode_vec A dec (N.to_nat n) bs.

Theorem decode_vec_N_spec :
  forall (A : Type) (enc : A -> byte_list) (dec : byte_list -> option (A * byte_list)),
    (forall bs x rest, dec bs = Some (x, rest) -> enc x ++ rest = bs) ->
    (forall x, enc x <> nil) ->
    forall n bs, decode_vec_N A dec n bs = decode_vec A dec (N.to_nat n) bs.
Proof.
  intros A enc dec Hed Hne n bs. unfold decode_vec_N.
  destruct (N.of_nat (List.length bs) <? n)%N eqn:E; [|reflexivity].
  apply N.ltb_lt in E.
  symmetry. apply (decode_vec_none_short A enc dec Hed Hne). lia.
Qed.

Definition stack_ok (st : list bytes) : Prop :=
  Forall script_ok st /\ compactsize_ok (N.of_nat (List.length st)).

Definition encode_stack (st : list bytes) : byte_list :=
  encode_compactsize (N.of_nat (List.length st)) ++ encode_vec bytes encode_script st.

Definition decode_stack (bs : byte_list) : option (list bytes * byte_list) :=
  match decode_compactsize bs with
  | None => None
  | Some (n, rest) => decode_vec_N bytes decode_script n rest
  end.

Lemma dec_enc_script :
  forall s rest, script_ok s -> decode_script (encode_script s ++ rest) = Some (s, rest).
Proof.
  apply decode_encode_script_app.
Qed.

Lemma enc_dec_script :
  forall bs s rest, decode_script bs = Some (s, rest) -> encode_script s ++ rest = bs.
Proof.
  apply encode_decode_script.
Qed.

Lemma decode_encode_stack_app :
  forall st rest,
    stack_ok st ->
    decode_stack (encode_stack st ++ rest) = Some (st, rest).
Proof.
  intros st rest [Hall Hlen].
  unfold decode_stack, encode_stack.
  rewrite <- app_assoc.
  rewrite (decode_encode_compactsize_app (N.of_nat (List.length st)) _ Hlen).
  rewrite (decode_vec_N_spec bytes encode_script decode_script
             enc_dec_script enc_script_nonempty).
  rewrite Nat2N.id.
  apply (decode_encode_vec_app bytes encode_script decode_script script_ok
           dec_enc_script st rest Hall).
Qed.

Lemma encode_decode_stack :
  forall bs st rest,
    decode_stack bs = Some (st, rest) ->
    encode_stack st ++ rest = bs.
Proof.
  intros bs st rest H.
  unfold decode_stack in H.
  destruct (decode_compactsize bs) as [[n rest0]|] eqn:Hc; [|discriminate].
  rewrite (decode_vec_N_spec bytes encode_script decode_script
             enc_dec_script enc_script_nonempty) in H.
  pose proof (encode_decode_vec bytes encode_script decode_script enc_dec_script
                (N.to_nat n) rest0 st rest H) as Hv.
  destruct Hv as [He Hlen].
  unfold encode_stack.
  assert (Hn : N.of_nat (List.length st) = n) by (rewrite Hlen; apply N2Nat.id).
  rewrite Hn.
  rewrite <- (encode_decode_compactsize bs n rest0 Hc).
  rewrite <- He.
  rewrite <- app_assoc.
  reflexivity.
Qed.

Definition witness_ok (w : Witness) : Prop := stack_ok (stack w).

Definition encode_witness (w : Witness) : byte_list := encode_stack (stack w).

Lemma enc_witness_nonempty : forall w, encode_witness w <> nil.
Proof.
  intros w H. unfold encode_witness, encode_stack in H.
  apply app_eq_nil in H. destruct H as [H _].
  exact (encode_compactsize_nonempty _ H).
Qed.

Definition decode_witness (bs : byte_list) : option (Witness * byte_list) :=
  match decode_stack bs with
  | None => None
  | Some (st, rest) => Some ({| stack := st |}, rest)
  end.

Lemma decode_encode_witness_app :
  forall w rest,
    witness_ok w ->
    decode_witness (encode_witness w ++ rest) = Some (w, rest).
Proof.
  intros w rest Hok.
  unfold decode_witness, encode_witness.
  rewrite (decode_encode_stack_app (stack w) rest Hok).
  destruct w as [st]. reflexivity.
Qed.

Lemma encode_decode_witness :
  forall bs w rest,
    decode_witness bs = Some (w, rest) ->
    encode_witness w ++ rest = bs.
Proof.
  intros bs w rest H.
  unfold decode_witness in H.
  destruct (decode_stack bs) as [[st rest']|] eqn:Hd; [|discriminate].
  injection H as Hw Hr. subst w rest.
  unfold encode_witness. simpl.
  apply encode_decode_stack.
  exact Hd.
Qed.

Fixpoint witness_has_data (ws : list Witness) : bool :=
  match ws with
  | nil => false
  | w :: rest =>
      match stack w with
      | nil => witness_has_data rest
      | _ => true
      end
  end.

Definition has_wit (tx : Transaction) : bool :=
  match witness tx with
  | nil => false
  | _ => true
  end.

Definition transaction_ok (tx : Transaction) : Prop :=
  z_u32 (version tx) /\
  z_u32 (locktime tx) /\
  inputs tx <> nil /\
  outputs tx <> nil /\
  Forall input_ok (inputs tx) /\
  Forall output_ok (outputs tx) /\
  compactsize_ok (N.of_nat (List.length (inputs tx))) /\
  compactsize_ok (N.of_nat (List.length (outputs tx))) /\
  (witness tx = nil \/
   (List.length (witness tx) = List.length (inputs tx) /\
    Forall witness_ok (witness tx) /\
    witness_has_data (witness tx) = true /\
    compactsize_ok (N.of_nat (List.length (witness tx))))).

Definition encode_transaction (tx : Transaction) : byte_list :=
  encode_u32z (version tx) ++
  (if has_wit tx then (0 :: 1 :: nil) else nil) ++
  encode_compactsize (N.of_nat (List.length (inputs tx))) ++
  encode_vec TxInput encode_input (inputs tx) ++
  encode_compactsize (N.of_nat (List.length (outputs tx))) ++
  encode_vec TxOutput encode_output (outputs tx) ++
  (if has_wit tx
   then encode_vec Witness encode_witness (witness tx)
   else nil) ++
  encode_u32z (locktime tx).

Definition decode_legacy_tail (ver : Z) (n_in : N) (bs : byte_list)
  : option (Transaction * byte_list) :=
  match decode_vec_N TxInput decode_input n_in bs with
  | None => None
  | Some (ins, rest) =>
      match decode_compactsize rest with
      | None => None
      | Some (n_out, rest1) =>
          match decode_vec_N TxOutput decode_output n_out rest1 with
          | None => None
          | Some (outs, rest2) =>
              match decode_u32z rest2 with
              | None => None
              | Some (lt, rest3) =>
                  Some ({| version := ver;
                           inputs := ins;
                           outputs := outs;
                           locktime := lt;
                           witness := nil |}, rest3)
              end
          end
      end
  end.

Definition decode_witness_tail (ver : Z) (bs : byte_list)
  : option (Transaction * byte_list) :=
  match decode_compactsize bs with
  | None => None
  | Some (n_in, rest) =>
      match decode_vec_N TxInput decode_input n_in rest with
      | None => None
      | Some (ins, rest1) =>
          match decode_compactsize rest1 with
          | None => None
          | Some (n_out, rest2) =>
              match decode_vec_N TxOutput decode_output n_out rest2 with
              | None => None
              | Some (outs, rest3) =>
                  match decode_vec Witness decode_witness
                                   (List.length ins) rest3 with
                  | None => None
                  | Some (ws, rest4) =>
                      match decode_u32z rest4 with
                      | None => None
                      | Some (lt, rest5) =>
                          if witness_has_data ws then
                            Some ({| version := ver;
                                     inputs := ins;
                                     outputs := outs;
                                     locktime := lt;
                                     witness := ws |}, rest5)
                          else None
                      end
                  end
              end
          end
      end
  end.

Definition decode_transaction_from (bs : byte_list)
  : option (Transaction * byte_list) :=
  match decode_u32z bs with
  | None => None
  | Some (ver, rest) =>
      match decode_compactsize rest with
      | None => None
      | Some (n, rest1) =>
          if (n =? 0)%N then
            match rest1 with
            | 1 :: rest2 => decode_witness_tail ver rest2
            | _ => None
            end
          else decode_legacy_tail ver n rest1
      end
  end.

Definition decode_transaction (bs : byte_list) : option Transaction :=
  match decode_transaction_from bs with
  | Some (tx, nil) => Some tx
  | _ => None
  end.

Lemma dec_enc_input :
  forall i rest, input_ok i -> decode_input (encode_input i ++ rest) = Some (i, rest).
Proof. apply decode_encode_input_app. Qed.

Lemma enc_dec_input :
  forall bs i rest, decode_input bs = Some (i, rest) -> encode_input i ++ rest = bs.
Proof. apply encode_decode_input. Qed.

Lemma dec_enc_output :
  forall o rest, output_ok o -> decode_output (encode_output o ++ rest) = Some (o, rest).
Proof. apply decode_encode_output_app. Qed.

Lemma enc_dec_output :
  forall bs o rest, decode_output bs = Some (o, rest) -> encode_output o ++ rest = bs.
Proof. apply encode_decode_output. Qed.

Lemma dec_enc_witness :
  forall w rest, witness_ok w -> decode_witness (encode_witness w ++ rest) = Some (w, rest).
Proof. apply decode_encode_witness_app. Qed.

Lemma enc_dec_witness :
  forall bs w rest, decode_witness bs = Some (w, rest) -> encode_witness w ++ rest = bs.
Proof. apply encode_decode_witness. Qed.

Lemma decode_cs_zero :
  forall rest, decode_compactsize (0 :: rest) = Some (0%N, rest).
Proof.
  intros rest. reflexivity.
Qed.

Lemma encode_cs_0 : encode_compactsize 0%N = [0].
Proof.
  unfold encode_compactsize.
  reflexivity.
Qed.

Lemma compactsize_ok_0 : compactsize_ok 0%N.
Proof.
  unfold compactsize_ok, two64. lia.
Qed.

Lemma nonempty_length_nz :
  forall (A : Type) (xs : list A),
    xs <> nil -> (N.of_nat (List.length xs) =? 0)%N = false.
Proof.
  intros A xs H.
  destruct xs as [|x l]; [exfalso; apply H; reflexivity|].
  apply N.eqb_neq. cbn [List.length]. lia.
Qed.

Lemma decode_legacy_tail_ok :
  forall ver ins outs lt rest,
    Forall input_ok ins ->
    Forall output_ok outs ->
    compactsize_ok (N.of_nat (List.length outs)) ->
    z_u32 lt ->
    decode_legacy_tail ver (N.of_nat (List.length ins))
      (encode_vec TxInput encode_input ins ++
       encode_compactsize (N.of_nat (List.length outs)) ++
       encode_vec TxOutput encode_output outs ++
       encode_u32z lt ++ rest)
    = Some ({| version := ver; inputs := ins; outputs := outs;
               locktime := lt; witness := nil |}, rest).
Proof.
  intros ver ins outs lt rest Hino Houto Houtc Hlt.
  unfold decode_legacy_tail.
  rewrite (decode_vec_N_spec TxInput encode_input decode_input
             enc_dec_input enc_input_nonempty), Nat2N.id.
  rewrite (decode_encode_vec_app TxInput encode_input decode_input input_ok
             dec_enc_input ins _ Hino).
  rewrite (decode_encode_compactsize_app (N.of_nat (List.length outs)) _ Houtc).
  rewrite (decode_vec_N_spec TxOutput encode_output decode_output
             enc_dec_output enc_output_nonempty), Nat2N.id.
  rewrite (decode_encode_vec_app TxOutput encode_output decode_output output_ok
             dec_enc_output outs _ Houto).
  rewrite (decode_encode_u32z_app lt rest Hlt).
  reflexivity.
Qed.

Lemma decode_witness_tail_ok :
  forall ver ins outs ws lt rest,
    Forall input_ok ins ->
    Forall output_ok outs ->
    Forall witness_ok ws ->
    compactsize_ok (N.of_nat (List.length ins)) ->
    compactsize_ok (N.of_nat (List.length outs)) ->
    z_u32 lt ->
    List.length ws = List.length ins ->
    witness_has_data ws = true ->
    decode_witness_tail ver
      (encode_compactsize (N.of_nat (List.length ins)) ++
       encode_vec TxInput encode_input ins ++
       encode_compactsize (N.of_nat (List.length outs)) ++
       encode_vec TxOutput encode_output outs ++
       encode_vec Witness encode_witness ws ++
       encode_u32z lt ++ rest)
    = Some ({| version := ver; inputs := ins; outputs := outs;
               locktime := lt; witness := ws |}, rest).
Proof.
  intros ver ins outs ws lt rest Hino Houto Hwok Hinc Houtc Hlt Hlen Hdata.
  unfold decode_witness_tail.
  rewrite (decode_encode_compactsize_app (N.of_nat (List.length ins)) _ Hinc).
  rewrite (decode_vec_N_spec TxInput encode_input decode_input
             enc_dec_input enc_input_nonempty), Nat2N.id.
  rewrite (decode_encode_vec_app TxInput encode_input decode_input input_ok
             dec_enc_input ins _ Hino).
  rewrite (decode_encode_compactsize_app (N.of_nat (List.length outs)) _ Houtc).
  rewrite (decode_vec_N_spec TxOutput encode_output decode_output
             enc_dec_output enc_output_nonempty), Nat2N.id.
  rewrite (decode_encode_vec_app TxOutput encode_output decode_output output_ok
             dec_enc_output outs _ Houto).
  rewrite <- Hlen.
  rewrite (decode_encode_vec_app Witness encode_witness decode_witness witness_ok
             dec_enc_witness ws _ Hwok).
  rewrite (decode_encode_u32z_app lt rest Hlt).
  rewrite Hdata.
  reflexivity.
Qed.

Lemma decode_encode_transaction_from :
  forall tx rest,
    transaction_ok tx ->
    decode_transaction_from (encode_transaction tx ++ rest) = Some (tx, rest).
Proof.
  intros tx rest Hok.
  destruct Hok as [Hv [Hlt [Hinn [Houtn [Hino [Houto [Hinc [Houtc Hw]]]]]]]].
  unfold decode_transaction_from, encode_transaction.
  rewrite <- app_assoc.
  rewrite (decode_encode_u32z_app (version tx) _ Hv).
  destruct (has_wit tx) eqn:Hwit.
  - rewrite <- app_assoc.
    rewrite cons_app.
    rewrite cons_app.
    rewrite decode_cs_zero.
    rewrite N.eqb_refl.
    destruct (witness tx) as [|w0 wst] eqn:Hws.
    + unfold has_wit in Hwit. rewrite Hws in Hwit. discriminate.
    + destruct Hw as [Hnil | [Hlen [Hwok [Hdata Hwc]]]].
      * rewrite Hnil in Hws. discriminate.
      * rewrite <- app_assoc.
        rewrite <- Hws in Hlen.
        rewrite <- Hws in Hwok.
        rewrite <- Hws in Hdata.
        rewrite app_nil_l.
        repeat rewrite <- app_assoc.
        rewrite <- Hws.
        rewrite (decode_witness_tail_ok (version tx) (inputs tx) (outputs tx)
                   (witness tx) (locktime tx) rest Hino Houto Hwok Hinc Houtc Hlt Hlen Hdata).
        destruct tx as [ver ins outs lt wit].
        reflexivity.
  - rewrite app_nil_l.
    rewrite <- app_assoc.
    rewrite (decode_encode_compactsize_app (N.of_nat (List.length (inputs tx))) _ Hinc).
    rewrite (nonempty_length_nz TxInput (inputs tx) Hinn).
    rewrite app_nil_l.
    repeat rewrite <- app_assoc.
    rewrite (decode_legacy_tail_ok (version tx) (inputs tx) (outputs tx)
               (locktime tx) rest Hino Houto Houtc Hlt).
    destruct tx as [ver ins outs lt wit].
    simpl in *.
    unfold has_wit in Hwit.
    destruct wit; [|discriminate].
    reflexivity.
Qed.

Theorem decode_encode_transaction :
  forall tx,
    transaction_ok tx ->
    decode_transaction (encode_transaction tx) = Some tx.
Proof.
  intros tx Hok.
  unfold decode_transaction.
  pose proof (decode_encode_transaction_from tx nil Hok) as H.
  rewrite app_nil_r in H.
  rewrite H.
  reflexivity.
Qed.

Lemma encode_decode_transaction_from :
  forall bs tx rest,
    decode_transaction_from bs = Some (tx, rest) ->
    encode_transaction tx ++ rest = bs.
Proof.
  intros bs tx rest H.
  unfold decode_transaction_from in H.
  destruct (decode_u32z bs) as [[ver restv]|] eqn:Hv; [|discriminate].
  destruct (decode_compactsize restv) as [[n rest1]|] eqn:Hc; [|discriminate].
  destruct (n =? 0)%N eqn:Hz.
  - destruct rest1 as [|b rest2]; [discriminate|].
    destruct b as [|b']; [discriminate|].
    destruct b'; [|discriminate].
    unfold decode_witness_tail in H.
    destruct (decode_compactsize rest2) as [[n_in rest3]|] eqn:Hin; [|discriminate].
    rewrite (decode_vec_N_spec TxInput encode_input decode_input
               enc_dec_input enc_input_nonempty) in H.
    destruct (decode_vec TxInput decode_input (N.to_nat n_in) rest3)
      as [[ins rest4]|] eqn:Hins; [|discriminate].
    destruct (decode_compactsize rest4) as [[n_out rest5]|] eqn:Hout; [|discriminate].
    rewrite (decode_vec_N_spec TxOutput encode_output decode_output
               enc_dec_output enc_output_nonempty) in H.
    destruct (decode_vec TxOutput decode_output (N.to_nat n_out) rest5)
      as [[outs rest6]|] eqn:Houts; [|discriminate].
    destruct (decode_vec Witness decode_witness (List.length ins) rest6)
      as [[ws rest7]|] eqn:Hws; [|discriminate].
    destruct (decode_u32z rest7) as [[lt rest8]|] eqn:Hlt; [|discriminate].
    destruct (witness_has_data ws) eqn:Hd; [|discriminate].
    injection H as Htx Hr. subst tx rest.
    destruct ws as [|w0 ws']; [simpl in Hd; discriminate|].
    unfold encode_transaction.
    cbn [has_wit version inputs outputs locktime witness].
    destruct (encode_decode_vec TxInput encode_input decode_input enc_dec_input
                (N.to_nat n_in) rest3 ins rest4 Hins) as [Eins Lin].
    destruct (encode_decode_vec TxOutput encode_output decode_output enc_dec_output
                (N.to_nat n_out) rest5 outs rest6 Houts) as [Eouts Lout].
    destruct (encode_decode_vec Witness encode_witness decode_witness enc_dec_witness
                (List.length ins) rest6 (w0 :: ws') rest7 Hws) as [Ews Lws].
    apply N.eqb_eq in Hz. subst n.
    rewrite <- (encode_decode_u32z bs ver restv Hv).
    rewrite <- (encode_decode_compactsize restv 0%N (1 :: rest2) Hc).
    rewrite <- (encode_decode_compactsize rest2 n_in rest3 Hin).
    rewrite <- Eins.
    rewrite <- (encode_decode_compactsize rest4 n_out rest5 Hout).
    rewrite <- Eouts.
    rewrite <- Ews.
    rewrite <- (encode_decode_u32z rest7 lt rest8 Hlt).
    rewrite Lin, Lout, !N2Nat.id.
    rewrite encode_cs_0.
    change ([0] ++ 1 :: encode_compactsize n_in ++
              encode_vec TxInput encode_input ins ++
              encode_compactsize n_out ++
              encode_vec TxOutput encode_output outs ++
              encode_vec Witness encode_witness (w0 :: ws') ++
              encode_u32z lt ++ rest8)
      with ([0; 1] ++
              encode_compactsize n_in ++
              encode_vec TxInput encode_input ins ++
              encode_compactsize n_out ++
              encode_vec TxOutput encode_output outs ++
              encode_vec Witness encode_witness (w0 :: ws') ++
              encode_u32z lt ++ rest8).
    do 7 (rewrite <- app_assoc).
    reflexivity.
  - unfold decode_legacy_tail in H.
    rewrite (decode_vec_N_spec TxInput encode_input decode_input
               enc_dec_input enc_input_nonempty) in H.
    destruct (decode_vec TxInput decode_input (N.to_nat n) rest1)
      as [[ins rest3]|] eqn:Hins; [|discriminate].
    destruct (decode_compactsize rest3) as [[n_out rest4]|] eqn:Hout; [|discriminate].
    rewrite (decode_vec_N_spec TxOutput encode_output decode_output
               enc_dec_output enc_output_nonempty) in H.
    destruct (decode_vec TxOutput decode_output (N.to_nat n_out) rest4)
      as [[outs rest5]|] eqn:Houts; [|discriminate].
    destruct (decode_u32z rest5) as [[lt rest6]|] eqn:Hlt; [|discriminate].
    injection H as Htx Hr. subst tx rest.
    unfold encode_transaction.
    cbn [has_wit version inputs outputs locktime witness].
    destruct (encode_decode_vec TxInput encode_input decode_input enc_dec_input
                (N.to_nat n) rest1 ins rest3 Hins) as [Eins Lin].
    destruct (encode_decode_vec TxOutput encode_output decode_output enc_dec_output
                (N.to_nat n_out) rest4 outs rest5 Houts) as [Eouts Lout].
    rewrite <- (encode_decode_u32z bs ver restv Hv).
    rewrite <- (encode_decode_compactsize restv n rest1 Hc).
    rewrite <- Eins.
    rewrite <- (encode_decode_compactsize rest3 n_out rest4 Hout).
    rewrite <- Eouts.
    rewrite <- (encode_decode_u32z rest5 lt rest6 Hlt).
    rewrite Lin, Lout, !N2Nat.id.
    rewrite app_nil_l.
    rewrite app_nil_l.
    do 5 (rewrite <- app_assoc).
    reflexivity.
Qed.

Theorem encode_decode_transaction_canonical :
  forall bs tx,
    decode_transaction bs = Some tx ->
    encode_transaction tx = bs.
Proof.
  intros bs tx H.
  unfold decode_transaction in H.
  destruct (decode_transaction_from bs) as [[tx' rest]|] eqn:Hd; [|discriminate].
  destruct rest; [|discriminate].
  injection H as Htx. subst tx'.
  pose proof (encode_decode_transaction_from bs tx nil Hd) as He.
  rewrite app_nil_r in He.
  exact He.
Qed.

Theorem canonical_encoding_unique :
  forall tx1 tx2,
    transaction_ok tx1 ->
    transaction_ok tx2 ->
    encode_transaction tx1 = encode_transaction tx2 ->
    tx1 = tx2.
Proof.
  intros tx1 tx2 H1 H2 Heq.
  pose proof (decode_encode_transaction tx1 H1) as D1.
  pose proof (decode_encode_transaction tx2 H2) as D2.
  rewrite Heq in D1.
  rewrite D1 in D2.
  injection D2 as Htx.
  exact Htx.
Qed.

Lemma enc_transaction_nonempty : forall tx, encode_transaction tx <> nil.
Proof.
  intros tx H. unfold encode_transaction in H.
  apply app_eq_nil in H. destruct H as [H _].
  unfold encode_u32z in H. exact (encode_u32_nonempty _ H).
Qed.

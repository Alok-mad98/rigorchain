(** ConsensusModel.BlockCodec

    Byte codec for BlockModel. Header + CompactSize + transactions.
    Not C++ Unserialize. Not DecodeHexBlk. Not extraction.

    CPP_KERNELINPUT_EQUIVALENCE = NOT PROVED
*)
From Coq Require Import Arith.PeanoNat.
From Coq Require Import Bool.Bool.
From Coq Require Import List.
From Coq Require Import ZArith.ZArith.
From Coq Require Import Lia.
From Coq Require Import NArith.NArith.
From ConsensusModel Require Import ByteModel.
From ConsensusModel Require Import CompactSizeModel.
From ConsensusModel Require Import TransactionModel.
From ConsensusModel Require Import BlockModel.
From ConsensusModel Require Import TransactionCodec.
Import ListNotations.

Definition header_ok (h : BlockHeader) : Prop :=
  z_u32 (bh_version h) /\
  bytes32_ok (bh_prev_hash h) /\
  bytes32_ok (bh_merkle_root h) /\
  z_u32 (bh_timestamp h) /\
  z_u32 (bh_bits h) /\
  z_u32 (bh_nonce h).

Definition encode_header (h : BlockHeader) : byte_list :=
  encode_u32z (bh_version h) ++
  bh_prev_hash h ++
  bh_merkle_root h ++
  encode_u32z (bh_timestamp h) ++
  encode_u32z (bh_bits h) ++
  encode_u32z (bh_nonce h).

Definition decode_header (bs : byte_list) : option (BlockHeader * byte_list) :=
  match decode_u32z bs with
  | None => None
  | Some (ver, rest) =>
      match take_n 32 rest with
      | None => None
      | Some (prev, rest1) =>
          if bytes_ok_bool prev then
            match take_n 32 rest1 with
            | None => None
            | Some (merkle, rest2) =>
                if bytes_ok_bool merkle then
                  match decode_u32z rest2 with
                  | None => None
                  | Some (tm, rest3) =>
                      match decode_u32z rest3 with
                      | None => None
                      | Some (bits, rest4) =>
                          match decode_u32z rest4 with
                          | None => None
                          | Some (nonce, rest5) =>
                              Some ({| bh_version := ver;
                                       bh_prev_hash := prev;
                                       bh_merkle_root := merkle;
                                       bh_timestamp := tm;
                                       bh_bits := bits;
                                       bh_nonce := nonce |}, rest5)
                          end
                      end
                  end
                else None
            end
          else None
      end
  end.

Lemma encode_header_app_assoc :
  forall h rest,
    encode_header h ++ rest =
    encode_u32z (bh_version h) ++
    bh_prev_hash h ++
    bh_merkle_root h ++
    encode_u32z (bh_timestamp h) ++
    encode_u32z (bh_bits h) ++
    encode_u32z (bh_nonce h) ++ rest.
Proof.
  intros h rest.
  unfold encode_header.
  do 5 (rewrite <- app_assoc).
  reflexivity.
Qed.

Lemma decode_encode_header_app :
  forall h rest,
    header_ok h ->
    decode_header (encode_header h ++ rest) = Some (h, rest).
Proof.
  intros h rest [Hv [[Hplen Hpok] [[Hmlen Hmok] [Ht [Hb Hn]]]]].
  rewrite encode_header_app_assoc.
  unfold decode_header.
  rewrite (decode_encode_u32z_app (bh_version h) _ Hv).
  assert (Hp32 :
    take_n 32
      (bh_prev_hash h ++
       bh_merkle_root h ++
       encode_u32z (bh_timestamp h) ++
       encode_u32z (bh_bits h) ++
       encode_u32z (bh_nonce h) ++ rest)
    = Some (bh_prev_hash h,
            bh_merkle_root h ++
            encode_u32z (bh_timestamp h) ++
            encode_u32z (bh_bits h) ++
            encode_u32z (bh_nonce h) ++ rest)).
  { rewrite <- Hplen. apply take_n_app. }
  rewrite Hp32.
  rewrite (bytes_ok_bool_spec (bh_prev_hash h) Hpok).
  assert (Hm32 :
    take_n 32
      (bh_merkle_root h ++
       encode_u32z (bh_timestamp h) ++
       encode_u32z (bh_bits h) ++
       encode_u32z (bh_nonce h) ++ rest)
    = Some (bh_merkle_root h,
            encode_u32z (bh_timestamp h) ++
            encode_u32z (bh_bits h) ++
            encode_u32z (bh_nonce h) ++ rest)).
  { rewrite <- Hmlen. apply take_n_app. }
  rewrite Hm32.
  rewrite (bytes_ok_bool_spec (bh_merkle_root h) Hmok).
  rewrite (decode_encode_u32z_app (bh_timestamp h) _ Ht).
  rewrite (decode_encode_u32z_app (bh_bits h) _ Hb).
  rewrite (decode_encode_u32z_app (bh_nonce h) rest Hn).
  destruct h as [ver prev merkle tm bits nonce].
  reflexivity.
Qed.

Lemma encode_decode_header :
  forall bs h rest,
    decode_header bs = Some (h, rest) ->
    encode_header h ++ rest = bs.
Proof.
  intros bs h rest Hd.
  unfold decode_header in Hd.
  destruct (decode_u32z bs) as [[ver rest0]|] eqn:Hv; [|discriminate].
  destruct (take_n 32 rest0) as [[prev rest1]|] eqn:Hp; [|discriminate].
  destruct (bytes_ok_bool prev) eqn:Hpb; [|discriminate].
  destruct (take_n 32 rest1) as [[merkle rest2]|] eqn:Hm; [|discriminate].
  destruct (bytes_ok_bool merkle) eqn:Hmb; [|discriminate].
  destruct (decode_u32z rest2) as [[tm rest3]|] eqn:Ht; [|discriminate].
  destruct (decode_u32z rest3) as [[bits rest4]|] eqn:Hb; [|discriminate].
  destruct (decode_u32z rest4) as [[nonce rest5]|] eqn:Hn; [|discriminate].
  injection Hd as Hh Hr. subst h rest.
  rewrite encode_header_app_assoc.
  cbn [bh_version bh_prev_hash bh_merkle_root bh_timestamp bh_bits bh_nonce].
  rewrite <- (encode_decode_u32z bs ver rest0 Hv).
  rewrite (take_n_split 32 rest0 prev rest1 Hp).
  rewrite (take_n_split 32 rest1 merkle rest2 Hm).
  rewrite <- (encode_decode_u32z rest2 tm rest3 Ht).
  rewrite <- (encode_decode_u32z rest3 bits rest4 Hb).
  rewrite <- (encode_decode_u32z rest4 nonce rest5 Hn).
  reflexivity.
Qed.

Definition block_ok (b : Block) : Prop :=
  header_ok (header b) /\
  Forall transaction_ok (transactions b) /\
  compactsize_ok (N.of_nat (List.length (transactions b))).

Definition encode_block (b : Block) : byte_list :=
  encode_header (header b) ++
  encode_compactsize (N.of_nat (List.length (transactions b))) ++
  encode_vec Transaction encode_transaction (transactions b).

Definition decode_block_from (bs : byte_list) : option (Block * byte_list) :=
  match decode_header bs with
  | None => None
  | Some (h, rest) =>
      match decode_compactsize rest with
      | None => None
      | Some (n, rest1) =>
          match decode_vec_N Transaction decode_transaction_from n rest1 with
          | None => None
          | Some (txs, rest2) =>
              Some ({| header := h; transactions := txs |}, rest2)
          end
      end
  end.

Definition decode_block (bs : byte_list) : option Block :=
  match decode_block_from bs with
  | Some (b, nil) => Some b
  | _ => None
  end.

Lemma dec_enc_tx_from :
  forall tx rest,
    transaction_ok tx ->
    decode_transaction_from (encode_transaction tx ++ rest) = Some (tx, rest).
Proof.
  apply decode_encode_transaction_from.
Qed.

Lemma encode_block_app_assoc :
  forall b rest,
    encode_block b ++ rest =
    encode_header (header b) ++
    encode_compactsize (N.of_nat (List.length (transactions b))) ++
    encode_vec Transaction encode_transaction (transactions b) ++ rest.
Proof.
  intros b rest.
  unfold encode_block.
  do 2 (rewrite <- app_assoc).
  reflexivity.
Qed.

Lemma decode_encode_block_from :
  forall b rest,
    block_ok b ->
    decode_block_from (encode_block b ++ rest) = Some (b, rest).
Proof.
  intros b rest [Hh [Htx Hcs]].
  rewrite encode_block_app_assoc.
  unfold decode_block_from.
  rewrite (decode_encode_header_app (header b) _ Hh).
  rewrite (decode_encode_compactsize_app
             (N.of_nat (List.length (transactions b))) _ Hcs).
  rewrite (decode_vec_N_spec Transaction encode_transaction
             decode_transaction_from encode_decode_transaction_from
             enc_transaction_nonempty), Nat2N.id.
  rewrite (decode_encode_vec_app Transaction encode_transaction
             decode_transaction_from transaction_ok dec_enc_tx_from
             (transactions b) rest Htx).
  destruct b as [h txs].
  reflexivity.
Qed.

Theorem decode_encode_block :
  forall b,
    block_ok b ->
    decode_block (encode_block b) = Some b.
Proof.
  intros b Hok.
  unfold decode_block.
  pose proof (decode_encode_block_from b nil Hok) as H.
  rewrite app_nil_r in H.
  rewrite H.
  reflexivity.
Qed.

(** * Canonicality

    Everything above is the [encode] then [decode] direction: it says
    the codec does not lose a block. That direction is satisfied by
    codecs with serious defects, including ones that accept several
    distinct encodings of the same block.

    These are the other direction. Whatever the decoder accepts is
    already the canonical encoding of what it produced, so a block has
    exactly one accepted byte string and malleability at this layer is
    impossible. The transaction layer proves the same thing in
    [encode_decode_transaction_canonical]; this lifts it to blocks,
    where it was missing.

    Nothing here assumes [block_ok]. That matters: the hypothesis is
    about the bytes on the wire, which is the only thing a node
    actually has when it is deciding what to do with them. *)

Theorem encode_decode_block_from :
  forall bs b rest,
    decode_block_from bs = Some (b, rest) ->
    encode_block b ++ rest = bs.
Proof.
  intros bs b rest Hd.
  unfold decode_block_from in Hd.
  destruct (decode_header bs) as [[h rest0]|] eqn:Hh; [|discriminate].
  destruct (decode_compactsize rest0) as [[n rest1]|] eqn:Hn; [|discriminate].
  rewrite (decode_vec_N_spec Transaction encode_transaction
             decode_transaction_from encode_decode_transaction_from
             enc_transaction_nonempty) in Hd.
  destruct (decode_vec Transaction decode_transaction_from (N.to_nat n) rest1)
    as [[txs rest2]|] eqn:Hv; [|discriminate].
  injection Hd as Hb Hr. subst b rest.
  destruct (encode_decode_vec Transaction encode_transaction
              decode_transaction_from encode_decode_transaction_from
              (N.to_nat n) rest1 txs rest2 Hv) as [Hvenc Hlen].
  rewrite encode_block_app_assoc.
  cbn [header transactions].
  rewrite Hlen, N2Nat.id, Hvenc.
  rewrite (encode_decode_compactsize rest0 n rest1 Hn).
  exact (encode_decode_header bs h rest0 Hh).
Qed.

(** The strict decoder, which requires the input to be exhausted,
    accepts only the canonical encoding and nothing else. *)
Theorem encode_decode_block :
  forall bs b,
    decode_block bs = Some b ->
    encode_block b = bs.
Proof.
  intros bs b Hd.
  unfold decode_block in Hd.
  destruct (decode_block_from bs) as [[b' rest]|] eqn:Hf; [|discriminate].
  destruct rest; [|discriminate].
  injection Hd as Heq. subst b'.
  pose proof (encode_decode_block_from bs b nil Hf) as H.
  rewrite app_nil_r in H.
  exact H.
Qed.

(** No malleability: one block, one accepted byte string. *)
Theorem block_encoding_injective :
  forall bs1 bs2 b,
    decode_block bs1 = Some b ->
    decode_block bs2 = Some b ->
    bs1 = bs2.
Proof.
  intros bs1 bs2 b H1 H2.
  rewrite <- (encode_decode_block bs1 b H1).
  rewrite <- (encode_decode_block bs2 b H2).
  reflexivity.
Qed.

(** The encoder side of the same coin: distinct well-formed blocks
    cannot share an encoding. *)
Theorem encode_block_injective :
  forall b1 b2,
    block_ok b1 ->
    block_ok b2 ->
    encode_block b1 = encode_block b2 ->
    b1 = b2.
Proof.
  intros b1 b2 H1 H2 He.
  pose proof (decode_encode_block b1 H1) as D1.
  rewrite He in D1.
  rewrite (decode_encode_block b2 H2) in D1.
  injection D1 as Heq.
  symmetry. exact Heq.
Qed.

(** Canonicality buys the roundtrip on accepted bytes for free, with no
    well-formedness hypothesis. That is the practical difference between
    the two directions: a node holding bytes off the wire knows nothing
    about [block_ok] yet. *)
Theorem decode_block_idempotent_on_accepted :
  forall bs b,
    decode_block bs = Some b ->
    decode_block (encode_block b) = Some b.
Proof.
  intros bs b H.
  rewrite (encode_decode_block bs b H).
  exact H.
Qed.

(** * Non-vacuity

    Everything above is an implication, and an implication holds
    whenever nothing satisfies its hypothesis. A decoder that rejected
    every byte string, or a [block_ok] that nothing satisfied, would
    make the whole section true and worthless. These witnesses rule
    that out.

    The field values are deliberately below 253, which keeps the
    witnesses single-byte at the CompactSize layer. Since the bound
    constants are [N], they evaluate, and the side conditions here are
    ordinary arithmetic. *)

Lemma bytes_ok_repeat :
  forall b n, byte_ok b -> bytes_ok (List.repeat b n).
Proof.
  intros b n Hb.
  induction n as [|n IH]; cbn; [exact I | split; assumption].
Qed.

Lemma small_lt_two32 : forall n, (n < 253)%N -> (n < two32)%N.
Proof. unfold two32. lia. Qed.

Lemma small_lt_two64 : forall n, (n < 253)%N -> (n < two64)%N.
Proof. unfold two64. lia. Qed.

Lemma small_z_u32 : forall z, (0 <= z)%Z -> (z < 253)%Z -> z_u32 z.
Proof.
  intros z H0 H1. split; [exact H0 | unfold two32; lia].
Qed.

Lemma small_z_u64 : forall z, (0 <= z)%Z -> (z < 253)%Z -> z_u64 z.
Proof.
  intros z H0 H1. split; [exact H0 | unfold two64; lia].
Qed.

Definition zero32 : bytes32 := List.repeat 0%nat 32.

Lemma zero32_ok : bytes32_ok zero32.
Proof.
  split.
  - apply repeat_length.
  - apply bytes_ok_repeat. unfold byte_ok, byte_base. lia.
Qed.

Lemma empty_script_ok : script_ok nil.
Proof.
  split; [exact I | apply compactsize_ok_0].
Qed.

Definition witness_input : TxInput :=
  {| previous_outpoint := {| op_hash := zero32; op_index := 0%N |};
     script_data := nil;
     sequence := 0%Z |}.

Definition witness_output : TxOutput :=
  {| amount := 1%Z; script_pubkey := nil |}.

Definition witness_tx : Transaction :=
  {| version := 1%Z;
     inputs := [witness_input];
     outputs := [witness_output];
     locktime := 0%Z;
     witness := nil |}.

Definition witness_header (nonce : Z) : BlockHeader :=
  {| bh_version := 1%Z;
     bh_prev_hash := zero32;
     bh_merkle_root := zero32;
     bh_timestamp := 1%Z;
     bh_bits := 1%Z;
     bh_nonce := nonce |}.

Definition witness_block : Block :=
  {| header := witness_header 1%Z; transactions := [witness_tx] |}.

Definition witness_block' : Block :=
  {| header := witness_header 2%Z; transactions := [witness_tx] |}.


Lemma witness_input_ok : input_ok witness_input.
Proof.
  unfold input_ok, witness_input.
  cbn [previous_outpoint script_data sequence].
  split.
  - unfold outpoint_ok. cbn [op_hash op_index].
    split; [apply zero32_ok | apply small_lt_two32; lia].
  - split; [apply empty_script_ok | apply small_z_u32; lia].
Qed.

Lemma witness_output_ok : output_ok witness_output.
Proof.
  unfold output_ok, witness_output.
  cbn [amount script_pubkey].
  split; [apply small_z_u64; lia | apply empty_script_ok].
Qed.

Lemma witness_tx_ok : transaction_ok witness_tx.
Proof.
  unfold transaction_ok, witness_tx.
  cbn [version inputs outputs locktime witness].
  split; [apply small_z_u32; lia|].
  split; [apply small_z_u32; lia|].
  split; [discriminate|].
  split; [discriminate|].
  split; [constructor; [apply witness_input_ok | constructor]|].
  split; [constructor; [apply witness_output_ok | constructor]|].
  split; [cbn [List.length]; apply small_lt_two64; lia|].
  split; [cbn [List.length]; apply small_lt_two64; lia|].
  left. reflexivity.
Qed.

Lemma witness_header_ok :
  forall n, (0 <= n)%Z -> (n < 253)%Z -> header_ok (witness_header n).
Proof.
  intros n H0 H1.
  unfold header_ok, witness_header.
  cbn [bh_version bh_prev_hash bh_merkle_root bh_timestamp bh_bits bh_nonce].
  split; [apply small_z_u32; lia|].
  split; [apply zero32_ok|].
  split; [apply zero32_ok|].
  split; [apply small_z_u32; lia|].
  split; [apply small_z_u32; lia|].
  apply small_z_u32; lia.
Qed.

Lemma witness_block_ok : block_ok witness_block.
Proof.
  unfold block_ok, witness_block.
  cbn [header transactions].
  split; [apply witness_header_ok; lia|].
  split; [constructor; [apply witness_tx_ok | constructor]|].
  cbn [List.length]. apply small_lt_two64. lia.
Qed.

Lemma witness_block'_ok : block_ok witness_block'.
Proof.
  unfold block_ok, witness_block'.
  cbn [header transactions].
  split; [apply witness_header_ok; lia|].
  split; [constructor; [apply witness_tx_ok | constructor]|].
  cbn [List.length]. apply small_lt_two64. lia.
Qed.

(** [block_ok] is satisfiable, so [decode_encode_block] is not vacuous. *)
Theorem block_ok_inhabited : exists b, block_ok b.
Proof. exists witness_block. apply witness_block_ok. Qed.

(** The decoder accepts something, so [encode_decode_block],
    [block_encoding_injective] and [decode_block_idempotent_on_accepted]
    all constrain a nonempty set of byte strings. *)
Theorem decode_block_accepts_something :
  exists bs b, decode_block bs = Some b.
Proof.
  exists (encode_block witness_block), witness_block.
  apply decode_encode_block, witness_block_ok.
Qed.

(** The decoder also rejects something, so it is not the trivial
    always-accept function. *)
Theorem decode_block_rejects_something :
  exists bs, decode_block bs = None.
Proof. exists nil. reflexivity. Qed.

(** The encoder is not constant, so the two injectivity theorems are
    not statements about a one-element set. *)
Theorem encode_block_not_constant :
  exists b1 b2,
    block_ok b1 /\ block_ok b2 /\ encode_block b1 <> encode_block b2.
Proof.
  exists witness_block, witness_block'.
  split; [apply witness_block_ok|].
  split; [apply witness_block'_ok|].
  intros He.
  pose proof (encode_block_injective _ _ witness_block_ok witness_block'_ok He) as Heq.
  assert (Hn : bh_nonce (header witness_block) = bh_nonce (header witness_block'))
    by (rewrite Heq; reflexivity).
  unfold witness_block, witness_block', witness_header in Hn.
  cbn [header bh_nonce] in Hn.
  discriminate.
Qed.

(** * Rejection

    The corpus in tests/serialization_vectors records, per case, what
    the C++ deserialiser does with a byte string. The cases whose
    verdict is decided at the codec layer rather than by validation are
    turned into theorems here, so that the model is pinned to the same
    answers instead of merely being consistent with them.

    Cases covered:
      SV-TRUNCATED-HEADER, SV-C1_truncated_block
        -> decode_header_needs_80, decode_block_needs_80
      SV-NONCANONICAL-COMPACTSIZE-VTX
        -> decode_block_rejects_noncanonical_tx_count
      SV-SUPERFLUOUS-WITNESS-RECORD
        -> decode_rejects_superfluous_witness
      SV-TRAILING-BYTE-AFTER-BLOCK
        -> decode_block_rejects_trailing_bytes

    The last one is the case where the model and C++ disagree. The
    corpus marks it "observe", not "reject": C++ DecodeHexBlk returns
    a CBlock and leaves one byte unread, while the model below rejects.
    The disagreement is real and is characterised, not papered over,
    in the wire/strict section that follows. *)

(** ** Trailing bytes *)

Theorem decode_block_rejects_trailing_bytes :
  forall bs b rest,
    decode_block_from bs = Some (b, rest) ->
    rest <> nil ->
    decode_block bs = None.
Proof.
  intros bs b rest H Hr.
  unfold decode_block. rewrite H.
  destruct rest as [|x xs]; [exfalso; apply Hr; reflexivity | reflexivity].
Qed.

(** The corpus case, stated on the bytes it actually builds: a valid
    block encoding with something appended. *)
Corollary decode_block_rejects_appended_bytes :
  forall b suffix,
    block_ok b ->
    suffix <> nil ->
    decode_block (encode_block b ++ suffix) = None.
Proof.
  intros b suffix Hok Hs.
  apply (decode_block_rejects_trailing_bytes _ b suffix); [|exact Hs].
  apply decode_encode_block_from, Hok.
Qed.

(** ** Truncation

    Rather than case on where the input runs out, this goes through
    canonicality: anything the header decoder accepts must be at least
    as long as a header, because it has to contain one. *)

Lemma decode_u32z_z_u32 :
  forall bs z rest, decode_u32z bs = Some (z, rest) -> z_u32 z.
Proof.
  intros bs z rest H.
  unfold decode_u32z, decode_u32 in H.
  destruct (decode_le 4 bs) as [[n r]|] eqn:Hn; [|discriminate].
  injection H as Hz Hr. subst z rest.
  split.
  - apply N2Z.is_nonneg.
  - rewrite N2Z.id, two32_pow.
    exact (decode_le_bound 4 bs n r Hn).
Qed.

Lemma header_ok_of_decode :
  forall bs h rest, decode_header bs = Some (h, rest) -> header_ok h.
Proof.
  intros bs h rest Hd.
  unfold decode_header in Hd.
  destruct (decode_u32z bs) as [[ver rest0]|] eqn:Hv; [|discriminate].
  destruct (take_n 32 rest0) as [[prev rest1]|] eqn:Hp; [|discriminate].
  destruct (bytes_ok_bool prev) eqn:Hpb; [|discriminate].
  destruct (take_n 32 rest1) as [[merkle rest2]|] eqn:Hm; [|discriminate].
  destruct (bytes_ok_bool merkle) eqn:Hmb; [|discriminate].
  destruct (decode_u32z rest2) as [[tm rest3]|] eqn:Ht; [|discriminate].
  destruct (decode_u32z rest3) as [[bits rest4]|] eqn:Hb; [|discriminate].
  destruct (decode_u32z rest4) as [[nonce rest5]|] eqn:Hn; [|discriminate].
  injection Hd as Hh _. subst h.
  unfold header_ok.
  cbn [bh_version bh_prev_hash bh_merkle_root bh_timestamp bh_bits bh_nonce].
  split; [exact (decode_u32z_z_u32 _ _ _ Hv)|].
  split; [split; [exact (take_n_length 32 _ _ _ Hp) | exact (bytes_ok_bool_true _ Hpb)]|].
  split; [split; [exact (take_n_length 32 _ _ _ Hm) | exact (bytes_ok_bool_true _ Hmb)]|].
  split; [exact (decode_u32z_z_u32 _ _ _ Ht)|].
  split; [exact (decode_u32z_z_u32 _ _ _ Hb)|].
  exact (decode_u32z_z_u32 _ _ _ Hn).
Qed.

Lemma encode_header_length :
  forall h, header_ok h -> List.length (encode_header h) = 80.
Proof.
  intros h [_ [[Hpl _] [[Hml _] _]]].
  unfold encode_header, encode_u32z, encode_u32.
  repeat rewrite app_length.
  rewrite Hpl, Hml.
  repeat rewrite encode_le_length.
  reflexivity.
Qed.

Theorem decode_header_needs_80 :
  forall bs, List.length bs < 80 -> decode_header bs = None.
Proof.
  intros bs Hlen.
  destruct (decode_header bs) as [[h rest]|] eqn:Hd; [|reflexivity].
  exfalso.
  pose proof (encode_decode_header bs h rest Hd) as He.
  pose proof (encode_header_length h (header_ok_of_decode bs h rest Hd)) as HL.
  rewrite <- He, app_length, HL in Hlen.
  lia.
Qed.

Theorem decode_block_needs_80 :
  forall bs, List.length bs < 80 -> decode_block bs = None.
Proof.
  intros bs H.
  unfold decode_block, decode_block_from.
  rewrite (decode_header_needs_80 bs H).
  reflexivity.
Qed.

(** ** Non-canonical transaction count

    A count below 253 written in the three-byte form is rejected, so
    the transaction count carries no spare bits. This is the block-level
    lift of [reject_noncanonical_encoding]. *)

Theorem decode_block_rejects_noncanonical_tx_count :
  forall h n body rest,
    header_ok h ->
    (n < 253)%N ->
    decode_block_from
      (encode_header h ++ 253 :: encode_le 2 n ++ body ++ rest) = None.
Proof.
  intros h n body rest Hh Hn.
  unfold decode_block_from.
  rewrite (decode_encode_header_app h _ Hh).
  rewrite (reject_noncanonical_encoding n (body ++ rest) Hn).
  reflexivity.
Qed.

(** Not vacuous: the witness block has a transaction count in range. *)
Corollary witness_block_noncanonical_count_rejected :
  forall rest,
    decode_block_from
      (encode_header (header witness_block) ++
       253 :: encode_le 2 1%N ++
       encode_vec Transaction encode_transaction [witness_tx] ++ rest) = None.
Proof.
  intros rest.
  apply decode_block_rejects_noncanonical_tx_count; [|lia].
  cbn [header witness_block]. apply witness_header_ok; lia.
Qed.

(** ** Superfluous witness record

    A transaction carrying the segwit marker whose witness stacks are
    all empty is rejected, so the marker cannot be used to attach a
    second encoding to a transaction that has no witness data. *)

Definition empty_witness : Witness := {| stack := nil |}.

Lemma empty_witness_ok : witness_ok empty_witness.
Proof.
  unfold witness_ok, empty_witness, stack_ok. cbn [stack].
  split; [constructor | apply compactsize_ok_0].
Qed.

Definition superfluous_witness_bytes (rest : byte_list) : byte_list :=
  encode_u32z 1%Z ++
  0%nat :: 1%nat ::
  encode_compactsize 1%N ++
  encode_vec TxInput encode_input [witness_input] ++
  encode_compactsize 1%N ++
  encode_vec TxOutput encode_output [witness_output] ++
  encode_vec Witness encode_witness [empty_witness] ++
  encode_u32z 0%Z ++ rest.

Theorem decode_rejects_superfluous_witness :
  forall rest, decode_transaction_from (superfluous_witness_bytes rest) = None.
Proof.
  intros rest.
  assert (Hcs1 : compactsize_ok 1%N) by (apply small_lt_two64; lia).
  assert (Hcs0 : forall X, decode_compactsize (0%nat :: X) = Some (0%N, X))
    by (intros X; unfold decode_compactsize; reflexivity).
  unfold superfluous_witness_bytes, decode_transaction_from.
  rewrite (decode_encode_u32z_app 1%Z _ (small_z_u32 1%Z ltac:(lia) ltac:(lia))).
  rewrite Hcs0.
  cbn [N.eqb].
  unfold decode_witness_tail.
  rewrite (decode_encode_compactsize_app 1%N _ Hcs1).
  rewrite (decode_vec_N_spec TxInput encode_input decode_input
             enc_dec_input enc_input_nonempty).
  replace (N.to_nat 1%N) with 1%nat by reflexivity.
  rewrite (decode_encode_vec_app TxInput encode_input decode_input input_ok
             dec_enc_input [witness_input] _
             (Forall_cons _ witness_input_ok (Forall_nil _))).
  rewrite (decode_encode_compactsize_app 1%N _ Hcs1).
  rewrite (decode_vec_N_spec TxOutput encode_output decode_output
             enc_dec_output enc_output_nonempty).
  replace (N.to_nat 1%N) with 1%nat by reflexivity.
  rewrite (decode_encode_vec_app TxOutput encode_output decode_output output_ok
             dec_enc_output [witness_output] _
             (Forall_cons _ witness_output_ok (Forall_nil _))).
  cbn [List.length].
  rewrite (decode_encode_vec_app Witness encode_witness decode_witness witness_ok
             dec_enc_witness [empty_witness] _
             (Forall_cons _ empty_witness_ok (Forall_nil _))).
  rewrite (decode_encode_u32z_app 0%Z _ (small_z_u32 0%Z ltac:(lia) ltac:(lia))).
  cbn [witness_has_data empty_witness stack].
  reflexivity.
Qed.

(** ** What decoding does not give you

    [decode_encode_block] carries a [block_ok] hypothesis, and the
    obvious way to discharge it downstream would be a lemma saying that
    whatever the decoder produces is [block_ok]. No such lemma is
    proved here, because it is false.

    The counterexample is not exotic. [transaction_ok] requires a
    nonempty output list, while [decode_legacy_tail] happily accepts an
    output count of zero: the guard in [decode_transaction_from] forces
    the input count away from zero, since zero is the segwit marker,
    but nothing constrains the output count.

    The header layer does satisfy its own predicate, which is what
    [header_ok_of_decode] above establishes. The transaction layer does
    not, so [block_ok] has to keep being carried as a hypothesis or be
    re-established by validation. *)

Definition zero_output_tx : Transaction :=
  {| version := 1%Z;
     inputs := [witness_input];
     outputs := nil;
     locktime := 0%Z;
     witness := nil |}.

Definition zero_output_tx_bytes (rest : byte_list) : byte_list :=
  encode_u32z 1%Z ++
  encode_compactsize 1%N ++
  encode_vec TxInput encode_input [witness_input] ++
  encode_compactsize 0%N ++
  encode_u32z 0%Z ++ rest.

Lemma decode_zero_output_tx :
  forall rest,
    decode_transaction_from (zero_output_tx_bytes rest) = Some (zero_output_tx, rest).
Proof.
  intros rest.
  unfold zero_output_tx_bytes, decode_transaction_from.
  rewrite (decode_encode_u32z_app 1%Z _ (small_z_u32 1%Z ltac:(lia) ltac:(lia))).
  rewrite (decode_encode_compactsize_app 1%N _ (small_lt_two64 1%N ltac:(lia))).
  cbn [N.eqb].
  unfold decode_legacy_tail.
  rewrite (decode_vec_N_spec TxInput encode_input decode_input
             enc_dec_input enc_input_nonempty).
  replace (N.to_nat 1%N) with 1%nat by reflexivity.
  rewrite (decode_encode_vec_app TxInput encode_input decode_input input_ok
             dec_enc_input [witness_input] _
             (Forall_cons _ witness_input_ok (Forall_nil _))).
  rewrite (decode_encode_compactsize_app 0%N _ compactsize_ok_0).
  rewrite (decode_vec_N_spec TxOutput encode_output decode_output
             enc_dec_output enc_output_nonempty).
  cbn [N.to_nat decode_vec].
  rewrite (decode_encode_u32z_app 0%Z _ (small_z_u32 0%Z ltac:(lia) ltac:(lia))).
  reflexivity.
Qed.

Definition zero_output_block : Block :=
  {| header := witness_header 1%Z; transactions := [zero_output_tx] |}.

Definition zero_output_block_bytes (rest : byte_list) : byte_list :=
  encode_header (witness_header 1%Z) ++
  encode_compactsize 1%N ++
  zero_output_tx_bytes rest.

Lemma decode_zero_output_block :
  forall rest,
    decode_block_from (zero_output_block_bytes rest) = Some (zero_output_block, rest).
Proof.
  intros rest.
  unfold zero_output_block_bytes, decode_block_from.
  rewrite (decode_encode_header_app (witness_header 1%Z) _
             (witness_header_ok 1%Z ltac:(lia) ltac:(lia))).
  rewrite (decode_encode_compactsize_app 1%N _ (small_lt_two64 1%N ltac:(lia))).
  rewrite (decode_vec_N_spec Transaction encode_transaction
             decode_transaction_from encode_decode_transaction_from
             enc_transaction_nonempty).
  replace (N.to_nat 1%N) with 1%nat by reflexivity.
  cbn [decode_vec].
  rewrite decode_zero_output_tx.
  reflexivity.
Qed.

Theorem decode_does_not_imply_block_ok :
  exists bs b, decode_block bs = Some b /\ ~ block_ok b.
Proof.
  exists (zero_output_block_bytes nil), zero_output_block.
  split.
  - unfold decode_block. rewrite decode_zero_output_block. reflexivity.
  - intros [_ [Hall _]].
    inversion Hall as [|x l Htx _ Heq]; subst.
    destruct Htx as [_ [_ [_ [Hout _]]]].
    apply Hout. reflexivity.
Qed.

(** * Executability

    Everything above says what the model means. This says the model
    runs.

    While the wire scalars were unary [nat] it did not. A single 50 BTC
    output is 5 * 10^9, and [two32] and [two64] were powers of 256 in
    unary, so neither the value nor the bound it is checked against
    could be built, let alone compared. Both theorems below are proved
    by evaluation.

    Two things follow. Differential testing against C++ becomes
    possible, since the model can now be run on the same bytes. And
    these are the sharpest non-vacuity witnesses in the file: rather
    than exhibiting some block satisfying the predicates, they exhibit
    the decoder returning the block, at values that occur in real
    consensus data. *)

Definition realistic_tx : Transaction :=
  {| version := 1%Z;
     inputs := [{| previous_outpoint :=
                     {| op_hash := List.repeat 0%nat 32;
                        op_index := 4294967295%N |};
                   script_data := [4%nat; 255%nat; 255%nat; 0%nat; 29%nat];
                   sequence := 4294967295%Z |}];
     outputs := [{| amount := 5000000000%Z;
                    script_pubkey := [118%nat; 169%nat; 20%nat] |}];
     locktime := 0%Z;
     witness := nil |}.

Definition realistic_block : Block :=
  {| header := {| bh_version := 1%Z;
                  bh_prev_hash := List.repeat 0%nat 32;
                  bh_merkle_root := List.repeat 7%nat 32;
                  bh_timestamp := 1231006505%Z;
                  bh_bits := 486604799%Z;
                  bh_nonce := 2083236893%Z |};
     transactions := [realistic_tx] |}.

Theorem codec_runs_on_full_range_values :
  decode_block (encode_block realistic_block) = Some realistic_block.
Proof. vm_compute. reflexivity. Qed.

(** The 50 BTC amount is really in the bytes, little-endian, so the
    roundtrip above is not passing because the value was truncated. *)
Theorem realistic_encoding_carries_50btc :
  List.length (encode_block realistic_block) = 149 /\
  encode_u64z (amount {| amount := 5000000000%Z; script_pubkey := nil |})
    = [0%nat; 242%nat; 5%nat; 42%nat; 1%nat; 0%nat; 0%nat; 0%nat].
Proof. split; vm_compute; reflexivity. Qed.

(** The length guard fires and terminates: a claimed count of 2^32-1
    with no bytes behind it is rejected by evaluation, without the
    count ever being turned into an index. *)
Theorem oversized_count_rejected_by_evaluation :
  decode_block_from
    (encode_header (header realistic_block) ++ encode_compactsize 4294967295%N)
  = None.
Proof. vm_compute. reflexivity. Qed.

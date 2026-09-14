(** ConsensusModel.WireCodec

    The strict codec in BlockCodec is the specification: it consumes
    its input exactly, and it is the one the canonicality theorems are
    about. The C++ deserialiser does something slightly different, and
    this module says what.

    Three divergences are known and all three are characterised below.
    The first two were found by reading the C++; the third was found by
    the differential harness in fuzz/codec_differential, which is the
    reason that harness exists.

      1. Trailing bytes. DecodeHexBlk in core/src/core_read.cpp calls
         ss >> block and returns true without checking that the stream
         is empty. The corpus case SV-TRAILING-BYTE-AFTER-BLOCK records
         exactly this: status ok, leftover_bytes 1, roundtrip_equal
         false. The strict decoder rejects. So the strict decoder is
         the stricter of the two, and the difference is confined to
         trailing bytes.

      2. MAX_SIZE. ReadCompactSize in core/src/serialize.h throws when
         the decoded count exceeds MAX_SIZE = 0x02000000, while
         decode_compactsize has no such bound. Here the model is the
         more permissive one. At the block level the divergence is
         largely masked, and the masking is quantified below.

      3. The dummy-vin flag byte. UnserializeTransaction in
         core/src/primitives/transaction.h re-reads vin and vout only
         when the flag byte after an empty vin is nonzero, so a zero
         flag decodes to a transaction with no inputs and no outputs.
         decode_transaction_from demands a flag of exactly 1. The model
         is again the stricter one, and the divergence is confined to
         transactions that fail transaction_ok.

    TCB STATEMENT. That decode_block_wire and decode_compactsize_wire
    faithfully mirror the C++ functions is NOT proved here and cannot
    be: it is a claim about a C++ program, not about a Coq term. It is
    an assumption, evidenced by the corpus in
    tests/serialization_vectors, and it belongs in the trusted base.
    Everything below the assumption is proved.

    CPP_KERNELINPUT_EQUIVALENCE = NOT PROVED
*)
From Coq Require Import Arith.PeanoNat.
From Coq Require Import Bool.Bool.
From Coq Require Import List.
From Coq Require Import Lia.
From Coq Require Import ZArith.ZArith.
From Coq Require Import NArith.NArith.
From ConsensusModel Require Import ByteModel.
From ConsensusModel Require Import CompactSizeModel.
From ConsensusModel Require Import TransactionModel.
From ConsensusModel Require Import BlockModel.
From ConsensusModel Require Import TransactionCodec.
From ConsensusModel Require Import BlockCodec.
Import ListNotations.

(** * Wire semantics for blocks: consume a prefix, ignore the rest *)

Definition decode_block_wire (bs : byte_list) : option Block :=
  match decode_block_from bs with
  | Some (b, _) => Some b
  | None => None
  end.

(** The strict decoder is the stricter one. *)
Theorem strict_implies_wire :
  forall bs b, decode_block bs = Some b -> decode_block_wire bs = Some b.
Proof.
  intros bs b H.
  unfold decode_block in H. unfold decode_block_wire.
  destruct (decode_block_from bs) as [[b' r]|]; [|discriminate].
  destruct r; [exact H | discriminate].
Qed.

Theorem wire_agrees_when_exhausted :
  forall bs b,
    decode_block_from bs = Some (b, nil) ->
    decode_block bs = decode_block_wire bs.
Proof.
  intros bs b H.
  unfold decode_block, decode_block_wire. rewrite H. reflexivity.
Qed.

(** The exact shape of the disagreement. Not "they differ somewhere",
    but "they differ on precisely the inputs with bytes left over". *)
Theorem wire_divergence_is_exactly_trailing :
  forall bs,
    decode_block bs <> decode_block_wire bs <->
    (exists b r, decode_block_from bs = Some (b, r) /\ r <> nil).
Proof.
  intros bs.
  unfold decode_block, decode_block_wire.
  destruct (decode_block_from bs) as [[b r]|] eqn:Hd.
  - destruct r as [|x xs].
    + split.
      * intros Hne. exfalso. apply Hne. reflexivity.
      * intros [b' [r' [Heq Hr]]].
        injection Heq as _ Hr'. subst r'.
        exfalso. apply Hr. reflexivity.
    + split.
      * intros _. exists b, (x :: xs). split; [reflexivity | discriminate].
      * intros _. discriminate.
  - split.
    + intros Hne. exfalso. apply Hne. reflexivity.
    + intros [b [r [Heq _]]]. discriminate.
Qed.

(** The consequence that matters. Under wire semantics a block has more
    than one accepted byte string, so the wire decoder has no analogue
    of [block_encoding_injective]. This is the formal content of
    roundtrip_equal = false in SV-TRAILING-BYTE-AFTER-BLOCK. *)
Theorem wire_is_malleable :
  exists bs1 bs2 b,
    bs1 <> bs2 /\
    decode_block_wire bs1 = Some b /\
    decode_block_wire bs2 = Some b.
Proof.
  exists (encode_block witness_block),
         (encode_block witness_block ++ [0%nat]),
         witness_block.
  split; [|split].
  - intros He.
    assert (Hl : List.length (encode_block witness_block)
               = List.length (encode_block witness_block ++ [0%nat]))
      by (rewrite <- He; reflexivity).
    rewrite app_length in Hl. cbn [List.length] in Hl. lia.
  - unfold decode_block_wire.
    pose proof (decode_encode_block_from witness_block nil witness_block_ok) as H.
    rewrite app_nil_r in H. rewrite H. reflexivity.
  - unfold decode_block_wire.
    rewrite (decode_encode_block_from witness_block [0%nat] witness_block_ok).
    reflexivity.
Qed.

(** The strict decoder does have that analogue: this is
    [block_encoding_injective], restated here for the contrast. *)
Corollary strict_is_not_malleable :
  forall bs1 bs2 b,
    decode_block bs1 = Some b ->
    decode_block bs2 = Some b ->
    bs1 = bs2.
Proof. exact block_encoding_injective. Qed.

(** The containment is strict in the direction one wants: everything
    the strict decoder accepts, the wire decoder accepts, and the wire
    decoder accepts strictly more. *)
Theorem strict_strictly_stronger_than_wire :
  (forall bs b, decode_block bs = Some b -> decode_block_wire bs = Some b) /\
  (exists bs b, decode_block_wire bs = Some b /\ decode_block bs = None).
Proof.
  split; [exact strict_implies_wire|].
  exists (encode_block witness_block ++ [0%nat]), witness_block.
  split.
  - unfold decode_block_wire.
    rewrite (decode_encode_block_from witness_block [0%nat] witness_block_ok).
    reflexivity.
  - apply decode_block_rejects_appended_bytes; [apply witness_block_ok | discriminate].
Qed.

(** * MAX_SIZE

    0x02000000 = 2 * 256^3 = 33554432. *)

Definition MAX_SIZE : N := 33554432%N.

Lemma MAX_SIZE_val : MAX_SIZE = (2 * npow_base 3)%N.
Proof. reflexivity. Qed.

Definition decode_compactsize_wire (bs : byte_list) : option (N * byte_list) :=
  match decode_compactsize bs with
  | None => None
  | Some (n, rest) => if (MAX_SIZE <? n)%N then None else Some (n, rest)
  end.

Lemma max_size_succ_lt_two32 : (N.succ MAX_SIZE < two32)%N.
Proof. unfold MAX_SIZE, two32. lia. Qed.

Lemma max_size_succ_compactsize_ok : compactsize_ok (N.succ MAX_SIZE).
Proof. unfold compactsize_ok, MAX_SIZE, two64. lia. Qed.

(** Whatever the wire decoder returns respects the C++ bound. *)
Theorem compactsize_wire_bounded :
  forall bs n rest, decode_compactsize_wire bs = Some (n, rest) -> (n <= MAX_SIZE)%N.
Proof.
  intros bs n rest H.
  unfold decode_compactsize_wire in H.
  destruct (decode_compactsize bs) as [[m r]|] eqn:Hd; [|discriminate].
  destruct (MAX_SIZE <? m)%N eqn:Hb; [discriminate|].
  injection H as Hn Hr. subst m r.
  apply N.ltb_ge in Hb. exact Hb.
Qed.

(** The second divergence, in the opposite direction from the first:
    here the model accepts and C++ rejects. *)
Theorem compactsize_model_accepts_above_max :
  forall n rest,
    (MAX_SIZE < n)%N ->
    compactsize_ok n ->
    decode_compactsize (encode_compactsize n ++ rest) = Some (n, rest) /\
    decode_compactsize_wire (encode_compactsize n ++ rest) = None.
Proof.
  intros n rest Hgt Hok.
  pose proof (decode_encode_compactsize_app n rest Hok) as Hd.
  split; [exact Hd|].
  unfold decode_compactsize_wire. rewrite Hd.
  apply N.ltb_lt in Hgt. rewrite Hgt. reflexivity.
Qed.

(** Not vacuous: such counts exist inside the CompactSize range. *)
Theorem compactsize_max_size_divergence_nonvacuous :
  exists n, (MAX_SIZE < n)%N /\ compactsize_ok n.
Proof.
  exists (N.succ MAX_SIZE).
  split; [lia | apply max_size_succ_compactsize_ok].
Qed.

(** * Why the MAX_SIZE divergence does not reach the block layer

    A transaction encoding is at least four bytes, so a block the model
    accepts cannot claim more transactions than its own length allows.
    A count above MAX_SIZE therefore needs an input of at least
    4 * 0x02000000 = 128 MiB before the model would even accept it,
    which no block-size rule permits. The divergence is real at the
    CompactSize layer and unreachable at the block layer. *)

Lemma encode_transaction_length_ge_4 :
  forall tx, 4 <= List.length (encode_transaction tx).
Proof.
  intros tx. unfold encode_transaction.
  rewrite app_length.
  unfold encode_u32z, encode_u32. rewrite encode_le_length.
  lia.
Qed.

Lemma encode_vec_tx_length :
  forall xs,
    4 * List.length xs <= List.length (encode_vec Transaction encode_transaction xs).
Proof.
  induction xs as [|x t IH]; cbn [encode_vec List.length]; [lia|].
  rewrite app_length.
  pose proof (encode_transaction_length_ge_4 x).
  lia.
Qed.

Theorem block_tx_count_bounded_by_length :
  forall bs b rest,
    decode_block_from bs = Some (b, rest) ->
    4 * List.length (transactions b) <= List.length bs.
Proof.
  intros bs b rest H.
  pose proof (encode_decode_block_from bs b rest H) as He.
  rewrite <- He.
  unfold encode_block.
  repeat rewrite app_length.
  pose proof (encode_vec_tx_length (transactions b)).
  lia.
Qed.

Theorem max_size_divergence_masked_at_block_level :
  forall bs b rest,
    decode_block_from bs = Some (b, rest) ->
    (MAX_SIZE < N.of_nat (List.length (transactions b)))%N ->
    (4 * MAX_SIZE < N.of_nat (List.length bs))%N.
Proof.
  intros bs b rest Hd Hgt.
  pose proof (block_tx_count_bounded_by_length bs b rest Hd).
  unfold MAX_SIZE in *. lia.
Qed.

(** * Divergence 3: the dummy-vin flag byte

    Found by the 1.3 differential harness, not by inspection: 189 of
    12465 generated inputs, all of them this one shape. See
    fuzz/codec_differential.

    UnserializeTransaction in core/src/primitives/transaction.h reads
    the flag byte that follows an empty vin, and re-reads vin and vout
    only when that byte is nonzero:

        s >> tx.vin;
        if (tx.vin.size() == 0 && fAllowWitness) {
            s >> flags;
            if (flags != 0) { s >> tx.vin; s >> tx.vout; }
        } else {
            s >> tx.vout;
        }

    A zero flag therefore consumes neither vin nor vout, and the ten
    bytes  version || 00 || 00 || locktime  deserialise to a
    transaction with no inputs and no outputs. C++ re-serialises those
    ten bytes unchanged, so on that side it is even a canonical
    encoding. decode_transaction_from requires the flag to be exactly
    1 and rejects.

    Note the direction: here the model is the stricter one, as in
    divergence 1 and unlike divergence 2. *)

Definition decode_transaction_cpp (bs : byte_list)
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
            | 0 :: rest2 =>
                match decode_u32z rest2 with
                | None => None
                | Some (lock, rest3) =>
                    Some ({| version := ver;
                             inputs := nil;
                             outputs := nil;
                             locktime := lock;
                             witness := nil |}, rest3)
                end
            | _ => None
            end
          else decode_legacy_tail ver n rest1
      end
  end.

(** The model decoder is the stricter one. *)
Theorem strict_implies_cpp_tx :
  forall bs r,
    decode_transaction_from bs = Some r ->
    decode_transaction_cpp bs = Some r.
Proof.
  intros bs r H.
  unfold decode_transaction_cpp, decode_transaction_from in *.
  destruct (decode_u32z bs) as [[ver rest]|]; [|discriminate].
  destruct (decode_compactsize rest) as [[n rest1]|]; [|discriminate].
  destruct (n =? 0)%N; [|exact H].
  destruct rest1 as [|b rest2]; [discriminate|].
  destruct b as [|b']; [discriminate|].
  destruct b' as [|b'']; [exact H|discriminate].
Qed.

(** The exact shape of the disagreement, in the style of divergence 1:
    not "they differ somewhere" but "everything C++ accepts and the
    model rejects is an empty transaction". *)
Theorem cpp_divergence_is_exactly_empty_tx :
  forall bs tx rest,
    decode_transaction_cpp bs = Some (tx, rest) ->
    decode_transaction_from bs = None ->
    inputs tx = nil /\ outputs tx = nil /\ witness tx = nil.
Proof.
  intros bs tx rest Hcpp Hstrict.
  unfold decode_transaction_cpp, decode_transaction_from in *.
  destruct (decode_u32z bs) as [[ver r0]|]; [|discriminate].
  destruct (decode_compactsize r0) as [[n r1]|]; [|discriminate].
  destruct (n =? 0)%N;
    [|rewrite Hstrict in Hcpp; discriminate].
  destruct r1 as [|b r2]; [discriminate|].
  destruct b as [|b'].
  - destruct (decode_u32z r2) as [[lock r3]|]; [|discriminate].
    inversion Hcpp; subst; simpl; auto.
  - destruct b' as [|b'']; [rewrite Hstrict in Hcpp; discriminate|discriminate].
Qed.

(** The masking argument. Every transaction on which the two decoders
    disagree fails transaction_ok, which requires a nonempty vin. So
    nothing the model proves about well-formed transactions is weakened
    by the divergence, and a node that rejects empty-vin transactions
    downstream reaches the model's verdict anyway. *)
Theorem cpp_divergence_excluded_by_transaction_ok :
  forall bs tx rest,
    decode_transaction_cpp bs = Some (tx, rest) ->
    decode_transaction_from bs = None ->
    ~ transaction_ok tx.
Proof.
  intros bs tx rest Hcpp Hstrict Hok.
  destruct (cpp_divergence_is_exactly_empty_tx bs tx rest Hcpp Hstrict)
    as [Hin _].
  unfold transaction_ok in Hok.
  destruct Hok as [_ [_ [Hne _]]].
  exact (Hne Hin).
Qed.

(** Not vacuous, and not hypothetical: these are the exact ten bytes
    the harness produced. *)
Definition empty_tx_bytes : byte_list :=
  [1; 0; 0; 0] ++ [0] ++ [0] ++ [0; 0; 0; 0].

Theorem cpp_divergence_nonvacuous :
  decode_transaction_from empty_tx_bytes = None /\
  exists tx, decode_transaction_cpp empty_tx_bytes = Some (tx, nil)
             /\ inputs tx = nil
             /\ outputs tx = nil.
Proof.
  split.
  - vm_compute. reflexivity.
  - eexists. repeat split; vm_compute; reflexivity.
Qed.

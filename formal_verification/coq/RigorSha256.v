(** Pure Coq SHA-256 / SHA256d (FIPS 180-4) for [RigorHash]. No axioms. *)
From Coq Require Import List.
From Coq Require Import ZArith.ZArith.
From Coq Require Import Arith.PeanoNat.
Import ListNotations.

Definition byte := nat.

Definition w32 (z : Z) : Z :=
  Z.modulo z (Z.shiftl 1 32).

Definition w32_add (a b : Z) : Z := w32 (a + b).
Definition w32_xor (a b : Z) : Z := w32 (Z.lxor a b).
Definition w32_and (a b : Z) : Z := w32 (Z.land a b).
Definition w32_not (a : Z) : Z := w32 (Z.lnot a).

Definition rotr32 (x : Z) (n : nat) : Z :=
  let nZ := Z.of_nat (Nat.min n 32) in
  w32 (Z.lor (Z.shiftr x nZ) (Z.shiftl x (32 - nZ))).

Definition shr32 (x : Z) (n : nat) : Z :=
  Z.shiftr x (Z.of_nat (Nat.min n 32)).

Definition ch (x y z : Z) : Z := w32_xor (w32_and x y) (w32_and (w32_not x) z).
Definition maj (x y z : Z) : Z := w32_xor (w32_xor (w32_and x y) (w32_and x z)) (w32_and y z).
Definition bsig0 (x : Z) : Z := w32_xor (w32_xor (rotr32 x 2) (rotr32 x 13)) (rotr32 x 22).
Definition bsig1 (x : Z) : Z := w32_xor (w32_xor (rotr32 x 6) (rotr32 x 11)) (rotr32 x 25).
Definition ssig0 (x : Z) : Z := w32_xor (w32_xor (rotr32 x 7) (rotr32 x 18)) (shr32 x 3).
Definition ssig1 (x : Z) : Z := w32_xor (w32_xor (rotr32 x 17) (rotr32 x 19)) (shr32 x 10).

Definition k256 : list Z :=
  [ 1116352408%Z; 1899447441%Z; 3049323471%Z; 3921009573%Z; 961987163%Z; 1508970993%Z;
    2453635748%Z; 2870763221%Z; 3624381080%Z; 310598401%Z; 607225278%Z; 1426881987%Z;
    1925078388%Z; 2162078206%Z; 2614888103%Z; 3248222580%Z; 3835390401%Z; 4022224774%Z;
    264347078%Z; 604807628%Z; 770255983%Z; 1249150122%Z; 1555081692%Z; 1996064986%Z;
    2554220882%Z; 2821834349%Z; 2952996808%Z; 3210313671%Z; 3336571891%Z; 3584528711%Z;
    113926993%Z; 338241895%Z; 666307205%Z; 773529912%Z; 1294757372%Z; 1396182291%Z;
    1695183700%Z; 1986661051%Z; 2177026350%Z; 2456956037%Z; 2730485921%Z; 2820302411%Z;
    3259730800%Z; 3345764771%Z; 3516065817%Z; 3600352804%Z; 4094571909%Z; 275423344%Z;
    430227734%Z; 506948616%Z; 659060556%Z; 883997877%Z; 958139571%Z; 1322822218%Z;
    1537002063%Z; 1747873779%Z; 1955562222%Z; 2024104815%Z; 2227730452%Z; 2361852424%Z;
    2428436474%Z; 2756734187%Z; 3204031479%Z; 3329325298%Z ].

Definition h256_init : list Z :=
  [ 1779033703%Z; 3144134277%Z; 1013904242%Z; 2773480762%Z;
    1359893119%Z; 2600822924%Z; 528734635%Z; 1541459225%Z ].

Fixpoint get_k (i : nat) (ks : list Z) : Z :=
  match ks with
  | nil => 0
  | k :: rest => if Nat.eqb i 0 then k else get_k (pred i) rest
  end.

Fixpoint nth_word (i : nat) (ws : list Z) : Z :=
  match i, ws with
  | _, nil => 0
  | O, w :: _ => w
  | S i', _ :: rest => nth_word i' rest
  end.

(* PD-36 fix: [r = 64 - i] is the schedule index being built (0..63), counting
   UP as the fixpoint counts [i] down 64..1. The previous code keyed the
   "load from message vs recurrence" test and the message lookup on [i'] (= i-1,
   counting DOWN), so W[0] was (wrongly) computed by the recurrence on an empty
   accumulator instead of loaded from M[0]. *)
Fixpoint build_w_go (i : nat) (M : list Z) (acc : list Z) : list Z :=
  match i with
  | O => rev acc
  | S i' =>
      let r := (64 - i)%nat in
      let wi :=
        if Nat.ltb r 16 then
          nth_word r M
        else
          let w_im2 := nth_word 1 acc in
          let w_im7 := nth_word 6 acc in
          let w_im15 := nth_word 14 acc in
          let w_im16 := nth_word 15 acc in
          w32_add (w32_add (w32_add (ssig1 w_im2) w_im16) (ssig0 w_im15)) w_im7
      in
      build_w_go i' M (wi :: acc)
  end.

Definition build_w (M : list Z) : list Z := build_w_go 64 M nil.

(* PD-36 fix: the round index must count UP (W[0]/K[0] applied first, W[63] last).
   With structural recursion on [t : 64..0], the round number is [64 - t]; the
   previous code used [t'] (= t-1), which applied the schedule in REVERSE order
   and produced a non-FIPS digest. *)
Fixpoint sha256_round (t : nat) (a b c d e f g h : Z) (W : list Z) : list Z :=
  match t with
  | O => [a; b; c; d; e; f; g; h]
  | S t' =>
      let r := (64 - t)%nat in
      let w := nth_word r W in
      let k := get_k r k256 in
      let t1 := w32_add (w32_add (w32_add (w32_add h (bsig1 e)) (ch e f g)) k) w in
      let t2 := w32_add (bsig0 a) (maj a b c) in
      sha256_round t' (w32_add t1 t2) a b c (w32_add d t1) e f g W
  end.

(* Uses [nth_word] accessors instead of deep list patterns [a;b;c;d;e;f;g;h].

   The pattern-match form compiled fine in isolation, but when this definition
   was referenced inside the [sha256_blocks] fixpoint, Coq unfolded it during
   typing and the 8-deep nested match (with the [| _ => hstate] catch-all
   duplicated at every level) expanded exponentially, OOM-killing [coqc].
   Accessor form has no deep match and elaborates in constant size. The SHA-256
   state is always exactly 8 words ([h256_init] has 8 and each compression
   preserves 8), and [sha256_round 64 ...] always yields 8 words, so on every
   reachable input this agrees byte-for-byte with the previous definition. *)
Definition sha256_compress_block (hstate M : list Z) : list Z :=
  let W := build_w M in
  let a := nth_word 0 hstate in
  let b := nth_word 1 hstate in
  let c := nth_word 2 hstate in
  let d := nth_word 3 hstate in
  let e := nth_word 4 hstate in
  let f := nth_word 5 hstate in
  let g := nth_word 6 hstate in
  let h := nth_word 7 hstate in
  let r := sha256_round 64 a b c d e f g h W in
  [ w32_add a (nth_word 0 r); w32_add b (nth_word 1 r);
    w32_add c (nth_word 2 r); w32_add d (nth_word 3 r);
    w32_add e (nth_word 4 r); w32_add f (nth_word 5 r);
    w32_add g (nth_word 6 r); w32_add h (nth_word 7 r) ].

Fixpoint bytes_to_words_go (bs : list byte) (acc : list Z) (n : nat) : list Z :=
  match n with
  | O => rev acc
  | S n' =>
      match bs with
      | b0 :: b1 :: b2 :: b3 :: rest =>
          let w :=
            w32 (Z.of_nat b0 * 16777216 + Z.of_nat b1 * 65536 +
                 Z.of_nat b2 * 256 + Z.of_nat b3)
          in
          bytes_to_words_go rest (w :: acc) n'
      | _ => rev acc
      end
  end.

Definition bytes_to_block16 (bs : list byte) : list Z :=
  bytes_to_words_go bs nil 16.

Fixpoint sha256_blocks (hstate : list Z) (blocks : list (list byte)) : list Z :=
  match blocks with
  | nil => hstate
  | blk :: rest =>
      sha256_blocks (sha256_compress_block hstate (bytes_to_block16 blk)) rest
  end.

Fixpoint repeat_n (n : nat) (x : byte) : list byte :=
  match n with
  | O => nil
  | S n' => x :: repeat_n n' x
  end.

Fixpoint app_list (xs ys : list byte) : list byte :=
  match xs with
  | nil => ys
  | x :: xr => x :: app_list xr ys
  end.

(* PD-37 fix: reduce modulo 256 in the Z domain BEFORE [Z.to_nat]. The previous
   form [Z.to_nat len_bytes mod 256] parses as [(Z.to_nat len_bytes) mod 256],
   which first materialises the full (possibly 64-bit) value as a *unary* [nat]
   -> vm_compute OOM. Value is unchanged (X>=0 => Z.to_nat X mod 256 =
   Z.to_nat (X mod 256)); only evaluation cost changes. Big-endian length here
   is the *bit* length already (caller passes bits); we serialise len in bits. *)
Definition u64_be_bytes (len_bits : Z) : list byte :=
  [ Z.to_nat (Z.modulo (Z.shiftr len_bits 56) 256);
    Z.to_nat (Z.modulo (Z.shiftr len_bits 48) 256);
    Z.to_nat (Z.modulo (Z.shiftr len_bits 40) 256);
    Z.to_nat (Z.modulo (Z.shiftr len_bits 32) 256);
    Z.to_nat (Z.modulo (Z.shiftr len_bits 24) 256);
    Z.to_nat (Z.modulo (Z.shiftr len_bits 16) 256);
    Z.to_nat (Z.modulo (Z.shiftr len_bits 8) 256);
    Z.to_nat (Z.modulo len_bits 256) ].

Fixpoint take_bytes (n : nat) (l : list byte) : list byte * list byte :=
  match n, l with
  | O, _ => (nil, l)
  | S n', x :: xr => let '(t, r) := take_bytes n' xr in (x :: t, r)
  | _, nil => (nil, nil)
  end.

(* Fuel-based structural recursion on [fuel : nat].

   The previous formulation recursed directly on [l] with the decreasing
   argument [rest := snd (take_bytes 64 l)]. Because [rest] is a projection of
   another function's result rather than a syntactic subterm of [l], Coq's
   guardedness checker had to repeatedly unfold [take_bytes] under the [| _ =>]
   wildcard, triggering an exponential-time/space guard check that OOM-killed
   [coqc]. Recursing on an explicit [nat] fuel (bounded by [length l]) makes the
   recursion trivially structural. Semantics are unchanged: each step consumes
   up to 64 bytes, so [length l] iterations always suffice. *)
Fixpoint split_blocks_fuel (fuel : nat) (l : list byte) (acc : list (list byte))
  : list (list byte) :=
  match fuel with
  | O => rev acc
  | S fuel' =>
      match l with
      | nil => rev acc
      | _ =>
          let '(blk, rest) := take_bytes 64 l in
          split_blocks_fuel fuel' rest (blk :: acc)
      end
  end.

Definition split_blocks_go (l : list byte) (acc : list (list byte)) : list (list byte) :=
  split_blocks_fuel (length l) l acc.

(* FIPS 180-4 §5.1.1 padding, byte domain.

   PD-36 fix: the previous formula  k := (448 - (n+1)) mod 512  mixed the
   *bit*-domain constants 448/512 with a *byte* length [n], producing far too
   many zero bytes (e.g. 8 blocks for the empty message instead of 1) and a
   hash that did NOT equal FIPS 180-4 / Bitcoin CSHA256.

   Correct byte-domain rule: after appending the 0x80 byte we need the total to
   be ≡ 56 (mod 64) bytes before the 8-byte big-endian bit-length, i.e.
   (n + 1 + k) ≡ 56 (mod 64)  ⇒  k ≡ (55 - n) (mod 64). *)
Definition sha256_pad (msg : list byte) : list (list byte) :=
  let n := length msg in
  let k := Z.to_nat (Z.modulo (55 - Z.of_nat n) 64) in
  (* FIPS 180-4: the single '1' bit is the byte 0x80 (=128), NOT 0x01. *)
  let padded := app_list msg (128 :: app_list (repeat_n k 0) (u64_be_bytes (Z.of_nat n * 8))) in
  split_blocks_go padded nil.

(* PD-37 fix: see [u64_be_bytes]. [Z.to_nat w mod 256] materialised the full
   32-bit word as a unary nat (~1.6e9 successors) -> vm_compute OOM. Reducing
   mod 256 in Z first is value-identical and evaluation-safe. *)
Definition word_to_bytes (w : Z) : list byte :=
  [ Z.to_nat (Z.modulo (Z.shiftr w 24) 256);
    Z.to_nat (Z.modulo (Z.shiftr w 16) 256);
    Z.to_nat (Z.modulo (Z.shiftr w 8) 256);
    Z.to_nat (Z.modulo w 256) ].

(* PD-36 fix: emit words in FORWARD order h0..h7 (big-endian within each word).
   The previous accumulator form prepended each word, reversing the 8-word order
   and producing a non-FIPS digest byte order. *)
Fixpoint words_to_bytes_go (ws : list Z) : list byte :=
  match ws with
  | nil => nil
  | w :: rest => app_list (word_to_bytes w) (words_to_bytes_go rest)
  end.

Definition words_to_bytes32 (ws : list Z) : list byte :=
  words_to_bytes_go ws.

Definition sha256_bytes (msg : list byte) : list byte :=
  let blocks := sha256_pad msg in
  words_to_bytes32 (sha256_blocks h256_init blocks).

Definition hash256_bytes (msg : list byte) : list byte :=
  sha256_bytes (sha256_bytes msg).

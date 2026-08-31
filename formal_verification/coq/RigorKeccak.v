(** Executable Keccak-f[1600] and SHA3-256 (FIPS PUB 202), for [PowDigest].

    SPEC-008 §2 / §9. This is a computation, not an oracle: every constant is
    written out and the digest is obtained by *running* the permutation, so
    the golden vectors at the end discharge by [vm_compute] exactly as the
    SHA-256 side of this development does.

    Lane representation is [N] with an explicit 64-bit mask, per the SPEC-008
    §9 note. Boolean-list lanes were rejected: 24 rounds over a 1600-bit
    state makes [vm_compute] impractical in that representation.

    Byte type is [RigorBytes.byte] (= [nat]) so this module composes with the
    rest of the corpus with no conversion layer.

    Scope discipline: the permutation is written under [N_scope]; everything
    from the byte/lane boundary onward is written under [nat_scope] with [%N]
    annotations. Mixing the two silently retypes numeric literals. *)

From Coq Require Import List.
From Coq Require Import Lia.
From Coq Require Import NArith.NArith.
From Rigor Require Import RigorBytes.
Import ListNotations.

(* ------------------------------------------------------------------ *)
(** * Part A — 64-bit lanes and the permutation                        *)
(* ------------------------------------------------------------------ *)

Local Open Scope N_scope.

Definition mask64 : N := 18446744073709551615.  (* 2^64 - 1 *)

Definition w64 (x : N) : N := N.land x mask64.

Definition lxor64 (a b : N) : N := N.lxor a b.
Definition land64 (a b : N) : N := N.land a b.

(** Bitwise complement inside the 64-bit word. [N.lnot] takes an explicit
    width and still needs masking afterwards, so XOR against the mask is both
    shorter and obviously total. *)
Definition lnot64 (a : N) : N := N.lxor a mask64.

(** Rotate left. Total for every [n]: at [n = 0] the high half is
    [N.shiftr x 64], which is [0] for [x < 2^64], so no special case is
    needed. *)
Definition rotl64 (x : N) (n : N) : N :=
  N.lor (w64 (N.shiftl x n)) (N.shiftr x (64 - n)).

(** ** Round constants (FIPS 202, Keccak-f[1600], 24 rounds) *)

Definition keccak_rc : list N :=
  [ 1                    (* 0x0000000000000001 *)
  ; 32898                (* 0x0000000000008082 *)
  ; 9223372036854808714  (* 0x800000000000808A *)
  ; 9223372039002292224  (* 0x8000000080008000 *)
  ; 32907                (* 0x000000000000808B *)
  ; 2147483649           (* 0x0000000080000001 *)
  ; 9223372039002292353  (* 0x8000000080008081 *)
  ; 9223372036854808585  (* 0x8000000000008009 *)
  ; 138                  (* 0x000000000000008A *)
  ; 136                  (* 0x0000000000000088 *)
  ; 2147516425           (* 0x0000000080008009 *)
  ; 2147483658           (* 0x000000008000000A *)
  ; 2147516555           (* 0x000000008000808B *)
  ; 9223372036854775947  (* 0x800000000000008B *)
  ; 9223372036854808713  (* 0x8000000000008089 *)
  ; 9223372036854808579  (* 0x8000000000008003 *)
  ; 9223372036854808578  (* 0x8000000000008002 *)
  ; 9223372036854775936  (* 0x8000000000000080 *)
  ; 32778                (* 0x000000000000800A *)
  ; 9223372039002259466  (* 0x800000008000000A *)
  ; 9223372039002292353  (* 0x8000000080008081 *)
  ; 9223372036854808704  (* 0x8000000000008080 *)
  ; 2147483649           (* 0x0000000080000001 *)
  ; 9223372039002292232  (* 0x8000000080008008 *)
  ].

(** ** The round function

    State is a 25-lane list in FIPS 202 order [i = x + 5*y], i.e. lane
    [a{x}{y}] sits at index [x + 5y]. The list is destructured rather than
    indexed with [nth]: the rho/pi offsets are a fixed table, so writing them
    out makes the permutation auditable against the standard line by line and
    removes 25 list traversals per round. *)

Definition keccak_round (rc : N) (st : list N) : list N :=
  match st with
  | a00::a10::a20::a30::a40::
    a01::a11::a21::a31::a41::
    a02::a12::a22::a32::a42::
    a03::a13::a23::a33::a43::
    a04::a14::a24::a34::a44::nil =>

      (* theta: C[x] = xor over y ; D[x] = C[x-1] xor rotl(C[x+1], 1) *)
      let c0 := lxor64 (lxor64 (lxor64 (lxor64 a00 a01) a02) a03) a04 in
      let c1 := lxor64 (lxor64 (lxor64 (lxor64 a10 a11) a12) a13) a14 in
      let c2 := lxor64 (lxor64 (lxor64 (lxor64 a20 a21) a22) a23) a24 in
      let c3 := lxor64 (lxor64 (lxor64 (lxor64 a30 a31) a32) a33) a34 in
      let c4 := lxor64 (lxor64 (lxor64 (lxor64 a40 a41) a42) a43) a44 in
      let d0 := lxor64 c4 (rotl64 c1 1) in
      let d1 := lxor64 c0 (rotl64 c2 1) in
      let d2 := lxor64 c1 (rotl64 c3 1) in
      let d3 := lxor64 c2 (rotl64 c4 1) in
      let d4 := lxor64 c3 (rotl64 c0 1) in

      let t00 := lxor64 a00 d0 in let t10 := lxor64 a10 d1 in
      let t20 := lxor64 a20 d2 in let t30 := lxor64 a30 d3 in
      let t40 := lxor64 a40 d4 in
      let t01 := lxor64 a01 d0 in let t11 := lxor64 a11 d1 in
      let t21 := lxor64 a21 d2 in let t31 := lxor64 a31 d3 in
      let t41 := lxor64 a41 d4 in
      let t02 := lxor64 a02 d0 in let t12 := lxor64 a12 d1 in
      let t22 := lxor64 a22 d2 in let t32 := lxor64 a32 d3 in
      let t42 := lxor64 a42 d4 in
      let t03 := lxor64 a03 d0 in let t13 := lxor64 a13 d1 in
      let t23 := lxor64 a23 d2 in let t33 := lxor64 a33 d3 in
      let t43 := lxor64 a43 d4 in
      let t04 := lxor64 a04 d0 in let t14 := lxor64 a14 d1 in
      let t24 := lxor64 a24 d2 in let t34 := lxor64 a34 d3 in
      let t44 := lxor64 a44 d4 in

      (* rho + pi: B[y][2x+3y] = rotl(A[x][y], r[x][y]) *)
      let b00 := rotl64 t00 0  in
      let b01 := rotl64 t30 28 in
      let b02 := rotl64 t10 1  in
      let b03 := rotl64 t40 27 in
      let b04 := rotl64 t20 62 in
      let b10 := rotl64 t11 44 in
      let b11 := rotl64 t41 20 in
      let b12 := rotl64 t21 6  in
      let b13 := rotl64 t01 36 in
      let b14 := rotl64 t31 55 in
      let b20 := rotl64 t22 43 in
      let b21 := rotl64 t02 3  in
      let b22 := rotl64 t32 25 in
      let b23 := rotl64 t12 10 in
      let b24 := rotl64 t42 39 in
      let b30 := rotl64 t33 21 in
      let b31 := rotl64 t13 45 in
      let b32 := rotl64 t43 8  in
      let b33 := rotl64 t23 15 in
      let b34 := rotl64 t03 41 in
      let b40 := rotl64 t44 14 in
      let b41 := rotl64 t24 61 in
      let b42 := rotl64 t04 18 in
      let b43 := rotl64 t34 56 in
      let b44 := rotl64 t14 2  in

      (* chi: A'[x][y] = B[x][y] xor ((not B[x+1][y]) and B[x+2][y]) *)
      let o00 := lxor64 b00 (land64 (lnot64 b10) b20) in
      let o10 := lxor64 b10 (land64 (lnot64 b20) b30) in
      let o20 := lxor64 b20 (land64 (lnot64 b30) b40) in
      let o30 := lxor64 b30 (land64 (lnot64 b40) b00) in
      let o40 := lxor64 b40 (land64 (lnot64 b00) b10) in
      let o01 := lxor64 b01 (land64 (lnot64 b11) b21) in
      let o11 := lxor64 b11 (land64 (lnot64 b21) b31) in
      let o21 := lxor64 b21 (land64 (lnot64 b31) b41) in
      let o31 := lxor64 b31 (land64 (lnot64 b41) b01) in
      let o41 := lxor64 b41 (land64 (lnot64 b01) b11) in
      let o02 := lxor64 b02 (land64 (lnot64 b12) b22) in
      let o12 := lxor64 b12 (land64 (lnot64 b22) b32) in
      let o22 := lxor64 b22 (land64 (lnot64 b32) b42) in
      let o32 := lxor64 b32 (land64 (lnot64 b42) b02) in
      let o42 := lxor64 b42 (land64 (lnot64 b02) b12) in
      let o03 := lxor64 b03 (land64 (lnot64 b13) b23) in
      let o13 := lxor64 b13 (land64 (lnot64 b23) b33) in
      let o23 := lxor64 b23 (land64 (lnot64 b33) b43) in
      let o33 := lxor64 b33 (land64 (lnot64 b43) b03) in
      let o43 := lxor64 b43 (land64 (lnot64 b03) b13) in
      let o04 := lxor64 b04 (land64 (lnot64 b14) b24) in
      let o14 := lxor64 b14 (land64 (lnot64 b24) b34) in
      let o24 := lxor64 b24 (land64 (lnot64 b34) b44) in
      let o34 := lxor64 b34 (land64 (lnot64 b44) b04) in
      let o44 := lxor64 b44 (land64 (lnot64 b04) b14) in

      (* iota *)
      (lxor64 o00 rc)::o10::o20::o30::o40::
      o01::o11::o21::o31::o41::
      o02::o12::o22::o32::o42::
      o03::o13::o23::o33::o43::
      o04::o14::o24::o34::o44::nil

  (* Total. A state of the wrong width is returned unchanged; every caller in
     this development builds the state from exactly 200 bytes. *)
  | _ => st
  end.

Fixpoint keccak_rounds (rcs : list N) (st : list N) : list N :=
  match rcs with
  | nil => st
  | rc :: rest => keccak_rounds rest (keccak_round rc st)
  end.

Definition keccak_f1600 (st : list N) : list N := keccak_rounds keccak_rc st.

(* ------------------------------------------------------------------ *)
(** * Part B — bytes, sponge, vectors                                  *)
(* ------------------------------------------------------------------ *)

Local Open Scope nat_scope.

(** ** Byte / lane conversion (little-endian lanes, FIPS 202 §3.1.2) *)

Fixpoint lane_of_bytes_go (bs : list byte) (sh : N) (acc : N) : N :=
  match bs with
  | nil => acc
  | b :: rest =>
      lane_of_bytes_go rest (sh + 8)%N (N.lor acc (N.shiftl (N.of_nat b) sh))
  end.

Definition lane_of_bytes (bs : list byte) : N := lane_of_bytes_go bs 0%N 0%N.

Definition byte_at (x : N) (i : N) : byte :=
  N.to_nat (N.land (N.shiftr x (8 * i)%N) 255%N).

Definition lane_to_bytes (x : N) : list byte :=
  [ byte_at x 0%N; byte_at x 1%N; byte_at x 2%N; byte_at x 3%N
  ; byte_at x 4%N; byte_at x 5%N; byte_at x 6%N; byte_at x 7%N ].

(** Split a byte string into [n] lanes of 8 bytes each. Short input yields
    fewer lanes; callers supply exactly [8*n] bytes. *)
Fixpoint lanes_of_bytes (n : nat) (bs : list byte) : list N :=
  match n with
  | O => nil
  | S n' =>
      match bs with
      | b0::b1::b2::b3::b4::b5::b6::b7::rest =>
          lane_of_bytes [b0;b1;b2;b3;b4;b5;b6;b7] :: lanes_of_bytes n' rest
      | _ => nil
      end
  end.

Fixpoint bytes_of_lanes (ls : list N) : list byte :=
  match ls with
  | nil => nil
  | x :: rest => lane_to_bytes x ++ bytes_of_lanes rest
  end.

(** ** SHA3-256

    Rate 136 bytes, capacity 64 bytes, domain suffix [0x06].

    [0x06], not [0x01]: SHA3-256 (FIPS 202) and Keccak-256 (the pre-standard
    variant) differ only in this byte and produce entirely different digests.
    SPEC-008 §2 makes this constant consensus-critical.

    This development only ever hashes messages that fit one rate block, which
    is the point of the SPEC-008 construction: absorption is a single
    permutation and squeezing is a prefix read. [sha3_256_pad] is therefore
    defined for [length msg <= 134]. At length 135 the suffix and the final
    [0x80] would share a byte (the merged [0x86] case); SPEC-008 §3.3 forbids
    implementing that case, and it is unreachable here. *)

Definition sha3_rate : nat := 136.

Definition sha3_256_pad (msg : list byte) : list byte :=
  msg ++ [6] ++ repeat_byte (134 - length msg) 0 ++ [128].

Definition sha3_256_block (msg : list byte) : list byte :=
  sha3_256_pad msg ++ repeat_byte 64 0.

(** Digest of a message of at most 134 bytes. *)
Definition sha3_256_short (msg : list byte) : list byte :=
  let st := keccak_f1600 (lanes_of_bytes 25 (sha3_256_block msg)) in
  firstn 32 (bytes_of_lanes st).

(** ** Golden vectors (FIPS 202)

    These discharge by [vm_compute]: Coq evaluates the permutation on
    concrete bytes and checks all 32 output bytes. A regression breaks the
    build. *)

Lemma sha3_256_empty :
  sha3_256_short [] =
  [167; 255; 198; 248; 191; 30; 215; 102; 81; 193; 71; 86; 160; 97; 214; 98;
   245; 128; 255; 77; 228; 59; 73; 250; 130; 216; 10; 75; 128; 248; 67; 74].
Proof. vm_compute. reflexivity. Qed.

Lemma sha3_256_abc :
  sha3_256_short [97; 98; 99] =
  [58; 152; 93; 167; 79; 226; 37; 178; 4; 92; 23; 45; 107; 211; 144; 189;
   133; 95; 8; 110; 62; 157; 82; 91; 70; 191; 226; 69; 17; 67; 21; 50].
Proof. vm_compute. reflexivity. Qed.

(** Block-boundary probe. 134 bytes is the longest message this single-block
    construction accepts; RIGOR's own preimage is 88, so this vector guards an
    edge the protocol never reaches. *)
Lemma sha3_256_len134 :
  sha3_256_short (seq 0 134) =
  [100; 78; 21; 34; 79; 85; 151; 53; 26; 239; 92; 75; 221; 34; 178; 124;
   160; 193; 157; 178; 36; 68; 49; 83; 76; 42; 74; 11; 235; 253; 243; 156].
Proof. vm_compute. reflexivity. Qed.

(** ** Structural facts *)

Lemma keccak_rc_length : length keccak_rc = 24.
Proof. reflexivity. Qed.

Lemma lane_to_bytes_length : forall x, length (lane_to_bytes x) = 8.
Proof. reflexivity. Qed.

Lemma repeat_byte_length : forall n v, length (repeat_byte n v) = n.
Proof.
  induction n as [|n IH]; intros v; simpl; [reflexivity|].
  rewrite IH. reflexivity.
Qed.

Lemma sha3_256_pad_length :
  forall msg, length msg <= 134 -> length (sha3_256_pad msg) = 136.
Proof.
  intros msg H. unfold sha3_256_pad.
  rewrite !app_length. rewrite repeat_byte_length.
  (* [cbn [length]], not [simpl]: [simpl] expands the numeral 134 to unary
     successors and unfolds [Nat.sub] into nested matches, which [lia] cannot
     read. Restricting reduction to [length] leaves [134 - length msg] intact. *)
  cbn [length]. lia.
Qed.

Lemma sha3_256_block_length :
  forall msg, length msg <= 134 -> length (sha3_256_block msg) = 200.
Proof.
  intros msg H. unfold sha3_256_block.
  rewrite app_length. rewrite repeat_byte_length.
  rewrite (sha3_256_pad_length msg H). reflexivity.
Qed.

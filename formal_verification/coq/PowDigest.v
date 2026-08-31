(** RIGOR proof-of-work digest (SPEC-008).

        pow_digest = SHA3-256( "RGRPOW01" || header_80 )
        valid      <-> pow_digest <= target(nBits)

    The preimage is 88 bytes. SHA3-256 has rate 136, so 88 bytes pad into
    exactly one rate block and the 32-byte digest is read without a second
    squeeze: computing this digest is *one* invocation of Keccak-f[1600].
    [pow_single_permutation] below states that as a checked fact rather than
    a claim in prose.

    Note what changed against OpuHash. OpuHash was a function of the 32-byte
    header id, [GetOpusHashFromBaseHash(header.GetHash())]. This digest is a
    function of the 80 raw header bytes. A [uint256] is therefore no longer
    enough to compute it, which is why the [CheckProofOfWork(uint256, ...)]
    overload is removed rather than merely deprecated.

    equivalence_claimed = false
    refinement_proved   = false
    This is the model. It is not proved equal to the C++ implementation. *)

From Coq Require Import List.
From Coq Require Import Lia.
From Coq Require Import ZArith.ZArith.
From Coq Require Import NArith.NArith.
From Rigor Require Import RigorBytes.
From Rigor Require Import RigorKeccak.
Import ListNotations.

Local Open Scope nat_scope.

(** * The domain tag

    ASCII "RGRPOW01". Prefixing it means a RIGOR PoW preimage can never
    coincide with a bare-header hash computed by any other protocol, and it
    keeps the PoW digest disjoint from every other SHA3 use in the system. *)

Definition rigor_pow_domain : list byte := [82; 71; 82; 80; 79; 87; 48; 49].

Lemma rigor_pow_domain_length : length rigor_pow_domain = 8.
Proof. reflexivity. Qed.

Definition rigor_pow_preimage (header80 : list byte) : list byte :=
  rigor_pow_domain ++ header80.

Definition rigor_pow_digest (header80 : list byte) : list byte :=
  sha3_256_short (rigor_pow_preimage header80).

(** * Well-formed headers *)

Definition header_wf (header80 : list byte) : Prop := length header80 = 80.

Lemma preimage_length :
  forall h, header_wf h -> length (rigor_pow_preimage h) = 88.
Proof.
  intros h H. unfold rigor_pow_preimage, header_wf in *.
  rewrite app_length, rigor_pow_domain_length, H. reflexivity.
Qed.

(** ** The single-permutation property (SPEC-008 §3.3)

    88 <= 134, so the padded message is one rate block; 200 bytes is the full
    Keccak state, absorbed and permuted once. *)

Theorem pow_single_permutation :
  forall h, header_wf h -> length (sha3_256_block (rigor_pow_preimage h)) = 200.
Proof.
  intros h H. apply sha3_256_block_length.
  rewrite (preimage_length h H). lia.
Qed.

(** * Shape preservation through the permutation *)

Lemma keccak_round_length :
  forall rc st, length st = 25 -> length (keccak_round rc st) = 25.
Proof.
  intros rc st H.
  do 25 (destruct st as [|? st]; [cbn in H; lia|]).
  destruct st as [|? st]; [reflexivity | cbn in H; lia].
Qed.

Lemma keccak_rounds_length :
  forall rcs st, length st = 25 -> length (keccak_rounds rcs st) = 25.
Proof.
  induction rcs as [|rc rest IH]; intros st H; [exact H|].
  cbn [keccak_rounds]. apply IH. apply keccak_round_length. exact H.
Qed.

Lemma keccak_f1600_length :
  forall st, length st = 25 -> length (keccak_f1600 st) = 25.
Proof. intros st H. apply keccak_rounds_length. exact H. Qed.

Lemma lanes_of_bytes_length :
  forall n bs, length bs = 8 * n -> length (lanes_of_bytes n bs) = n.
Proof.
  induction n as [|n IH]; intros bs H; [reflexivity|].
  destruct bs as [|b0 [|b1 [|b2 [|b3 [|b4 [|b5 [|b6 [|b7 rest]]]]]]]];
    try (cbn in H; lia).
  cbn [lanes_of_bytes length]. f_equal. apply IH. cbn in H. lia.
Qed.

Lemma bytes_of_lanes_length :
  forall ls, length (bytes_of_lanes ls) = 8 * length ls.
Proof.
  induction ls as [|x rest IH]; [reflexivity|].
  cbn [bytes_of_lanes length]. rewrite app_length, lane_to_bytes_length, IH. lia.
Qed.

(** The digest is 32 bytes for every well-formed header. *)
Theorem pow_digest_length :
  forall h, header_wf h -> length (rigor_pow_digest h) = 32.
Proof.
  intros h H.
  unfold rigor_pow_digest, sha3_256_short.
  rewrite firstn_length, bytes_of_lanes_length.
  rewrite keccak_f1600_length.
  - reflexivity.
  - apply lanes_of_bytes_length. rewrite pow_single_permutation by exact H. lia.
Qed.

(** * Validity

    [uint256_of_bytes] is the little-endian interpretation that matches
    [arith_uint256] on the C++ side: digest byte 0 is the least significant.
    Getting this backwards is the classic way a PoW model and its
    implementation silently disagree, so the byte order is pinned by the
    vector [pow_digest_zero_header_le] below and not left to prose. *)

Definition target_ok (digest : list byte) (target : Z) : bool :=
  Z.leb (uint256_of_bytes digest) target.

Definition pow_valid (header80 : list byte) (target : Z) : bool :=
  target_ok (rigor_pow_digest header80) target.

(** Raising the target never invalidates a digest. This is the property the
    difficulty layer relies on, and it is a property of the *comparison*, not
    of Keccak, so it is stated over an abstract digest. *)
Theorem target_ok_monotone :
  forall d t1 t2,
    (t1 <= t2)%Z ->
    target_ok d t1 = true ->
    target_ok d t2 = true.
Proof.
  intros d t1 t2 Hle Hv. unfold target_ok in *.
  apply Z.leb_le. apply Z.leb_le in Hv.
  exact (Z.le_trans _ _ _ Hv Hle).
Qed.

(** ** Reduction boundary — read before extending this module

    Do not state a lemma that forces weak-head reduction of
    [uint256_of_bytes (rigor_pow_digest h)] for a *symbolic* [h]. Coq's
    conversion machine does not terminate on it in practice.

    Why: [Z.leb] and [uint256_of_bytes] must expose a constructor, so the
    evaluator is driven into [keccak_f1600], which is 24 nested applications
    of a 25-way [match]. With [h] symbolic, [lanes_of_bytes] gets stuck on the
    9th byte, so no [match] ever fires and every enclosing round re-evaluates
    the stuck argument. The result is not an error but a non-terminating
    conversion.

    Measured on this tree (Coq 8.15): [lia], [exact], and [Qed] each ran past
    200s and were not interruptible by [Timeout]. All three failures are the
    same root cause, not three separate problems. [generalize] over the digest
    does not help, because the kernel re-checks the instantiated term at [Qed].

    What does work:
      - statements about [length], which never force the byte contents
        ([pow_digest_length] below);
      - statements over an abstract digest ([target_ok_monotone] above);
      - [vm_compute] on a *concrete* header, which is total and fast
        (the golden vectors below).

    So this model is an executable oracle on concrete inputs, not a symbolic
    rewriting target. The SPEC-008 differential harness must be built on
    concrete vectors. *)

(** * Golden vectors *)

Definition zero_header : list byte := repeat_byte 80 0.

Lemma zero_header_wf : header_wf zero_header.
Proof. reflexivity. Qed.

(** SHA3-256("RGRPOW01" || 80 zero bytes). Cross-checked against a reference
    SHA3-256 implementation; the C++ side must reproduce these 32 bytes. *)
Lemma pow_digest_zero_header :
  rigor_pow_digest zero_header =
  [221; 5; 127; 193; 223; 143; 91; 148; 102; 232; 52; 204; 157; 123; 129; 158;
   45; 81; 28; 117; 180; 54; 117; 181; 119; 246; 217; 245; 214; 222; 4; 252].
Proof. vm_compute. reflexivity. Qed.

(** The same digest read as a little-endian 256-bit integer, i.e. the value
    that is actually compared against the target. This pins the byte order. *)
Lemma pow_digest_zero_header_le :
  uint256_of_bytes (rigor_pow_digest zero_header) =
  113991443214250993822008262451400111709481499196443645548532729518767640348125%Z.
Proof. vm_compute. reflexivity. Qed.

(** The domain tag is load-bearing: dropping it yields a different digest.
    Proved by computation, so it cannot rot. *)
Lemma pow_domain_is_load_bearing :
  rigor_pow_digest zero_header <> sha3_256_short zero_header.
Proof. vm_compute. discriminate. Qed.

(** Sanity on the validity predicate at the extremes. *)
Lemma pow_valid_zero_target_false :
  pow_valid zero_header 0%Z = false.
Proof. vm_compute. reflexivity. Qed.

Lemma pow_valid_max_target_true :
  pow_valid zero_header (Z.shiftl 1 256 - 1)%Z = true.
Proof. vm_compute. reflexivity. Qed.

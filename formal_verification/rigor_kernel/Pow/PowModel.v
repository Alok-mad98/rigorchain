(** RigorKernel.Pow.PowModel — Phase A abstract PoW predicate.

    Scope: consensus kernel only.

    Header digest is [H (serialize header)]. [H] stays uninterpreted.
    No ASERT. No Keccak. No compact-nBits codec.

    NOT CLAIMED:
      C++ implementation equivalence
      Full chain validity
      SHA3-256 / "RGRPOW01" concrete evaluation
      ASERT difficulty transition
*)
From Coq Require Import List.
From Coq Require Import ZArith.ZArith.
From Coq Require Import Lia.
Import ListNotations.
From RigorKernel Require Import Core.Bytes.
From RigorKernel Require Import Pow.AbstractHash.

Local Open Scope Z_scope.

Record header := {
  hd_version : Z;
  hd_prev : list byte;
  hd_merkle : list byte;
  hd_time : Z;
  hd_bits : Z;
  hd_nonce : Z;
  hd_target : Z
}.

Definition serialize_header (h : header) : list byte :=
  app_bytes (le32_bytes (hd_version h))
    (app_bytes (hd_prev h)
      (app_bytes (hd_merkle h)
        (app_bytes (le32_bytes (hd_time h))
          (app_bytes (le32_bytes (hd_bits h))
                     (le32_bytes (hd_nonce h)))))).

Section PowModel.

Variable H : list byte -> list byte.

Definition header_digest (h : header) : list byte :=
  H (serialize_header h).

Definition valid_pow (h : header) : Prop :=
  hash_le_target (header_digest h) (hd_target h).

Definition same_hash_result (h : header) : Prop :=
  header_digest h = header_digest h.

(** Statement: a header that satisfies [valid_pow] produces a
    determinate digest — the same header hashes the same way.
    Assumptions: [valid_pow header] (unused; follows from [H]
    being a function).
    Scope: consensus kernel only. *)
Theorem pow_same_header_same_result :
  forall header,
    valid_pow header ->
    same_hash_result header.
Proof.
  intros hdr Hv. unfold same_hash_result. reflexivity.
Qed.

(** Statement: equal headers produce equal digests.
    Assumptions: [h1 = h2].
    Scope: consensus kernel only. *)
Theorem pow_header_deterministic :
  forall h1 h2 : header,
    h1 = h2 -> header_digest h1 = header_digest h2.
Proof. intros h1 h2 Heq. rewrite Heq. reflexivity. Qed.

(** Statement: a digest strictly above target is rejected.
    Assumptions: [digest_as_Z (header_digest h) > hd_target h]
    or [hd_target h <= 0].
    Scope: consensus kernel only. *)
Theorem pow_rejects_above_target :
  forall h : header,
    digest_as_Z (header_digest h) > hd_target h ->
    ~ valid_pow h.
Proof.
  intros h Hgt. unfold valid_pow, hash_le_target. lia.
Qed.

Theorem pow_rejects_nonpositive_target :
  forall h : header,
    hd_target h <= 0 ->
    ~ valid_pow h.
Proof.
  intros h Hle. unfold valid_pow, hash_le_target. lia.
Qed.

End PowModel.

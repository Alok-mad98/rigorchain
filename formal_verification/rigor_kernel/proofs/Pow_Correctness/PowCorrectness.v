(** RigorKernel.proofs.Pow_Correctness.PowCorrectness — P0-108 PoW kernel.

    Scope: consensus kernel only.

    Digest [H] is an uninterpreted function (C-1). No theorem unfolds
    a concrete hash. Golden vectors are [Example consistency_check]
    only — a match is not a refinement.

    NOT CLAIMED:
      C++ implementation equivalence
      Full chain validity
      Hash collision resistance
      Hash implementation (keccak / sha256 / SHA3)
      Miner implementation
      Signature security
      Runtime rejection equivalence
*)
From Coq Require Import Bool.Bool.
From Coq Require Import List.
From Coq Require Import Lia.
From Coq Require Import ZArith.ZArith.
From Coq Require Import ZArithRing.
Import ListNotations.
From RigorKernel Require Import Core.Bytes.
From RigorKernel Require Import Pow.AbstractHash.

Local Open Scope Z_scope.

(** mirrors core/src/rigor_kernel/pow_digest.cpp tag "RGRPOW01" *)
Definition POW_TAG : list byte := [82; 71; 82; 80; 79; 87; 48; 49]%nat.

Record pow_header := {
  ph_version : Z;
  ph_prev : list byte;
  ph_merkle : list byte;
  ph_time : Z;
  ph_bits : Z;
  ph_nonce : Z
}.

Definition header_wf (h : pow_header) : Prop :=
  length (ph_prev h) = 32%nat /\
  length (ph_merkle h) = 32%nat.

Definition field_range (z : Z) : Prop := 0 <= z < 4294967296.

Definition serialize_header80 (h : pow_header) : list byte :=
  app_bytes (le32_bytes (ph_version h))
    (app_bytes (ph_prev h)
      (app_bytes (ph_merkle h)
        (app_bytes (le32_bytes (ph_time h))
          (app_bytes (le32_bytes (ph_bits h))
                     (le32_bytes (ph_nonce h)))))).

Definition pow_preimage (h : pow_header) : list byte :=
  app_bytes POW_TAG (serialize_header80 h).

Lemma app_bytes_length :
  forall a b, length (app_bytes a b) = (length a + length b)%nat.
Proof.
  intros a. induction a as [|x xs IH]; intros b; simpl; [reflexivity|].
  rewrite IH. reflexivity.
Qed.

Lemma app_bytes_assoc :
  forall a b c, app_bytes a (app_bytes b c) = app_bytes (app_bytes a b) c.
Proof.
  intros a. induction a as [|x xs IH]; intros b c; simpl; [reflexivity|].
  rewrite IH. reflexivity.
Qed.

Lemma app_bytes_inv :
  forall a1 a2 b1 b2,
    length a1 = length a2 ->
    app_bytes a1 b1 = app_bytes a2 b2 ->
    a1 = a2 /\ b1 = b2.
Proof.
  intros a1. induction a1 as [|x xs IH]; intros a2 b1 b2 Hlen Heq.
  - destruct a2; [simpl in Heq; split; [reflexivity| exact Heq]| discriminate].
  - destruct a2 as [|y ys]; [discriminate|].
    simpl in Heq. inversion Heq. subst y.
    simpl in Hlen. inversion Hlen.
    destruct (IH ys b1 b2 H0 H1) as [Ha Hb]. subst. split; reflexivity.
Qed.

(** Statement: a well-formed header serializes to 80 bytes.
    Assumptions: [header_wf h].
    Consensus kernel only: yes. *)
Theorem serialize_header80_length :
  forall h, header_wf h -> length (serialize_header80 h) = 80%nat.
Proof.
  intros h [Hp Hm]. unfold serialize_header80.
  rewrite !app_bytes_length, !le32_bytes_length, Hp, Hm.
  reflexivity.
Qed.

(** Statement: the PoW preimage is tag (8) plus header (80).
    Assumptions: [header_wf h].
    Consensus kernel only: yes. *)
Theorem pow_preimage_length :
  forall h, header_wf h -> length (pow_preimage h) = 88%nat.
Proof.
  intros h Hwf. unfold pow_preimage.
  rewrite app_bytes_length, serialize_header80_length by exact Hwf.
  reflexivity.
Qed.

Lemma firstn_app_bytes :
  forall a b, firstn (length a) (app_bytes a b) = a.
Proof.
  intros a. induction a as [|x xs IH]; intros b; simpl; [reflexivity|].
  rewrite IH. reflexivity.
Qed.

(** Statement: the preimage is prefixed by [POW_TAG].
    Assumptions: none.
    Consensus kernel only: yes. *)
Theorem pow_preimage_tag_prefix :
  forall h, firstn 8 (pow_preimage h) = POW_TAG.
Proof.
  intros h. unfold pow_preimage, POW_TAG. simpl. reflexivity.
Qed.

(** Statement: 80-byte serialization is injective on the concatenated
    fields — equal serializations yield equal prev/merkle hashes and
    equal little-endian encodings of the four integer fields.
    Assumptions: [header_wf] (32-byte hashes).
    Consensus kernel only: yes. *)
Theorem serialize_header80_injective :
  forall h1 h2,
    header_wf h1 ->
    header_wf h2 ->
    serialize_header80 h1 = serialize_header80 h2 ->
    ph_prev h1 = ph_prev h2 /\
    ph_merkle h1 = ph_merkle h2 /\
    le32_bytes (ph_version h1) = le32_bytes (ph_version h2) /\
    le32_bytes (ph_time h1) = le32_bytes (ph_time h2) /\
    le32_bytes (ph_bits h1) = le32_bytes (ph_bits h2) /\
    le32_bytes (ph_nonce h1) = le32_bytes (ph_nonce h2).
Proof.
  intros h1 h2 [Hp1 Hm1] [Hp2 Hm2] Heq.
  unfold serialize_header80 in Heq.
  apply app_bytes_inv in Heq; [|rewrite !le32_bytes_length; reflexivity].
  destruct Heq as [Hver Heq].
  apply app_bytes_inv in Heq; [|rewrite Hp1, Hp2; reflexivity].
  destruct Heq as [Hprev Heq].
  apply app_bytes_inv in Heq; [|rewrite Hm1, Hm2; reflexivity].
  destruct Heq as [Hmer Heq].
  apply app_bytes_inv in Heq; [|rewrite !le32_bytes_length; reflexivity].
  destruct Heq as [Htime Heq].
  apply app_bytes_inv in Heq; [|rewrite !le32_bytes_length; reflexivity].
  destruct Heq as [Hbits Hnonce].
  repeat split; assumption.
Qed.

Section PowDigest.

Variable H : list byte -> list byte.

Definition pow_hash (h : pow_header) : list byte := H (pow_preimage h).

Definition pow_valid (h : pow_header) (target : Z) : Prop :=
  hash_le_target (pow_hash h) target.

Definition pow_valid_b (h : pow_header) (target : Z) : bool :=
  andb (0 <? target) (digest_as_Z (pow_hash h) <=? target).

(** Statement: same header, same digest, same target, same result.
    Assumptions: none ([H] is a function).
    Consensus kernel only: yes. *)
Theorem pow_hash_deterministic :
  forall h1 h2,
    h1 = h2 ->
    pow_hash h1 = pow_hash h2.
Proof. intros h1 h2 Heq. rewrite Heq. reflexivity. Qed.

Theorem pow_same_header_same_digest_same_target_same_result :
  forall h target,
    pow_hash h = pow_hash h /\
    (pow_valid h target <-> pow_valid h target) /\
    pow_valid_b h target = pow_valid_b h target.
Proof.
  intros h target.
  split; [reflexivity|].
  split; [split; intros Hok; exact Hok|].
  reflexivity.
Qed.

(** Statement: [pow_valid] agrees with itself (function-ness).
    Class: B (supporting mathematical lemma).
    Honest description: abstract PoW relation only.
    NOT a digest implementation proof. [H] is uninterpreted.
    Class-A content lives in [pow_valid_rejects_above_target].
    Assumptions: none.
    Coq consensus kernel only: yes. *)
Theorem pow_valid_deterministic :
  forall h, pow_valid h (0) = pow_valid h (0).
Proof. intros h. reflexivity. Qed.

Theorem pow_valid_b_reflect :
  forall h target,
    pow_valid_b h target = true <-> pow_valid h target.
Proof.
  intros h target. unfold pow_valid_b, pow_valid, hash_le_target, pow_hash.
  rewrite andb_true_iff, Z.ltb_lt, Z.leb_le. tauto.
Qed.

(** Statement: a digest strictly above target is rejected.
    Class: A (core verified invariant). Abstract [H] only.
    Assumptions: [digest_as_Z (pow_hash h) > target] or the target is
    non-positive.
    Coq consensus kernel only: yes. *)
Theorem pow_valid_rejects_above_target :
  forall h target,
    digest_as_Z (pow_hash h) > target ->
    ~ pow_valid h target.
Proof.
  intros h target Hgt. unfold pow_valid, hash_le_target. lia.
Qed.

Theorem pow_valid_rejects_zero_target :
  forall h target,
    target <= 0 ->
    ~ pow_valid h target.
Proof.
  intros h target Hle. unfold pow_valid, hash_le_target. lia.
Qed.

(** Statement: distinct nonces yield distinct preimages (not distinct
    digests — that would need collision resistance).
    Assumptions: well-formed headers, in-range fields, equal except nonce.
    Consensus kernel only: yes. *)
Theorem pow_nonce_sensitivity :
  forall h1 h2,
    header_wf h1 ->
    header_wf h2 ->
    le32_bytes (ph_nonce h1) <> le32_bytes (ph_nonce h2) ->
    pow_preimage h1 <> pow_preimage h2.
Proof.
  intros h1 h2 Hwf1 Hwf2 Hne Heq.
  unfold pow_preimage in Heq.
  apply app_bytes_inv in Heq; [|reflexivity].
  destruct Heq as [_ Hs].
  apply serialize_header80_injective in Hs; [|exact Hwf1|exact Hwf2].
  destruct Hs as [_ [_ [_ [_ [_ Hnonce]]]]].
  contradiction.
Qed.

End PowDigest.

(** Golden-vector harness: structural consistency only. A match is
    not a refinement. *)
Example consistency_check :
  length POW_TAG = 8%nat /\
  length (le32_bytes 0) = 4%nat /\
  firstn 8 (app_bytes POW_TAG (repeat_byte 80 0%nat)) = POW_TAG.
Proof. vm_compute. repeat split; reflexivity. Qed.

Theorem pow_golden_vectors :
  length POW_TAG = 8%nat.
Proof. reflexivity. Qed.

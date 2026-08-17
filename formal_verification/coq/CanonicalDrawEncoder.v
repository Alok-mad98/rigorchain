(** Canonical Draw Encoder — exact byte layout of the σ preimage.

    This is the specification both Coq and C++ must conform to. It does not
    prove that the C++ memcpy path equals this function (that would be a
    refinement proof). It pins the layout so that any disagreement is a
    detectable vector failure rather than an ambiguous comment.

    Layout (100 bytes, no padding, no separators):

        offset  size  field
        ------  ----  ---------------------------------------------
        0       32    txid_i          native on-the-wire byte order
        32      32    ConvergenceRoot C over F
        64      32    B_{h-1}         parent header hash, verbatim
        96      4     height          uint32 little-endian (payout h)

    Hash: SHA-256d (Bitcoin Hash256). There is no target_prev field.

    C++: core/src/consensus_rigor/sigma_hash.h and sigma_hash.cpp
    Python: formal_verification/spec/rigor_consensus_spec.py SIGMA_OFF constants
    Coq: SigmaPoW01.sigma_preimage_bytes, reused here with offset theorems. *)

From Coq Require Import Arith.Arith.
From Coq Require Import List.
From Coq Require Import Lia.
From Coq Require Import ZArith.ZArith.
Import ListNotations.

From Rigor Require Import RigorBytes.
Include RigorBytes.
Require Import Rigor.RigorHash.
Require Import Rigor.SigmaPoW01.

Definition SIGMA_INPUT_SIZE : nat := 100.
Definition SIGMA_OFF_TXID : nat := 0.
Definition SIGMA_OFF_CROOT : nat := 32.
Definition SIGMA_OFF_PREV_BLOCK : nat := 64.
Definition SIGMA_OFF_HEIGHT : nat := 96.
Definition SIGMA_HEIGHT_LEN : nat := 4.
Definition SIGMA_FIELD_WIDTH : nat := 32.

Lemma sigma_layout_offsets :
  SIGMA_OFF_TXID = 0%nat /\
  SIGMA_OFF_CROOT = 32%nat /\
  SIGMA_OFF_PREV_BLOCK = 64%nat /\
  SIGMA_OFF_HEIGHT = 96%nat /\
  SIGMA_HEIGHT_LEN = 4%nat /\
  (SIGMA_OFF_HEIGHT + SIGMA_HEIGHT_LEN = SIGMA_INPUT_SIZE)%nat.
Proof. repeat split; reflexivity. Qed.

Lemma sigma_layout_fields_abut :
  (SIGMA_OFF_TXID + SIGMA_FIELD_WIDTH = SIGMA_OFF_CROOT)%nat /\
  (SIGMA_OFF_CROOT + SIGMA_FIELD_WIDTH = SIGMA_OFF_PREV_BLOCK)%nat /\
  (SIGMA_OFF_PREV_BLOCK + SIGMA_FIELD_WIDTH = SIGMA_OFF_HEIGHT)%nat.
Proof. repeat split; reflexivity. Qed.

Lemma app_bytes_length :
  forall a b, length (app_bytes a b) = (length a + length b)%nat.
Proof.
  induction a as [|x xs IH]; intros b; simpl.
  - reflexivity.
  - rewrite IH. reflexivity.
Qed.

Lemma firstn_app_bytes :
  forall n a b,
    length a = n -> firstn n (app_bytes a b) = a.
Proof.
  intros n a. revert n.
  induction a as [|x xs IH]; intros n b Hlen; simpl in *.
  - subst n. reflexivity.
  - destruct n as [|n]; [discriminate|].
    injection Hlen as Hlen.
    simpl. f_equal. apply IH. exact Hlen.
Qed.

Lemma skipn_app_bytes :
  forall n a b,
    length a = n -> skipn n (app_bytes a b) = b.
Proof.
  intros n a. revert n.
  induction a as [|x xs IH]; intros n b Hlen; simpl in *.
  - subst n. reflexivity.
  - destruct n as [|n]; [discriminate|].
    injection Hlen as Hlen.
    simpl. apply IH. exact Hlen.
Qed.

Lemma le32_bytes_length : forall z, length (le32_bytes z) = 4%nat.
Proof. intros. reflexivity. Qed.

Lemma sigma_preimage_length :
  forall txid C B h,
    length txid = 32%nat ->
    length C = 32%nat ->
    length B = 32%nat ->
    length (sigma_preimage_bytes txid C B h) = SIGMA_INPUT_SIZE.
Proof.
  intros txid C B h Ht Hc Hb.
  unfold sigma_preimage_bytes, app_bytes32, SIGMA_INPUT_SIZE.
  rewrite !app_bytes_length, le32_bytes_length, Ht, Hc, Hb.
  reflexivity.
Qed.

Lemma sigma_preimage_txid_at_0 :
  forall txid C B h,
    length txid = 32%nat ->
    firstn 32 (sigma_preimage_bytes txid C B h) = txid.
Proof.
  intros txid C B h Ht.
  unfold sigma_preimage_bytes, app_bytes32.
  apply firstn_app_bytes. exact Ht.
Qed.

Lemma sigma_preimage_croot_at_32 :
  forall txid C B h,
    length txid = 32%nat ->
    length C = 32%nat ->
    firstn 32 (skipn 32 (sigma_preimage_bytes txid C B h)) = C.
Proof.
  intros txid C B h Ht Hc.
  unfold sigma_preimage_bytes, app_bytes32.
  rewrite skipn_app_bytes; [|exact Ht].
  apply firstn_app_bytes. exact Hc.
Qed.

Lemma skipn_plus :
  forall n m (l : list byte), skipn (n + m) l = skipn m (skipn n l).
Proof.
  intros n m l. revert m l.
  induction n as [|n IH]; intros m l; simpl.
  - reflexivity.
  - destruct l as [|x xs]; simpl.
    + destruct m; reflexivity.
    + apply IH.
Qed.

Lemma sigma_preimage_prev_at_64 :
  forall txid C B h,
    length txid = 32%nat ->
    length C = 32%nat ->
    length B = 32%nat ->
    firstn 32 (skipn 64 (sigma_preimage_bytes txid C B h)) = B.
Proof.
  intros txid C B h Ht Hc Hb.
  unfold sigma_preimage_bytes, app_bytes32.
  change 64%nat with (32 + 32)%nat. rewrite skipn_plus.
  rewrite skipn_app_bytes; [|exact Ht].
  rewrite skipn_app_bytes; [|exact Hc].
  apply firstn_app_bytes. exact Hb.
Qed.

Lemma sigma_preimage_height_at_96 :
  forall txid C B h,
    length txid = 32%nat ->
    length C = 32%nat ->
    length B = 32%nat ->
    skipn 96 (sigma_preimage_bytes txid C B h) = le32_bytes h.
Proof.
  intros txid C B h Ht Hc Hb.
  unfold sigma_preimage_bytes, app_bytes32.
  change 96%nat with (32 + (32 + 32))%nat.
  rewrite skipn_plus. rewrite skipn_plus.
  rewrite skipn_app_bytes; [|exact Ht].
  rewrite skipn_app_bytes; [|exact Hc].
  rewrite skipn_app_bytes; [|exact Hb].
  reflexivity.
Qed.

(** Height 256 serialises little-endian as [0; 1; 0; 0], not big-endian. *)
Lemma le32_bytes_256 : le32_bytes 256 = [0; 1; 0; 0]%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma le32_bytes_1 : le32_bytes 1 = [1; 0; 0; 0]%nat.
Proof. vm_compute. reflexivity. Qed.

(** The encoder IS sigma_preimage_bytes: there is one construction, not two. *)
Definition canonical_sigma_preimage := sigma_preimage_bytes.
Definition canonical_sigma_digest := sigma_bytes.

Lemma canonical_sigma_is_sha256d :
  forall txid C B h,
    canonical_sigma_digest txid C B h =
    hash256_bytes (canonical_sigma_preimage txid C B h).
Proof. intros. reflexivity. Qed.

(** No target_prev slot exists: the preimage of four fields is 100 bytes, so
    there is nowhere to fold nBits without changing SIGMA_INPUT_SIZE. *)
Lemma canonical_encoder_has_no_target_prev_slot :
  (SIGMA_OFF_TXID + SIGMA_FIELD_WIDTH +
   SIGMA_FIELD_WIDTH + SIGMA_FIELD_WIDTH + SIGMA_HEIGHT_LEN = SIGMA_INPUT_SIZE)%nat.
Proof. reflexivity. Qed.

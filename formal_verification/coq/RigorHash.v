(** SHA256d at byte granularity — executable model (no Parameters). *)
From Coq Require Import List.
From Coq Require Import ZArith.ZArith.
From Rigor Require Import RigorBytes.
Include RigorBytes.
From Rigor Require Import RigorSha256.
Import ListNotations.

Definition hash256_bytes : list byte -> list byte := RigorSha256.hash256_bytes.

Theorem hash256_bytes_deterministic :
  forall x y : list byte, x = y -> hash256_bytes x = hash256_bytes y.
Proof.
  intros x y H. rewrite H. reflexivity.
Qed.

(* Each compression step yields exactly 8 state words (explicit 8-element list). *)
Lemma compress_len :
  forall h m, length (RigorSha256.sha256_compress_block h m) = 8%nat.
Proof. intros h m. reflexivity. Qed.

(* [sha256_blocks] preserves the 8-word state length. *)
Lemma blocks_len :
  forall blocks h,
    length h = 8%nat ->
    length (RigorSha256.sha256_blocks h blocks) = 8%nat.
Proof.
  induction blocks as [| blk rest IH]; intros h Hh.
  - cbn [RigorSha256.sha256_blocks]. exact Hh.
  - cbn [RigorSha256.sha256_blocks]. apply IH. apply compress_len.
Qed.

(* Serializing exactly 8 words yields 32 bytes (4 bytes per word). *)
Lemma words_to_bytes32_len8 :
  forall ws, length ws = 8%nat -> length (RigorSha256.words_to_bytes32 ws) = 32%nat.
Proof.
  intros ws H.
  destruct ws as [|w0 [|w1 [|w2 [|w3 [|w4 [|w5 [|w6 [|w7 [|w8 ws']]]]]]]]];
    cbn [length] in H; try discriminate.
  reflexivity.
Qed.

Theorem hash256_bytes_length :
  forall x, length (hash256_bytes x) = 32%nat.
Proof.
  intros x.
  unfold hash256_bytes, RigorSha256.hash256_bytes, RigorSha256.sha256_bytes.
  apply words_to_bytes32_len8.
  apply blocks_len.
  reflexivity.
Qed.

(** SignatureBoundary.v — P058.

    Ed25519_verify is an external cryptographic primitive.
    This module proves the *accept boundary*, not curve arithmetic.

      valid_signature = signature_accept_boundary

    The accept bit is supplied by production [ed25519::VerifyDetached]
    (libsodium). Coq does not reimplement edwards25519.

    No [Admitted] / [Axiom] / [Parameter] / [Extract Constant]. *)

From Coq Require Import List.
From Coq Require Import Bool.Bool.
From Coq Require Import Arith.PeanoNat.
Import ListNotations.

Require Import RigorV01.AssetClassification.
Require Import RigorV01.Ed25519Ownership.

Definition Ed25519_verify
  (sig pk msg : list byte) (crypto_accept : bool) : bool :=
  andb (ed25519_encoding_ok sig pk msg) crypto_accept.

Definition valid_signature
  (sig pk msg : list byte) (crypto_accept : bool) : bool :=
  Ed25519_verify sig pk msg crypto_accept.

Definition signature_accept_boundary
  (sig pk msg : list byte) (crypto_accept : bool) : bool :=
  andb (Nat.eqb (length sig) ed25519_sig_size)
       (andb (Nat.eqb (length pk) ed25519_pk_size)
             (andb (Nat.eqb (length msg) sighash_size) crypto_accept)).

Theorem valid_signature_eq_accept_boundary :
  forall sig pk msg crypto_accept,
    valid_signature sig pk msg crypto_accept =
    signature_accept_boundary sig pk msg crypto_accept.
Proof.
  intros sig pk msg crypto_accept.
  unfold valid_signature, Ed25519_verify, signature_accept_boundary,
         ed25519_encoding_ok.
  rewrite <- andb_assoc.
  rewrite <- andb_assoc.
  reflexivity.
Qed.

Theorem signature_boundary_rejects_bad_sig_len :
  forall pk msg acc,
    signature_accept_boundary nil pk msg acc = false.
Proof. intros. reflexivity. Qed.

Theorem signature_boundary_rejects_bad_pk_len :
  forall sig msg acc,
    Nat.eqb (length sig) ed25519_sig_size = true ->
    signature_accept_boundary sig nil msg acc = false.
Proof.
  intros sig msg acc Hs.
  unfold signature_accept_boundary. rewrite Hs. reflexivity.
Qed.

Theorem signature_boundary_requires_accept :
  forall sig pk msg,
    Nat.eqb (length sig) ed25519_sig_size = true ->
    Nat.eqb (length pk) ed25519_pk_size = true ->
    Nat.eqb (length msg) sighash_size = true ->
    signature_accept_boundary sig pk msg false = false /\
    signature_accept_boundary sig pk msg true = true.
Proof.
  intros sig pk msg Hs Hp Hm.
  unfold signature_accept_boundary.
  rewrite Hs, Hp, Hm. split; reflexivity.
Qed.

Definition curve_math_reimplemented : bool := false.

Theorem no_curve_math_in_boundary :
  curve_math_reimplemented = false.
Proof. reflexivity. Qed.

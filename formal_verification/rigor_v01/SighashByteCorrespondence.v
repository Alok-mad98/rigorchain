(** RIGOR v0.1 — sighash preimage byte correspondence (P0-040).

      C++ SignatureHash serialization
            ↓  finite golden preimage bytes (rigor_sighash_p0_018 / vectors)
      Coq [hash256_bytes] = SHA256d

    [hash256_bytes] is definitionally double SHA-256. The three P0-018
    preimages hash to the golden digests. C++ tests lock
    Hash(json_preimage) = SignatureHash = expected_hash on that set.

    Not ∀-tx VST of SignatureHash. Not libsodium / RFC8032 VST.
    Interpreter is not restored.

      sighash_refinement_proved = false

    No [Axiom] / [Admitted]. Production sighash C++ is not modified. *)
From Coq Require Import List.
Import ListNotations.
Require Import RigorV01.VectorAgreement.
From Rigor Require Import RigorSha256.
From Rigor Require Import RigorHash.

Definition sighash_refinement_proved : bool := false.

Theorem sighash_refinement_not_proved :
  sighash_refinement_proved = false.
Proof. reflexivity. Qed.

Theorem hash256_bytes_is_sha256d :
  forall m, hash256_bytes m = sha256_bytes (sha256_bytes m).
Proof. intros. reflexivity. Qed.

Theorem sighash_oru_preimage_sha256d :
  hash256_bytes vec_oru_preimage = vec_oru_digest.
Proof. exact vec_oru_hash. Qed.

Theorem sighash_res_preimage_sha256d :
  hash256_bytes vec_res_preimage = vec_res_digest.
Proof. exact vec_res_hash. Qed.

Theorem sighash_mint_preimage_sha256d :
  hash256_bytes vec_res_mint_preimage = vec_res_mint_digest.
Proof. exact vec_res_mint_hash. Qed.

Theorem sighash_preimage_sha256d_three_paths :
  hash256_bytes vec_oru_preimage = vec_oru_digest /\
  hash256_bytes vec_res_preimage = vec_res_digest /\
  hash256_bytes vec_res_mint_preimage = vec_res_mint_digest.
Proof. exact sighash_hash_vectors_agree. Qed.

(** Unproved ∀ obligation (P0-041). [Tx] is the C++ transaction/context
    argument, [cpp_preimage] the SignatureHash serialization and [cpp_digest]
    SignatureHash itself. Discharging this for the live serializer is the
    byte-level refinement; the vectors above witness it at three points only,
    and the C++ tests lock those three points as
    [SignatureHash = Hash(preimage) = expected_hash]. *)
Definition SignatureHash_refines_sha256d
    (Tx : Type)
    (cpp_preimage : Tx -> list byte)
    (cpp_digest : Tx -> list byte) : Prop :=
  forall tx : Tx, cpp_digest tx = hash256_bytes (cpp_preimage tx).

(** The obligation has content: a serializer/digest pair that disagrees on a
    single input refutes it. A [Prop] that no instance can fail would be
    decoration, not an obligation. *)
Theorem sighash_obligation_is_refutable :
  ~ SignatureHash_refines_sha256d unit (fun _ => nil) (fun _ => nil).
Proof. intro H. specialize (H tt). vm_compute in H. discriminate. Qed.

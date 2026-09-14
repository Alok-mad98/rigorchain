(** RigorKernel.Kernel.CryptoAssumptions — centralized crypto inventory.

    Scope: Coq consensus kernel only.

    Every hash / keccak / signature / PoW-digest assumption used by
    this cone is recorded here. These are *assumptions*, not security
    proofs.

    Encoding contracts and extraction source-identity live elsewhere
    (SerializationBoundary, ExtractionBoundary). They do not prove
    cryptographic primitives or a full extraction refinement.

    NOT CLAIMED:
      SHA3 / SHA256 / keccak collision or preimage resistance
      Ed25519 EUF-CMA or related-key security
      Concrete GetRigorPowHash / "RGRPOW01" digest values
      Miner implementation correctness
      C++ CheckProofOfWork equivalence
      Full extraction refinement
*)
From RigorKernel Require Import Audit.KernelNonClaims.

(** * Hash / keccak / SHA family — uninterpreted, not proved secure *)

(** PoW theorems quantify over an abstract [H]. No theorem unfolds
    keccak, SHA3-256, or SHA256d. Constraint C-1. *)
Definition pow_digest_is_abstract_H : bool := true.

Definition keccak_implementation_verified : bool := false.
Definition sha3_collision_resistance_proved : bool := false.
Definition sha256_collision_resistance_proved : bool := false.
Definition hash_preimage_resistance_proved : bool := false.

(** * Signatures *)

Definition ed25519_eufcma_proved : bool := false.
Definition signature_verifier_is_abstract : bool := true.

(** * PoW digest vs integer comparison *)

(** [check_pow] compares a *supplied* integer to a target. It does
    not evaluate a concrete hash. *)
Definition pow_checker_evaluates_concrete_hash : bool := false.

(** * Serialization / encoding — no crypto layer *)

Definition serialization_uses_cryptographic_hash : bool := false.

(** * Production refinement of crypto *)

Definition production_crypto_refinement : bool := false.

Theorem crypto_keccak_not_verified :
  keccak_implementation_verified = false.
Proof. reflexivity. Qed.

Theorem crypto_sha3_not_proved :
  sha3_collision_resistance_proved = false /\ sha3_security = false.
Proof. split; reflexivity. Qed.

Theorem crypto_sha256_not_proved :
  sha256_collision_resistance_proved = false.
Proof. reflexivity. Qed.

Theorem crypto_ed25519_not_proved :
  ed25519_eufcma_proved = false /\ ed25519_security = false.
Proof. split; reflexivity. Qed.

Theorem crypto_pow_hash_not_evaluated_by_checker :
  pow_checker_evaluates_concrete_hash = false
  /\ pow_digest_is_abstract_H = true.
Proof. split; reflexivity. Qed.

Theorem crypto_serialization_has_no_hash :
  serialization_uses_cryptographic_hash = false.
Proof. reflexivity. Qed.

Theorem crypto_assumptions_are_not_security_proofs :
  sha3_security = false
  /\ ed25519_security = false
  /\ crypto_security = false
  /\ keccak_implementation_verified = false
  /\ production_crypto_refinement = false.
Proof. repeat split; reflexivity. Qed.

Theorem pow_digest_remains_abstract :
  pow_digest_is_abstract_H = true.
Proof. reflexivity. Qed.

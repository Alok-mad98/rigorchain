(** RigorKernel.Audit.KernelAssumptions — explicit kernel assumptions.

    Scope: consensus kernel only.

    Records the modelling assumptions of the critical-consensus cone:
    abstract digest [H], draw eligibility as a hypothesis, and the
    absence of cryptographic security proofs.

    Hash / keccak / signature / PoW-digest assumptions are centralized
    in [Kernel.CryptoAssumptions]. This file does not re-state them.

    NOT CLAIMED:
      C++ implementation equivalence
      Full chain validity
      Hash collision resistance
      Signature security
      Runtime rejection equivalence
*)
From Coq Require Import List.
From RigorKernel Require Import Core.Bytes.
From RigorKernel Require Import Audit.KernelNonClaims.
From RigorKernel Require Import Kernel.CryptoAssumptions.

(** Digest functions used by PoW theorems are Coq functions
    [list byte -> list byte]. No concrete hash is unfolded. *)
Definition digest_is_a_function (H : list byte -> list byte) : Prop :=
  forall x, H x = H x.

Theorem abstract_digest_is_a_function :
  forall H : list byte -> list byte,
    digest_is_a_function H.
Proof. intros H x. reflexivity. Qed.

(** Draw eligibility is an abstract predicate supplied as a hypothesis
    at the RES issuance boundary. Ownership binding is not derived. *)
Definition draw_eligibility_is_hypothesis : Prop :=
  draw_owner_binding = false.

Theorem draw_eligibility_remains_a_hypothesis :
  draw_eligibility_is_hypothesis.
Proof. reflexivity. Qed.

Theorem kernel_does_not_assume_sha3_security :
  sha3_security = false.
Proof. reflexivity. Qed.

Theorem kernel_does_not_assume_ed25519_security :
  ed25519_security = false.
Proof. reflexivity. Qed.

Theorem kernel_assumptions_closed_shape :
  digest_is_a_function (fun x : list byte => x) /\
  draw_eligibility_is_hypothesis /\
  sha3_security = false /\
  ed25519_security = false.
Proof. repeat split; reflexivity. Qed.

Theorem crypto_inventory_is_centralized :
  keccak_implementation_verified = false
  /\ pow_digest_is_abstract_H = true
  /\ production_crypto_refinement = false.
Proof. repeat split; reflexivity. Qed.

(** RigorKernel.Pow.AbstractHash — Phase A abstract digest interface.

    Scope: consensus kernel only.

    [H] is an uninterpreted function [list byte -> list byte].
    This file never mentions sha3, keccak, or any concrete hash.

    Constraint C-1 (P0-106): no theorem may attempt symbolic Keccak
    reduction; no theorem contains [sha3_256].

    Crypto inventory: [Kernel.CryptoAssumptions]. This file does not
    prove keccak / SHA3 / SHA256 security.

    NOT CLAIMED:
      C++ implementation equivalence
      Full chain validity
      Collision / preimage resistance of any concrete hash
*)
From Coq Require Import List.
From Coq Require Import ZArith.ZArith.
From RigorKernel Require Import Core.Bytes.

Local Open Scope Z_scope.

Section AbstractHash.

Variable H : list byte -> list byte.

Definition digest (x : list byte) : list byte := H x.

(** Statement: [H] is a function — it agrees with itself on every input.
    Assumptions: none ([H] is a Coq function).
    Scope: consensus kernel only. *)
Theorem hash_deterministic :
  forall x : list byte,
    H x = H x.
Proof. intros x. reflexivity. Qed.

(** Statement: equal preimages yield equal digests.
    Assumptions: [x = y].
    Scope: consensus kernel only. *)
Theorem hash_extensional :
  forall x y : list byte,
    x = y -> H x = H y.
Proof. intros x y Heq. rewrite Heq. reflexivity. Qed.

(** Target-comparison abstraction: digest viewed as an integer,
    compared against a non-negative target. Endianness is not
    claimed to match C++. *)
Definition hash_le_target (d : list byte) (target : Z) : Prop :=
  0 < target /\ digest_as_Z d <= target.

Definition difficulty_ok (preimage : list byte) (target : Z) : Prop :=
  hash_le_target (H preimage) target.

(** Statement: the difficulty predicate is deterministic in the
    digest — equal preimages decide equally.
    Assumptions: [p1 = p2].
    Scope: consensus kernel only. *)
Theorem difficulty_deterministic :
  forall p1 p2 : list byte,
  forall target : Z,
    p1 = p2 ->
    difficulty_ok p1 target <-> difficulty_ok p2 target.
Proof.
  intros p1 p2 target Heq. subst p2. split; intros Hok; exact Hok.
Qed.

End AbstractHash.

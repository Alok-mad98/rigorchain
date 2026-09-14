(** Target ordering. Abstract digest only. No SHA3 / keccak.

    NOT CLAIMED:
      hash implementation verified
      C++ endianness
      cryptographic security
*)
From Coq Require Import ZArith.ZArith.
From Coq Require Import Lia.
From RigorKernel Require Import Core.Bytes.
From RigorKernel Require Import Pow.AbstractHash.

Local Open Scope Z_scope.

(** Named so M15 can reverse the comparison without touching [hash_le_target]. *)
Definition meets_target (d : list byte) (target : Z) : Prop :=
  0 < target /\ digest_as_Z d <= target.

Theorem hash_less_target_equivalent :
  forall d target,
    meets_target d target <-> hash_le_target d target.
Proof.
  intros d target. unfold meets_target, hash_le_target. tauto.
Qed.

Theorem target_monotonic :
  forall d t1 t2,
    0 < t1 ->
    t1 <= t2 ->
    meets_target d t1 ->
    meets_target d t2.
Proof.
  intros d t1 t2 Ht1 Hle [Hpos Hdig].
  unfold meets_target. split; lia.
Qed.

Theorem pow_acceptance_monotonic :
  forall (Hfun : list byte -> list byte) (p : list byte) (t1 t2 : Z),
    0 < t1 ->
    t1 <= t2 ->
    difficulty_ok Hfun p t1 ->
    difficulty_ok Hfun p t2.
Proof.
  intros Hfun p t1 t2 Ht1 Hle Hok.
  unfold difficulty_ok, hash_le_target in *.
  destruct Hok as [Hp Hd]. split; lia.
Qed.

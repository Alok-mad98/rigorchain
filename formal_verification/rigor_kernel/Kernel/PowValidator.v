(** RigorKernel.Kernel.PowValidator — executable PoW check.

    Scope: consensus kernel only.

    Digest is supplied already as an integer ([pow_digest_z]).
    This is the [hash_le_target] comparison, not a hash evaluation.
    Compact lower bound is the existing [compact_decode_lower_bound].
    No SHA3 / Keccak. No new consensus rule.

    NOT CLAIMED:
      C++ CheckProofOfWork equivalence
      Hash collision resistance
      Concrete "RGRPOW01" digest values
*)
From Coq Require Import Bool.Bool.
From Coq Require Import ZArith.ZArith.
From RigorKernel Require Import Pow.CompactBits.

Local Open Scope Z_scope.

Record PowHeader := {
  pow_digest_z : Z;
  pow_target : Z;
  pow_compact_t : Z
}.

Definition check_pow (h : PowHeader) : bool :=
  andb (0 <? pow_target h)
  (andb (pow_digest_z h <=? pow_target h)
  (andb (0 <=? pow_compact_t h)
        (compact_decode (compact_encode (pow_compact_t h))
           <=? pow_compact_t h))).

(** Statement: a passing check is the target comparison used by
    [hash_le_target] / [pow_valid], plus the compact lower bound.
    Assumptions: [0 <= pow_compact_t] is part of the boolean.
    Consensus kernel only: yes. *)
Theorem check_pow_sound :
  forall h,
    check_pow h = true ->
    0 < pow_target h
    /\ pow_digest_z h <= pow_target h
    /\ 0 <= pow_compact_t h
    /\ compact_decode (compact_encode (pow_compact_t h))
         <= pow_compact_t h.
Proof.
  intros h H.
  unfold check_pow in H.
  rewrite !andb_true_iff, Z.ltb_lt, !Z.leb_le in H.
  destruct H as [Ht [Hd [Hnn Hbd]]].
  repeat split; assumption.
Qed.

Theorem check_pow_compact :
  forall h,
    check_pow h = true ->
    compact_decode (compact_encode (pow_compact_t h))
      <= pow_compact_t h.
Proof.
  intros h H. apply check_pow_sound in H. tauto.
Qed.

(** Function-ness of the executable check — the same statement
    shape as [pow_valid_deterministic]. *)
Theorem check_pow_deterministic :
  forall h, check_pow h = check_pow h.
Proof. intros h. reflexivity. Qed.

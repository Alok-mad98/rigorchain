(** PoW acceptance pipeline. Abstract digest [H] only.

    bits decode → target compare → abstract hash value.

    NOT CLAIMED: SHA3, keccak, hash implementation, C++ CheckProofOfWork.
*)
From Coq Require Import List.
From Coq Require Import ZArith.ZArith.
From Coq Require Import Lia.
From RigorKernel Require Import Core.Bytes.
From RigorKernel Require Import Pow.CompactBits.

Local Open Scope Z_scope.

Definition decode_bits (nbits : Z) : Z := compact_decode nbits.

(** Named compare. M22 inverts [<=] to [>]. *)
Definition pipeline_compare (digest : list byte) (target : Z) : Prop :=
  0 < target /\ digest_as_Z digest <= target.

Definition pow_pipeline
    (Hfun : list byte -> list byte)
    (preimage : list byte)
    (nbits : Z) : Prop :=
  pipeline_compare (Hfun preimage) (decode_bits nbits).

Theorem pow_pipeline_sound :
  forall (Hfun : list byte -> list byte) (p : list byte) (nbits : Z),
    pow_pipeline Hfun p nbits ->
    0 < compact_decode nbits /\
    digest_as_Z (Hfun p) <= compact_decode nbits.
Proof.
  intros Hfun p nbits [Hp Hd].
  unfold decode_bits in *. split; lia.
Qed.

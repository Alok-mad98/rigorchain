(** RigorSurface.Kernel.Pow — [check_pow]. No theorems. *)
From Coq Require Import Bool.Bool.
From Coq Require Import ZArith.ZArith.
From RigorSurface Require Import Pow.Compact.

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

(** RigorSurface.Pow.Compact — encode/decode slice. No theorems. *)
From Coq Require Import ZArith.ZArith.

Local Open Scope Z_scope.

Definition compact_size (nbits : Z) : Z := nbits / 16777216.
Definition compact_mantissa (nbits : Z) : Z := nbits mod 8388608.

Definition compact_decode (nbits : Z) : Z :=
  let size := compact_size nbits in
  let word := compact_mantissa nbits in
  if size <=? 3 then Z.shiftr word (8 * (3 - size))
  else Z.shiftl word (8 * (size - 3)).

Definition raw_size (t : Z) : Z := (Z.log2 t + 8) / 8.

Definition raw_mantissa (t : Z) : Z :=
  let s := raw_size t in
  if s <=? 3 then Z.shiftl t (8 * (3 - s))
  else Z.shiftr t (8 * (s - 3)).

Definition adjust_signbit (m s : Z) : Z * Z :=
  if Z.testbit m 23 then (Z.shiftr m 8, s + 1) else (m, s).

Definition compact_encode (t : Z) : Z :=
  if t <=? 0 then 0
  else
    let '(m, s) := adjust_signbit (raw_mantissa t) (raw_size t) in
    s * 16777216 + m.

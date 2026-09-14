(** RigorSurface.Pow.ASERTQ — boxed-Z [exp_q_raw] slice. No theorems.

    Do not lower this to CompCert [long long].
*)
From Coq Require Import ZArith.ZArith.

Local Open Scope Z_scope.

Definition ASERT_RADIX : Z := 65536.

Definition exp_q_raw (te tau : Z) : Z :=
  Z.quot (te * ASERT_RADIX) tau.

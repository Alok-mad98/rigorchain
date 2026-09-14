(** RigorSurface.Kernel.ASERT — [check_asert]. Boxed [Z] only. *)
From Coq Require Import Bool.Bool.
From Coq Require Import ZArith.ZArith.
From RigorSurface Require Import Pow.ASERTQ.

Local Open Scope Z_scope.

Record ASERTInput := {
  asert_te : Z;
  asert_tau : Z;
  asert_expected_q : Z
}.

Definition check_asert (i : ASERTInput) : bool :=
  andb (asert_te i <? 0)
  (andb (0 <? asert_tau i)
  (andb (exp_q_raw (asert_te i) (asert_tau i) =? asert_expected_q i)
        (exp_q_raw (asert_te i) (asert_tau i)
           =? Z.quot (asert_te i * ASERT_RADIX) (asert_tau i)))).

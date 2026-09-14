(** Load published LambdaANF lemma module without Compiler.pipeline. *)
From CertiCoq Require Import LambdaANF.toplevel_theorems.

Definition published_lambdaanf_lemmas_load : bool := true.

Theorem published_lambdaanf_lemmas_available :
  published_lambdaanf_lemmas_load = true.
Proof. reflexivity. Qed.

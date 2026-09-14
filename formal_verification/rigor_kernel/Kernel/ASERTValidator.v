(** RigorKernel.Kernel.ASERTValidator — executable ASERT check.

    Scope: consensus kernel only.

    Checks the operator split ([Z.quot] for [exp_q_raw]) against an
    expected quotient. Does not evaluate a concrete SHA3 digest.
    No new consensus rule.

    NOT CLAIMED:
      C++ __int128 / arith_uint256 equivalence
      Miner fairness
*)
From Coq Require Import Bool.Bool.
From Coq Require Import ZArith.ZArith.
From RigorKernel Require Import Pow.ASERTModel.
From RigorKernel.proofs.Pow_Correctness Require Import ASERTCorrectness.

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

(** Statement: a passing check implies [negative_time_error_preserved]
    and that the recorded quotient is the model's [Z.quot] value.
    Assumptions: none beyond the boolean preconditions.
    Consensus kernel only: yes. *)
Theorem check_asert_sound :
  forall i,
    check_asert i = true ->
    asert_te i < 0
    /\ 0 < asert_tau i
    /\ exp_q_raw (asert_te i) (asert_tau i) = asert_expected_q i
    /\ exp_q_raw (asert_te i) (asert_tau i)
         = Z.quot (asert_te i * ASERT_RADIX) (asert_tau i)
    /\ num_shifts (exp_q (asert_te i) (asert_tau i))
         = Z.div (exp_q (asert_te i) (asert_tau i)) ASERT_RADIX
    /\ 0 <= exp_rem (exp_q (asert_te i) (asert_tau i)) < ASERT_RADIX.
Proof.
  intros i H.
  unfold check_asert in H.
  rewrite !andb_true_iff, !Z.ltb_lt, !Z.eqb_eq in H.
  destruct H as [Hte [Htau [Heq Hquot]]].
  pose proof (negative_time_error_preserved (asert_te i) (asert_tau i) Hte Htau)
    as Hp.
  destruct Hp as [Hq [Hs Hr]].
  tauto.
Qed.

(** Re-export of [pow_asert_correctness] — ASERT is a function. *)
Theorem check_asert_deterministic :
  forall anchor parent, deterministic_asert anchor parent.
Proof. apply pow_asert_correctness. Qed.

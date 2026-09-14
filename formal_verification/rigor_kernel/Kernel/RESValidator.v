(** RigorKernel.Kernel.RESValidator — executable RES check.

    Scope: consensus kernel only.

    [check_res] is the boolean form of [res_created]: a body mint
    whose amounts satisfy R · (2^25 + h)^2 ≤ B << 40. No draw bit.
    No new consensus rule.

    NOT CLAIMED:
      C++ implementation equivalence
      Full chain validity
      Runtime S_RES rejection
*)
From Coq Require Import Bool.Bool.
From Coq Require Import ZArith.ZArith.
From Coq Require Import Lia.
From RigorKernel Require Import Supply.SupplySchedule.
From RigorKernel.proofs.RES_Issuance Require Import RESIssuance.

Local Open Scope Z_scope.

Definition RESState := res_tx.

Definition check_res (s : RESState) : bool :=
  andb (negb (coinbase s))
  (andb (rt_mint_ok s)
  (andb (0 <? mint_amount s)
  (andb (mint_amount s <=? RES_MAX_SUPPLY)
  (andb (res_mint_authorized (rt_height s))
  (andb (0 <=? burn_oru s)
        (mint_amount s * res_destroy_pole_sq (rt_height s)
           <=? Z.shiftl (burn_oru s) RES_DESTROY_SHIFT)))))).

Lemma check_res_reflect :
  forall s,
    check_res s = true ->
    res_created s.
Proof.
  intros s H.
  unfold check_res, res_created, res_mint_fits_rate, valid_res_mint,
         coinbase, mint_amount, burn_oru in *.
  rewrite !andb_true_iff, negb_true_iff, Z.ltb_lt, !Z.leb_le in H.
  destruct H as [Hcb [Hok [Hpos [Hcap [Hauth [Hburn Hfit]]]]]].
  repeat split; try assumption.
  apply rate_pos_of_authorized. exact Hauth.
Qed.

(** Statement: a passing check implies [res_issuance_soundness].
    Assumptions: none (C-4 is gone).
    Consensus kernel only: yes. *)
Theorem check_res_sound :
  forall s,
    check_res s = true ->
    coinbase s = false
    /\ valid_res_mint s
    /\ mint_amount s * res_destroy_pole_sq (rt_height s)
       <= Z.shiftl (burn_oru s) RES_DESTROY_SHIFT.
Proof.
  intros s H.
  apply check_res_reflect in H.
  eapply res_issuance_soundness; [exact H | reflexivity].
Qed.

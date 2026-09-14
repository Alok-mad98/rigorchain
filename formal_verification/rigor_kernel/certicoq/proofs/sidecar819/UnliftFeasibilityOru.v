(** TASK 3.1C: [f_case_118] measurements used by the feasibility audit. *)
From Coq Require Import List.
From CheckOruGen Require Import check_oru_clight.
From Phase10 Require Import UnliftDef UnliftRelation UnliftFeasibility UnliftOruFacts.
From Coq Require Import Arith.
From compcert Require Import Coqlib Maps Ctypes Clight Clightdefs.
Import Clightdefs.ClightNotations.
Local Open Scope clight_scope.

Fixpoint count_scall (s: statement) : nat :=
  match s with
  | Scall _ _ _ => 1
  | Sbuiltin _ _ _ _ => 0
  | Ssequence s1 s2 => count_scall s1 + count_scall s2
  | Sifthenelse _ s1 s2 => count_scall s1 + count_scall s2
  | Sloop s1 s2 => count_scall s1 + count_scall s2
  | Sswitch _ ls => count_scall_ls ls
  | Slabel _ s1 => count_scall s1
  | Sset _ _ | Sassign _ _ | Sskip | Sbreak | Scontinue
  | Sreturn _ | Sgoto _ => 0
  end
with count_scall_ls (ls: labeled_statements) : nat :=
  match ls with
  | LSnil => 0
  | LScons _ s ls1 => count_scall s + count_scall_ls ls1
  end.

Definition f_case_118_scall_count : nat :=
  count_scall (fn_body f_f_case_118).

Lemma f_case_118_has_scalls :
  Nat.ltb 0 f_case_118_scall_count = true.
Proof. vm_compute. reflexivity. Qed.

Lemma f_case_118_core_same_params :
  fn_params (unlift_function_core f_f_case_118) = fn_params f_f_case_118.
Proof. reflexivity. Qed.

Lemma f_case_118_core_body_is_unlift :
  fn_body (unlift_function_core f_f_case_118) =
  unlift_stmt (fn_params f_f_case_118) (fn_body f_f_case_118).
Proof. reflexivity. Qed.

Lemma f_case_118_entry2_env_301_not_local :
  forall ge vargs m e le m',
    function_entry2 ge f_f_case_118 vargs m e le m' ->
    PTree.get __env_301 e = None.
Proof.
  intros.
  eapply function_entry2_param_not_local; eauto.
  apply f_case_118_env_301_not_var.
Qed.

Lemma f_case_118_entry1_env_301_has_block :
  forall ge vargs m e le m',
    function_entry1 ge f_f_case_118 vargs m e le m' ->
    exists b,
      PTree.get __env_301 e = Some (b, tlong) /\
      In (b, 0, sizeof ge tlong) (blocks_of_env ge e).
Proof.
  intros.
  eapply entry1_param_is_in_free_list; eauto.
  apply f_case_118_env_301_in_params.
Qed.

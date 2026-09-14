(** TASK 3.1A measurement theorems. Sidecar 8.19. No Axiom. *)
From Phase10 Require Import Phase31A_SimplLocals Phase31B_Unlift Phase31C_Selection.

Theorem sl_f_case_120_computes_ok :
  sl_f_case_120_ok = true.
Proof. vm_compute. reflexivity. Qed.

Theorem sl_f_case_118_computes_error :
  sl_f_case_118_ok = false.
Proof. vm_compute. reflexivity. Qed.

Theorem first_bad_temporary_is_f_case_118_env_301 :
  first_fail_is_f_case_118 = true /\
  first_fail_temp_is_env_301 = true /\
  env301_is_param_of_f_case_118 = true /\
  env301_in_cenv = true /\
  env301_addr_taken = false.
Proof. vm_compute. repeat split; reflexivity. Qed.

Theorem unlift_makes_f_case_118_simpllocals_ok :
  sl_unlift_f_case_118_ok = true.
Proof. vm_compute. reflexivity. Qed.

Theorem unlift_makes_all_internal_simpllocals_ok :
  sl_unlift_all_ok = true.
Proof. vm_compute. reflexivity. Qed.

Theorem check_oru_cminor_has_switch :
  cminor_has_switch = true.
Proof. vm_compute. reflexivity. Qed.

Theorem check_oru_selection_helpers_ok :
  helpers_ok = true.
Proof. vm_compute. reflexivity. Qed.

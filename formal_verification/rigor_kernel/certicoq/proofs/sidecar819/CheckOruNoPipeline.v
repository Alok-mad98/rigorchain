(** Phase 10: CertiCoq stages for [check_oru] without [Compiler.pipeline].

    Does not [Require] [CertiCoq.Compiler.pipeline], so
    [Axiom print_Clight] is not in this file's environment.

    Stages stop at in-memory [Clight.program] ([compile_Clight]).
    This file does not call any printer and does not write [.c].

    NOT CLAIMED: Codegen correctness, Level 5, compiler_correctness.
*)
From Coq Require Import List PArith.BinPos.
From ExtLib Require Import Monads.
From MetaCoq.Common Require Import BasicAst Kernames.
From MetaCoq.ErasurePlugin Require Import Erasure.
From MetaCoq.Utils Require Import bytestring.
From CertiCoq.Common Require Import Pipeline_utils compM.
From CertiCoq Require Import
  LambdaBoxMut.toplevel
  LambdaBoxLocal.toplevel
  LambdaANF.toplevel
  Codegen.toplevel.
From Phase10 Require Import CheckOruQuote.

Import Monads.
Import MonadNotation.
Import ListNotations.
Open Scope monad_scope.

Definition default_opts : Options :=
  Build_Options
    Erasure.default_erasure_config
    false (* direct = CPS *)
    5
    0
    false
    0
    false
    false
    false
    0
    ""%bs
    "body"%bs
    [].

Definition next_id := 100%positive.

Definition empty_prims : list (kername * string * bool * nat * positive) := [].

Definition erase_stage :=
  run_pipeline _ _ default_opts q_check_oru
    (compile_LambdaBoxMut Erasure.default_erasure_config).

Definition local_stage :=
  run_pipeline _ _ default_opts q_check_oru
    (fun p =>
       p <- compile_LambdaBoxMut Erasure.default_erasure_config p ;;
       compile_LambdaBoxLocal empty_prims p).

Definition anf_cps_stage :=
  run_pipeline _ _ default_opts q_check_oru
    (fun p =>
       p <- compile_LambdaBoxMut Erasure.default_erasure_config p ;;
       p <- compile_LambdaBoxLocal empty_prims p ;;
       compile_LambdaANF_CPS next_id empty_prims p).

Definition anf_full_stage :=
  run_pipeline _ _ default_opts q_check_oru
    (fun p =>
       p <- compile_LambdaBoxMut Erasure.default_erasure_config p ;;
       p <- compile_LambdaBoxLocal empty_prims p ;;
       p <- compile_LambdaANF_CPS next_id empty_prims p ;;
       compile_LambdaANF next_id p).

(* Official pipeline runs [compile_LambdaANF] (opts) before
   Codegen. That pass timed out at 600s on this term. Codegen
   from the CPS term is the in-memory Clight we can actually
   form without [print_Clight]. *)
Definition clight_from_cps_stage :=
  run_pipeline _ _ default_opts q_check_oru
    (fun p =>
       p <- compile_LambdaBoxMut Erasure.default_erasure_config p ;;
       p <- compile_LambdaBoxLocal empty_prims p ;;
       p <- compile_LambdaANF_CPS next_id empty_prims p ;;
       compile_Clight empty_prims p).

Definition clight_mem_stage :=
  run_pipeline _ _ default_opts q_check_oru
    (fun p =>
       p <- compile_LambdaBoxMut Erasure.default_erasure_config p ;;
       p <- compile_LambdaBoxLocal empty_prims p ;;
       p <- compile_LambdaANF_CPS next_id empty_prims p ;;
       p <- compile_LambdaANF next_id p ;;
       compile_Clight empty_prims p).

Definition stage_ok {A} (r : error A * string) : bool :=
  match fst r with
  | Ret _ => true
  | Err _ => false
  end.

(* CPS conversion computed [Ret] in seconds. Full ANF opts did not. *)
Definition exp_of_check_oru_opt : option LambdaANF.cps.exp :=
  match fst anf_cps_stage with
  | Ret t => Some (snd t)
  | Err _ => None
  end.

Definition mem_clight_prog : option Clight.program :=
  match fst clight_from_cps_stage with
  | Ret cp =>
      let '(_, p, _) := cp in Some p
  | Err _ => None
  end.

Definition mem_clight_from_full_anf : option Clight.program :=
  match fst clight_mem_stage with
  | Ret cp =>
      let '(_, p, _) := cp in Some p
  | Err _ => None
  end.

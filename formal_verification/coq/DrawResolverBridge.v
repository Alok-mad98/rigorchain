(** DrawResolverBridge.v — P052 draw resolver mapping.

    Kernel object:

      Coq  select_winner / pick_better_tx
        ↕
      C++  ResolveConvergenceDraw(F, T)

    State assembly [GetConvergenceDrawSetForBlock] is not Kernel.

    Does not prove C++ = Coq. No [Axiom] / [Admitted] / [Parameter]. *)
From Coq Require Import List.
From Coq Require Import ZArith.ZArith.
Import ListNotations.
Require Import Rigor.legacy_account_model.Types.
Require Import Rigor.legacy_account_model.Selection.
Require Import Rigor.Sigma.
Require Import Rigor.Kernel.KernelSelection.

Local Open Scope Z_scope.

Definition refinement_proved : bool := false.
Definition equivalence_proved : bool := false.

Theorem draw_bridge_refinement_not_proved : refinement_proved = false.
Proof. reflexivity. Qed.

Theorem draw_bridge_equivalence_not_proved : equivalence_proved = false.
Proof. reflexivity. Qed.

(** C++ F = ConvergenceDrawSet.candidates after assembly. *)
Definition CandidateSet := list Tx.

(** Coq σ; C++ SelectionDigest / CalculateSigmaHash. *)
Definition SigmaBridge := Sigma.

(** C++ T = RES_PROBABILITY_TARGET. *)
Definition Target := Z.

Definition cpp_pick_better_bridge := pick_better_tx.
Definition cpp_select_winner_bridge := select_winner.

(** C++ T maps to this constant. Not a second target. *)
Definition cpp_target_bridge : Z := RES_PROBABILITY_TARGET.

(** Eligible filter then winner. Assembly of F is outside this object. *)
Definition cpp_resolve_draw_bridge
  (F : CandidateSet) (C B : list byte) (h : Z) : option Tx :=
  select_winner (eligible_candidates F C B h) C B h.

Theorem cpp_select_winner_bridge_is_canonical :
  cpp_select_winner_bridge = select_winner.
Proof. reflexivity. Qed.

Theorem cpp_pick_better_bridge_is_canonical :
  cpp_pick_better_bridge = pick_better_tx.
Proof. reflexivity. Qed.

Theorem cpp_target_bridge_is_canonical :
  cpp_target_bridge = RES_PROBABILITY_TARGET.
Proof. reflexivity. Qed.

Theorem empty_F_resolves_to_none :
  forall C B h, cpp_resolve_draw_bridge [] C B h = None.
Proof. intros. reflexivity. Qed.

Theorem empty_select_winner_none :
  forall C B h, select_winner [] C B h = None.
Proof. intros. reflexivity. Qed.

Theorem winner_comes_from_F :
  forall l C B h w,
    select_winner l C B h = Some w -> List.In w l.
Proof. exact KernelSelection.select_winner_in. Qed.

(** Documentation flags: assembly is orchestration, not this bridge. *)
Definition GetConvergenceDrawSetForBlock_is_kernel : bool := false.
Definition ResolveConvergenceDraw_is_kernel : bool := true.

Theorem assembly_is_not_kernel :
  GetConvergenceDrawSetForBlock_is_kernel = false.
Proof. reflexivity. Qed.

Theorem resolver_is_kernel :
  ResolveConvergenceDraw_is_kernel = true.
Proof. reflexivity. Qed.

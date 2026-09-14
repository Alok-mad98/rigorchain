(** Unified kernel-step soundness. Category A.

    Unified consensus kernel transition invariants are formally verified
    inside the Coq model. Not a node; not C++ refinement.

    NOT CLAIMED: Bitcoin protocol, full implementation, CompCert.
*)
From Coq Require Import ZArith.ZArith.
From Coq Require Import Lia.
From RigorKernel Require Import Kernel.UnifiedState.
From RigorKernel Require Import Kernel.UnifiedTransition.
From RigorKernel Require Import Kernel.InvariantBundle.

Local Open Scope Z_scope.

Lemma step_of_preserves :
  forall s i,
    kernel_invariants s ->
    input_ok s i ->
    kernel_invariants (kernel_step_of s i).
Proof.
  intros s i Hinv Hok.
  unfold kernel_invariants, kernel_step_of, input_ok,
         utxo_sound, oru_conservation, res_bound,
         script_classification, pow_validity, asert_boundary,
         utxo_transition, oru_transition, res_transition,
         script_transition, pow_transition, asert_transition,
         oru_delta in *.
  simpl in *.
  destruct Hinv as [Hu [Ho [Hr [Hs [Hp Ha]]]]].
  destruct Hok as [Huto [Hsc [Hpo [Has [Hin [Hout [Hfe [Hbu [Hsu [Hmi [Hbal [Hcap0 Hcap]]]]]]]]]]]].
  rewrite Hu, Hs, Hp, Ha, Huto, Hsc, Hpo, Has. simpl.
  repeat split; try reflexivity.
  - lia.
  - apply Z.min_glb; lia.
  - apply Z.le_min_l.
Qed.

Theorem kernel_step_preserves_invariants :
  forall s s' : KernelState,
    kernel_invariants s ->
    kernel_step s s' ->
    kernel_invariants s'.
Proof.
  intros s s' Hinv [i Happ].
  apply kernel_apply_is_graph in Happ as [Hok ->].
  apply step_of_preserves; assumption.
Qed.

Theorem kernel_transition_deterministic :
  forall s i s1 s2,
    kernel_apply s i s1 ->
    kernel_apply s i s2 ->
    s1 = s2.
Proof.
  intros s i s1 s2 H1 H2.
  apply kernel_apply_is_graph in H1 as [_ ->].
  apply kernel_apply_is_graph in H2 as [_ ->].
  reflexivity.
Qed.

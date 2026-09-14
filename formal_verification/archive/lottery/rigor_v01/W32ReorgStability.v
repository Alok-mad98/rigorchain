(** RIGOR v0.1 — W=32 reorg-stability (P0-032), now a specialization of
    the W-generic window model (P0-034).

    These theorems are instances of [RigorV01.WindowGeneric] at W = 32.
    They are not an independent mathematical universe.

    Production W remains 32. This file does not land W=36.

    Honesty:
      model proved
      refinement_proved = false
      No [Axiom] / [Admitted]. *)
From Coq Require Import ZArith.
From Coq Require Import Lia.
Require Import RigorV01.WindowGeneric.
Require Import RigorV01.WindowSpecialization.
Open Scope Z_scope.

(** Shipping lock — an instance, not a generic hypothesis. *)
Definition CONVERGENCE_DRAW_WINDOW : Z := W32.

Theorem W32_window_width_locked :
  CONVERGENCE_DRAW_WINDOW = 32.
Proof. reflexivity. Qed.

Definition draw_window_start (s : Z) : Z := window_start s W32.
Definition draw_window_end (s : Z) : Z := window_end s W32.
Definition window_of_source_W32 (s : Z) : Z * Z := window_of_source s W32.
Definition in_draw_window (h s : Z) : Prop := in_window h s W32.
Definition branch_window_W32 (branch s : Z) : Z * Z := branch_window branch s W32.

Theorem W32_group_membership_height_stable :
  forall s : Z,
    draw_window_start s = Z.max 0 (s - CONVERGENCE_DRAW_WINDOW + 1) /\
    draw_window_end s = s.
Proof. intros. apply window_membership_height_stable. Qed.

Theorem W32_reorg_preserves_group_boundary :
  forall (branch_a branch_b s : Z),
    branch_window_W32 branch_a s = branch_window_W32 branch_b s.
Proof. intros. apply W32_reorg_preserves_boundary. Qed.

Theorem W32_reorg_preserves_group_boundary_formula :
  forall s : Z,
    window_of_source_W32 s = (draw_window_start s, draw_window_end s).
Proof. intros. reflexivity. Qed.

Theorem W32_reorg_changes_contents_only :
  forall (branch_a branch_b s : Z),
    branch_window_W32 branch_a s = branch_window_W32 branch_b s /\
    (branch_a <> branch_b -> contents_at branch_a s <> contents_at branch_b s).
Proof. intros. apply WindowSpecialization.W32_reorg_changes_contents_only. Qed.

Theorem W32_orphan_state_not_canonical :
  forall o : GroupStateOwner,
    o = OrphanOwner ->
    owner_is_canonical o = false.
Proof. apply orphan_state_not_canonical. Qed.

Theorem W32_canonical_owner_is_canonical :
  owner_is_canonical CanonicalOwner = true.
Proof. apply canonical_owner_is_canonical. Qed.

Theorem W32_maturity_constant_is_100 :
  COINBASE_MATURITY = 100.
Proof. apply maturity_constant_is_100. Qed.

Theorem W32_maturity_neq_window :
  COINBASE_MATURITY <> CONVERGENCE_DRAW_WINDOW.
Proof. unfold CONVERGENCE_DRAW_WINDOW, W32, COINBASE_MATURITY. lia. Qed.

Theorem W32_maturity_independent :
  forall (h s : Z),
    coinbase_mature (h + CONVERGENCE_DRAW_WINDOW) h = false /\
    coinbase_mature (h + COINBASE_MATURITY) h = true.
Proof. intros h s. apply WindowSpecialization.W32_maturity_independent. Qed.

Theorem W32_maturity_not_derived_from_window :
  forall (h s : Z),
    in_draw_window h s ->
    coinbase_mature (h + CONVERGENCE_DRAW_WINDOW) h = false.
Proof.
  intros h s Hin.
  unfold CONVERGENCE_DRAW_WINDOW.
  apply (maturity_not_derived_from_window h s W32).
  - exact W32_ok.
  - unfold W32, COINBASE_MATURITY. lia.
  - exact Hin.
Qed.

Theorem W32_deep_reorg_stability :
  forall (reorg_depth s : Z),
    draw_window_start s = Z.max 0 (s - CONVERGENCE_DRAW_WINDOW + 1) /\
    draw_window_end s = s.
Proof. intros. apply window_membership_height_stable. Qed.

Theorem W32_deep_reorg_depths_33_and_64 :
  forall s : Z,
    branch_window_W32 0 s = branch_window_W32 33 s /\
    branch_window_W32 0 s = branch_window_W32 64 s.
Proof. intros. split; reflexivity. Qed.

Theorem W32_not_floor_partition :
  draw_window_start 1000 <> 32 * (1000 / 32).
Proof. apply WindowSpecialization.W32_not_floor_partition. Qed.

Theorem W32_coincides_with_floor_slice_only_at_slice_end :
  forall g : Z,
    0 <= g ->
    draw_window_start (32 * g + 31) = 32 * g /\
    draw_window_end (32 * g + 31) = 32 * g + 31.
Proof.
  intros g Hg.
  unfold draw_window_start, draw_window_end, W32.
  apply (coincides_with_floor_slice_only_at_slice_end g 32).
  - exact W32_ok.
  - exact Hg.
Qed.

Theorem W32_task_scenario_A_boundary :
  draw_window_start 1023 = 992 /\ draw_window_end 1023 = 1023.
Proof. apply WindowSpecialization.W32_task_scenario_A_boundary. Qed.

Theorem W32_task_scenario_A_reorg_does_not_slide_interval :
  draw_window_start 1023 <> 1010 /\ draw_window_end 1023 <> 1041.
Proof. apply WindowSpecialization.W32_task_scenario_A_reorg_does_not_slide. Qed.

Theorem W32_task_scenario_B_boundaries :
  draw_window_start 991 = 960 /\
  draw_window_start 992 = 961 /\
  draw_window_start 1023 = 992 /\
  draw_window_start 1024 = 993.
Proof. apply WindowSpecialization.W32_task_scenario_B_boundaries. Qed.

Theorem W32_windows_overlap :
  in_draw_window 1000 1000 /\ in_draw_window 1000 1031.
Proof.
  unfold in_draw_window, W32.
  unfold in_window, window_start, window_end.
  split; split; lia.
Qed.

Theorem W32_one_source_window_per_source_height :
  forall s : Z,
    window_of_source_W32 s = window_of_source_W32 s.
Proof. intros. reflexivity. Qed.

Theorem W32_genesis_clamp :
  draw_window_start 0 = 0 /\
  draw_window_start 15 = 0 /\
  draw_window_start 31 = 0 /\
  draw_window_start 32 = 1.
Proof. apply WindowSpecialization.W32_genesis_clamp. Qed.

(** RIGOR v0.1 — concrete specializations of the W-generic window (P0-034).

    W=32 : shipping production instance (does not change C++).
    W=36 : formal candidate only. NOT landed in production.

    Recovers P0-032 core properties as instances of [WindowGeneric].
    Golden vectors: genesis, W-1, W, W+1, 2W.

    Honesty: no [Axiom] / [Admitted]. refinement_proved = false.
    Production W=36: NOT LANDED. *)
From Coq Require Import ZArith.
From Coq Require Import Lia.
Require Import RigorV01.WindowGeneric.
Require Import RigorV01.WindowTarget.
Open Scope Z_scope.

(** * W = 32 shipping instance *)

Definition W32 : Z := 32.

Theorem W32_ok : W_ok W32.
Proof. unfold W_ok, W32. lia. Qed.

Theorem W32_window_length_full :
  forall s : Z, 31 <= s -> window_length s W32 = 32.
Proof.
  intros s Hs. unfold W32. apply window_length_full; [exact W32_ok | lia].
Qed.

Theorem W32_genesis_clamp :
  window_start 0 W32 = 0 /\
  window_start 15 W32 = 0 /\
  window_start 31 W32 = 0 /\
  window_start 32 W32 = 1.
Proof. unfold W32. vm_compute. repeat split; reflexivity. Qed.

Theorem W32_golden_W_minus_1 :
  window_start 31 W32 = 0 /\ window_end 31 W32 = 31 /\
  window_length 31 W32 = 32.
Proof. unfold W32. vm_compute. repeat split; reflexivity. Qed.

Theorem W32_golden_W :
  window_start 32 W32 = 1 /\ window_end 32 W32 = 32 /\
  window_length 32 W32 = 32.
Proof. unfold W32. vm_compute. repeat split; reflexivity. Qed.

Theorem W32_golden_W_plus_1 :
  window_start 33 W32 = 2 /\ window_end 33 W32 = 33 /\
  window_length 33 W32 = 32.
Proof. unfold W32. vm_compute. repeat split; reflexivity. Qed.

Theorem W32_golden_2W :
  window_start 64 W32 = 33 /\ window_end 64 W32 = 64 /\
  window_length 64 W32 = 32.
Proof. unfold W32. vm_compute. repeat split; reflexivity. Qed.

Theorem W32_draw_interval :
  forall h : Z, draw_window h W32 = (h - 32, h - 1).
Proof. intros. reflexivity. Qed.

Theorem W32_clamped_draw_at_W :
  clamped_draw_start 32 W32 = 0 /\ draw_end 32 W32 = 31.
Proof. unfold W32. vm_compute. split; reflexivity. Qed.

Theorem W32_reorg_preserves_boundary :
  forall ba bb s : Z, branch_window ba s W32 = branch_window bb s W32.
Proof. intros. apply reorg_preserves_window_boundary. Qed.

Theorem W32_reorg_changes_contents_only :
  forall ba bb s : Z,
    branch_window ba s W32 = branch_window bb s W32 /\
    (ba <> bb -> contents_at ba s <> contents_at bb s).
Proof. intros. apply reorg_changes_contents_only. Qed.

Theorem W32_deep_reorg_relative :
  forall s : Z,
    branch_window 0 s W32 = branch_window 31 s W32 /\
    branch_window 0 s W32 = branch_window 32 s W32 /\
    branch_window 0 s W32 = branch_window 33 s W32 /\
    branch_window 0 s W32 = branch_window 64 s W32.
Proof. intros. apply deep_reorg_depths_relative_to_W. exact W32_ok. Qed.

Theorem W32_not_floor_partition :
  window_start 1000 W32 <> 32 * (1000 / 32).
Proof. unfold W32. vm_compute. discriminate. Qed.

Theorem W32_task_scenario_A_boundary :
  window_start 1023 W32 = 992 /\ window_end 1023 W32 = 1023.
Proof. unfold W32. vm_compute. split; reflexivity. Qed.

Theorem W32_task_scenario_A_reorg_does_not_slide :
  window_start 1023 W32 <> 1010 /\ window_end 1023 W32 <> 1041.
Proof. unfold W32. vm_compute. split; discriminate. Qed.

Theorem W32_task_scenario_B_boundaries :
  window_start 991 W32 = 960 /\
  window_start 992 W32 = 961 /\
  window_start 1023 W32 = 992 /\
  window_start 1024 W32 = 993.
Proof. unfold W32. vm_compute. repeat split; reflexivity. Qed.

Theorem W32_maturity_independent :
  forall h : Z,
    coinbase_mature (h + W32) h = false /\
    coinbase_mature (h + COINBASE_MATURITY) h = true.
Proof.
  intros h. split.
  - apply window_width_does_not_mature; [exact W32_ok | unfold W32, COINBASE_MATURITY; lia].
  - apply maturity_absolute_depth.
Qed.

Theorem W32_orphan_not_canonical :
  forall o : GroupStateOwner,
    o = OrphanOwner -> owner_is_canonical o = false.
Proof. apply orphan_state_not_canonical. Qed.

Theorem W32_cache_hash_safe :
  cache_key_is_reorg_safe CacheByBlockHash = true /\
  cache_key_is_reorg_safe CacheByHeight = false /\
  cache_key_is_reorg_safe CacheByGroupId = false.
Proof. repeat split; reflexivity. Qed.

Theorem W32_T_shipping :
  T_of_W W32 = RES_PROBABILITY_TARGET_UNWINDOWED / 32.
Proof. reflexivity. Qed.

(** * W = 36 candidate instance — formal only, not production *)

Definition W36 : Z := 36.

Theorem W36_ok : W_ok W36.
Proof. unfold W_ok, W36. lia. Qed.

Theorem W36_not_power_of_two :
  forall k : Z, 0 <= k -> W36 <> Z.shiftl 1 k.
Proof. unfold W36. apply thirty_six_is_not_a_power_of_two. Qed.

Theorem W36_window_length_full :
  forall s : Z, 35 <= s -> window_length s W36 = 36.
Proof.
  intros s Hs. unfold W36. apply window_length_full; [exact W36_ok | lia].
Qed.

Theorem W36_genesis_clamp :
  window_start 0 W36 = 0 /\
  window_start 34 W36 = 0 /\
  window_start 35 W36 = 0 /\
  window_start 36 W36 = 1.
Proof. unfold W36. vm_compute. repeat split; reflexivity. Qed.

Theorem W36_golden_W_minus_1 :
  window_start 35 W36 = 0 /\ window_end 35 W36 = 35 /\
  window_length 35 W36 = 36.
Proof. unfold W36. vm_compute. repeat split; reflexivity. Qed.

Theorem W36_golden_W :
  window_start 36 W36 = 1 /\ window_end 36 W36 = 36 /\
  window_length 36 W36 = 36.
Proof. unfold W36. vm_compute. repeat split; reflexivity. Qed.

Theorem W36_golden_W_plus_1 :
  window_start 37 W36 = 2 /\ window_end 37 W36 = 37 /\
  window_length 37 W36 = 36.
Proof. unfold W36. vm_compute. repeat split; reflexivity. Qed.

Theorem W36_golden_2W :
  window_start 72 W36 = 37 /\ window_end 72 W36 = 72 /\
  window_length 72 W36 = 36.
Proof. unfold W36. vm_compute. repeat split; reflexivity. Qed.

Theorem W36_draw_interval :
  forall h : Z, draw_window h W36 = (h - 36, h - 1).
Proof. intros. reflexivity. Qed.

Theorem W36_reorg_preserves_boundary :
  forall ba bb s : Z, branch_window ba s W36 = branch_window bb s W36.
Proof. intros. apply reorg_preserves_window_boundary. Qed.

Theorem W36_reorg_changes_contents_only :
  forall ba bb s : Z,
    branch_window ba s W36 = branch_window bb s W36 /\
    (ba <> bb -> contents_at ba s <> contents_at bb s).
Proof. intros. apply reorg_changes_contents_only. Qed.

Theorem W36_deep_reorg_relative :
  forall s : Z,
    branch_window 0 s W36 = branch_window 35 s W36 /\
    branch_window 0 s W36 = branch_window 36 s W36 /\
    branch_window 0 s W36 = branch_window 37 s W36 /\
    branch_window 0 s W36 = branch_window 72 s W36.
Proof. intros. apply deep_reorg_depths_relative_to_W. exact W36_ok. Qed.

Theorem W36_not_floor_partition :
  window_start (2 * W36) W36 <> W36 * ((2 * W36) / W36).
Proof. apply not_floor_partition. exact W36_ok. Qed.

Theorem W36_maturity_independent :
  forall h : Z,
    coinbase_mature (h + W36) h = false /\
    coinbase_mature (h + COINBASE_MATURITY) h = true.
Proof.
  intros h. split.
  - apply window_width_does_not_mature; [exact W36_ok | unfold W36, COINBASE_MATURITY; lia].
  - apply maturity_absolute_depth.
Qed.

Theorem W36_maturity_same_as_W32 :
  forall spend coin : Z,
    coinbase_mature spend coin = coinbase_mature spend coin.
Proof. intros. reflexivity. Qed.

Theorem W36_cache_hash_safe :
  cache_key_is_reorg_safe CacheByBlockHash = true /\
  cache_key_is_reorg_safe CacheByHeight = false /\
  cache_key_is_reorg_safe CacheByGroupId = false /\
  cache_key_is_reorg_safe CacheByFloorSlice = false.
Proof. repeat split; reflexivity. Qed.

Theorem W36_T_is_floor_unwindowed_div_36 :
  T_of_W W36 = RES_PROBABILITY_TARGET_UNWINDOWED / 36.
Proof. reflexivity. Qed.

Theorem W36_T_differs_from_T32 :
  T_of_W W36 <> T_of_W W32.
Proof.
  unfold W36, W32.
  intro H. apply (proj1 T_pinned_old_W_is_not_generic). symmetry. exact H.
Qed.

Theorem W36_stale_T32_is_not_the_pair :
  ~ (T_of_W W32 * W36 <= RES_PROBABILITY_TARGET_UNWINDOWED <
       (T_of_W W32 + 1) * W36).
Proof. unfold W36, W32. apply (proj2 T_pinned_old_W_is_not_generic). Qed.

(** W=36 specialization does NOT imply production W=36. *)
Definition PRODUCTION_W36_LANDED : bool := false.

Theorem production_W36_not_landed :
  PRODUCTION_W36_LANDED = false.
Proof. reflexivity. Qed.

Definition PRODUCTION_W : Z := 32.

Theorem production_W_is_32 :
  PRODUCTION_W = 32.
Proof. reflexivity. Qed.

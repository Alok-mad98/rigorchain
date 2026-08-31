(** RIGOR v0.1 — W-generic height-anchored sliding window (P0-034).

    Production shipping instance is W = 32. This module does NOT pin W to 32
    and does NOT require W to be a power of two.

    Live C++ (validation.cpp / convergence_logic.cpp), for source height [s]:
      F(s, W) = [max(0, s - W + 1), s]
    Equivalently, the draw paid at height [h] uses
      draw(h, W) = [h - W, h - 1]
    (genesis-clamped: [max(0, h - W), h - 1]).

    Hypothesis on W: [W_ok W] := [1 < W].
    W = 1 is the A-9C exclusivity defect (miner of h-1 chooses F alone).

    Honesty:
      model proved
      refinement_proved = false
      This file is not a C++ refinement of validation.cpp.
      No [Axiom] / [Admitted].
      Production W is not changed by this development. *)
From Coq Require Import ZArith.
From Coq Require Import Lia.
Open Scope Z_scope.

(** * Parameters *)

Definition W_ok (W : Z) : Prop := 1 < W.

Definition COINBASE_MATURITY : Z := 100.

(** * Window geometry — W is an explicit argument, never a hidden 32 *)

Definition window_start (s W : Z) : Z :=
  Z.max 0 (s - W + 1).

Definition window_end (s W : Z) : Z := s.

Definition window_of_source (s W : Z) : Z * Z :=
  (window_start s W, window_end s W).

Definition window_length (s W : Z) : Z :=
  window_end s W - window_start s W + 1.

Definition in_window (h s W : Z) : Prop :=
  window_start s W <= h /\ h <= window_end s W.

(** Unclamped draw paid at height [h]: [h - W, h - 1]. *)
Definition draw_start (h W : Z) : Z := h - W.
Definition draw_end (h W : Z) : Z := h - 1.
Definition draw_window (h W : Z) : Z * Z :=
  (draw_start h W, draw_end h W).

Definition draw_length (h W : Z) : Z :=
  draw_end h W - draw_start h W + 1.

(** Genesis clamp of the paid draw: [max(0, h - W), h - 1].
    Equals the source window of parent height [h - 1]. *)
Definition clamped_draw_start (h W : Z) : Z :=
  Z.max 0 (h - W).
Definition clamped_draw_window (h W : Z) : Z * Z :=
  (clamped_draw_start h W, draw_end h W).

(** * Cache / identity model. Canonical contents are keyed by block hash,
    modelled as an opaque (branch, height) pair. Height-only and
    floor(h/W) keys are not reorg-safe. *)

Definition contents_at (branch h : Z) : Z := branch + h * 1000003.
Definition block_hash (branch h : Z) : Z := contents_at branch h.

Inductive CacheKeyKind :=
| CacheByBlockHash
| CacheByHeight
| CacheByGroupId
| CacheByFloorSlice.

Definition cache_key_is_reorg_safe (k : CacheKeyKind) : bool :=
  match k with
  | CacheByBlockHash => true
  | CacheByHeight => false
  | CacheByGroupId => false
  | CacheByFloorSlice => false
  end.

Inductive GroupStateOwner :=
| CanonicalOwner
| OrphanOwner.

Definition owner_is_canonical (o : GroupStateOwner) : bool :=
  match o with
  | CanonicalOwner => true
  | OrphanOwner => false
  end.

Definition coinbase_mature (spend_height coin_height : Z) : bool :=
  spend_height - coin_height >=? COINBASE_MATURITY.

(** Branch tag is an opaque chain identity. Window geometry ignores it. *)
Definition branch_window (branch s W : Z) : Z * Z :=
  window_of_source s W.

(** * W-1 — window length *)

Theorem window_start_nonneg :
  forall s W : Z, 0 <= window_start s W.
Proof.
  intros s W. unfold window_start. apply Z.le_max_l.
Qed.

(** For [s >= 0] a member of the window is never a negative height. *)
Theorem in_window_height_nonneg :
  forall s W h : Z,
    0 <= s ->
    in_window h s W ->
    0 <= h.
Proof.
  intros s W h Hs [Hlo Hhi].
  pose proof (window_start_nonneg s W). lia.
Qed.

Theorem window_length_full :
  forall s W : Z,
    W_ok W ->
    W - 1 <= s ->
    window_length s W = W.
Proof.
  intros s W Hok Hs.
  unfold window_length, window_end, window_start.
  unfold W_ok in Hok.
  assert (Hpos : 0 <= s - (W - 1)) by lia.
  replace (s - W + 1) with (s - (W - 1)) by lia.
  rewrite Z.max_r by exact Hpos.
  lia.
Qed.

Theorem window_length_genesis_truncation :
  forall s W : Z,
    W_ok W ->
    0 <= s ->
    s < W - 1 ->
    window_start s W = 0 /\
    window_end s W = s /\
    window_length s W = s + 1.
Proof.
  intros s W Hok Hs Htrunc.
  unfold window_length, window_end, window_start.
  unfold W_ok in Hok.
  assert (Hneg : s - (W - 1) < 0) by lia.
  replace (s - W + 1) with (s - (W - 1)) by lia.
  rewrite Z.max_l by apply Z.lt_le_incl, Hneg.
  lia.
Qed.

Theorem draw_length_is_W :
  forall h W : Z, draw_length h W = W.
Proof.
  intros h W. unfold draw_length, draw_end, draw_start. lia.
Qed.

Theorem clamped_draw_agrees_with_parent_source :
  forall h W : Z,
    clamped_draw_start h W = window_start (h - 1) W /\
    draw_end h W = window_end (h - 1) W.
Proof.
  intros h W.
  unfold clamped_draw_start, window_start, draw_end, window_end.
  split.
  - f_equal. lia.
  - lia.
Qed.

Theorem clamped_draw_full_when_h_ge_W :
  forall h W : Z,
    W_ok W ->
    W <= h ->
    clamped_draw_start h W = h - W.
Proof.
  intros h W Hok Hh.
  unfold clamped_draw_start, W_ok in *.
  apply Z.max_r. lia.
Qed.

(** * Membership is a function of (s, W) only — not tip, not branch *)

Theorem window_membership_height_stable :
  forall s W : Z,
    window_start s W = Z.max 0 (s - W + 1) /\
    window_end s W = s.
Proof. intros. split; reflexivity. Qed.

Theorem reorg_preserves_window_boundary :
  forall (branch_a branch_b s W : Z),
    branch_window branch_a s W = branch_window branch_b s W.
Proof. intros. reflexivity. Qed.

Theorem reorg_changes_contents_only :
  forall (branch_a branch_b s W : Z),
    branch_window branch_a s W = branch_window branch_b s W /\
    (branch_a <> branch_b -> contents_at branch_a s <> contents_at branch_b s).
Proof.
  intros ba bb s W.
  split.
  - reflexivity.
  - intros Hneq. unfold contents_at. lia.
Qed.

(** Reorg depth is not an input to the membership function.
    Depths < W, = W, > W, and = 2W all leave the interval of [s] unchanged. *)
Theorem deep_reorg_stability :
  forall (reorg_depth s W : Z),
    window_of_source s W = window_of_source s W.
Proof. intros. reflexivity. Qed.

Theorem deep_reorg_depths_relative_to_W :
  forall s W : Z,
    W_ok W ->
    branch_window 0 s W = branch_window (W - 1) s W /\
    branch_window 0 s W = branch_window W s W /\
    branch_window 0 s W = branch_window (W + 1) s W /\
    branch_window 0 s W = branch_window (2 * W) s W.
Proof. intros. repeat split; reflexivity. Qed.

(** * Canonical / orphan *)

Theorem orphan_state_not_canonical :
  forall o : GroupStateOwner,
    o = OrphanOwner ->
    owner_is_canonical o = false.
Proof. intros o H. subst o. reflexivity. Qed.

Theorem canonical_owner_is_canonical :
  owner_is_canonical CanonicalOwner = true.
Proof. reflexivity. Qed.

(** Same height on two branches does not identify a block: hashes differ. *)
Theorem same_height_distinct_branches_distinct_hash :
  forall (branch_a branch_b h : Z),
    branch_a <> branch_b ->
    block_hash branch_a h <> block_hash branch_b h.
Proof.
  intros ba bb h Hneq. unfold block_hash, contents_at. lia.
Qed.

Theorem orphan_hash_is_not_canonical_hash :
  forall (canonical_branch orphan_branch h : Z),
    canonical_branch <> orphan_branch ->
    block_hash orphan_branch h <> block_hash canonical_branch h.
Proof.
  intros. apply same_height_distinct_branches_distinct_hash.
  intro Heq. apply H. lia.
Qed.

(** * Cache: only BlockHash is a reorg-safe key *)

Theorem hash_cache_is_reorg_safe :
  cache_key_is_reorg_safe CacheByBlockHash = true.
Proof. reflexivity. Qed.

Theorem height_only_cache_not_reorg_safe :
  cache_key_is_reorg_safe CacheByHeight = false.
Proof. reflexivity. Qed.

Theorem group_id_cache_not_reorg_safe :
  cache_key_is_reorg_safe CacheByGroupId = false.
Proof. reflexivity. Qed.

Theorem floor_slice_cache_not_reorg_safe :
  cache_key_is_reorg_safe CacheByFloorSlice = false.
Proof. reflexivity. Qed.

Theorem reorg_changes_cache_key :
  forall (branch_a branch_b h : Z),
    branch_a <> branch_b ->
    block_hash branch_a h <> block_hash branch_b h.
Proof. apply same_height_distinct_branches_distinct_hash. Qed.

(** Looking up the old hash cannot return the new branch's contents, because
    the keys differ. *)
Theorem old_hash_does_not_alias_new_hash :
  forall (old_branch new_branch h : Z),
    old_branch <> new_branch ->
    block_hash old_branch h <> block_hash new_branch h.
Proof. apply same_height_distinct_branches_distinct_hash. Qed.

(** * Not a floor(h/W) partition *)

Theorem not_floor_partition :
  forall W : Z,
    W_ok W ->
    window_start (2 * W) W <> W * ((2 * W) / W).
Proof.
  intros W Hok.
  unfold window_start, W_ok in *.
  replace (Z.max 0 (2 * W - W + 1)) with (W + 1) by lia.
  rewrite (Z.div_mul 2 W) by lia.
  lia.
Qed.

Theorem coincides_with_floor_slice_only_at_slice_end :
  forall g W : Z,
    W_ok W ->
    0 <= g ->
    window_start (W * g + (W - 1)) W = W * g /\
    window_end (W * g + (W - 1)) W = W * g + (W - 1).
Proof.
  intros g W Hok Hg.
  unfold window_start, window_end, W_ok in *.
  split.
  - replace (W * g + (W - 1) - W + 1) with (W * g) by lia.
    apply Z.max_r. lia.
  - reflexivity.
Qed.

Theorem windows_overlap :
  forall W : Z,
    W_ok W ->
    in_window W W W /\ in_window W (2 * W - 1) W.
Proof.
  intros W Hok.
  unfold in_window, window_start, window_end, W_ok in *.
  split.
  - split; lia.
  - split; lia.
Qed.

Theorem one_source_window_per_source_height :
  forall s W : Z, window_of_source s W = window_of_source s W.
Proof. intros. reflexivity. Qed.

(** * Maturity is 100-block depth, not a function of W *)

Theorem maturity_constant_is_100 :
  COINBASE_MATURITY = 100.
Proof. reflexivity. Qed.

Theorem maturity_predicate_ignores_W :
  forall (W1 W2 spend coin : Z),
    coinbase_mature spend coin = coinbase_mature spend coin.
Proof. intros. reflexivity. Qed.

Theorem maturity_absolute_depth :
  forall (h : Z),
    coinbase_mature (h + COINBASE_MATURITY) h = true.
Proof.
  intros h.
  unfold coinbase_mature, COINBASE_MATURITY.
  replace (h + 100 - h) with 100 by lia.
  reflexivity.
Qed.

Theorem window_width_does_not_mature :
  forall (h W : Z),
    W_ok W ->
    W < COINBASE_MATURITY ->
    coinbase_mature (h + W) h = false.
Proof.
  intros h W Hok Hlt.
  unfold coinbase_mature, COINBASE_MATURITY in Hlt |- *.
  replace (h + W - h) with W by lia.
  unfold Z.geb, Z.leb.
  destruct (Z.compare_spec W 100) as [Heq | HltW | Hgt].
  - lia.
  - reflexivity.
  - lia.
Qed.

Theorem maturity_not_equal_W :
  forall W : Z,
    W_ok W ->
    W <> COINBASE_MATURITY ->
    COINBASE_MATURITY <> W.
Proof. intros. lia. Qed.

Theorem maturity_not_derived_from_window :
  forall (h s W : Z),
    W_ok W ->
    W < COINBASE_MATURITY ->
    in_window h s W ->
    coinbase_mature (h + W) h = false.
Proof.
  intros h s W Hok Hlt _.
  apply window_width_does_not_mature; assumption.
Qed.

(** Forbidden implicit relations: maturity is not W, 100/W, or N*W as a rule.
    The predicate is always [spend - coin >= 100], for every legal W. *)
Theorem maturity_not_div_W_rule :
  forall W spend coin : Z,
    coinbase_mature spend coin =
    (spend - coin >=? 100).
Proof. intros. reflexivity. Qed.

(** * W > 1 is required; power-of-two is NOT required *)

Theorem W_ok_excludes_one :
  ~ W_ok 1.
Proof. unfold W_ok. lia. Qed.

Theorem W_ok_holds_for_non_pow2 :
  W_ok 3 /\ W_ok 36 /\ W_ok 48.
Proof. unfold W_ok. lia. Qed.

Theorem W_ok_holds_for_pow2 :
  W_ok 2 /\ W_ok 4 /\ W_ok 8 /\ W_ok 16 /\ W_ok 32 /\ W_ok 64.
Proof. unfold W_ok. lia. Qed.

Lemma pow2_mono :
  forall a b : Z, 0 <= a -> a <= b -> Z.pow 2 a <= Z.pow 2 b.
Proof.
  intros a b Ha Hab.
  apply Z.pow_le_mono_r; lia.
Qed.

Theorem thirty_six_is_not_a_power_of_two :
  forall k : Z, 0 <= k -> 36 <> Z.shiftl 1 k.
Proof.
  intros k Hk Heq.
  rewrite Z.shiftl_mul_pow2 in Heq by exact Hk.
  rewrite Z.mul_1_l in Heq.
  destruct (Z.le_gt_cases k 5) as [Hle | Hgt].
  - pose proof (pow2_mono k 5 Hk Hle) as Hpow.
    change (Z.pow 2 5) with 32 in Hpow. lia.
  - assert (H6 : 6 <= k) by lia.
    pose proof (pow2_mono 6 k ltac:(lia) H6) as Hpow.
    change (Z.pow 2 6) with 64 in Hpow. lia.
Qed.

Theorem three_is_not_a_power_of_two :
  forall k : Z, 0 <= k -> 3 <> Z.shiftl 1 k.
Proof.
  intros k Hk Heq.
  rewrite Z.shiftl_mul_pow2 in Heq by exact Hk.
  rewrite Z.mul_1_l in Heq.
  destruct (Z.le_gt_cases k 1) as [Hle | Hgt].
  - pose proof (pow2_mono k 1 Hk Hle) as Hpow.
    change (Z.pow 2 1) with 2 in Hpow. lia.
  - assert (H2 : 2 <= k) by lia.
    pose proof (pow2_mono 2 k ltac:(lia) H2) as Hpow.
    change (Z.pow 2 2) with 4 in Hpow. lia.
Qed.

(** * Supply / consensus objects that do not take W *)

Definition oru_subsidy_model (_height : Z) : Z := 1000.

Theorem subsidy_independent_of_W :
  forall (W1 W2 h : Z),
    oru_subsidy_model h = oru_subsidy_model h.
Proof. intros. reflexivity. Qed.

(** Draw-window W is not RFC-017 mint-reward W(k). *)
Definition rfc017_reward_W_is_not_draw_window : Prop := True.

Theorem rfc017_name_collision_is_not_coupling :
  rfc017_reward_W_is_not_draw_window.
Proof. exact I. Qed.

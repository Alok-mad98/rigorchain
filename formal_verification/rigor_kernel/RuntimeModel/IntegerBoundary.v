(** Kernel-domain integers vs the documented runtime word bound.

    Every theorem here has the shape "values produced by a kernel
    domain fit in a signed 63-bit magnitude". The domains are the real
    ones — [money_range], the block-height range, the compact-bits
    target encoding, and the ASERT time-error input — so the bound has
    to be derived rather than restated.

    Two earlier theorems in this file ([target_range_safe],
    [time_error_range_safe]) were identities: their premise was
    syntactically their conclusion and the proof was [exact H]. They
    are now stated over the domains that actually produce those
    values.

    Does not prove anything about all OCaml [int]s or all CPU
    integers.

    NOT CLAIMED: compiler integer semantics, ABI, machine words.
*)
From Coq Require Import ZArith.ZArith.
From Coq Require Import Lia.
From RigorKernel Require Import Core.Amount.
From RigorKernel Require Import Pow.CompactBits.
From RigorKernel Require Import Pow.ASERTModel.

Local Open Scope Z_scope.

Definition runtime_signed_max : Z := 2 ^ 62 - 1.

Lemma two_pow_ge_1 : forall k : Z, 0 <= k -> 1 <= 2 ^ k.
Proof.
  intros k Hk. rewrite <- (Z.pow_0_r 2).
  apply Z.pow_le_mono_r; lia.
Qed.

(** * Monetary values *)

Definition integer_in_kernel_range (z : Z) : Prop := money_range z.

Theorem integer_range_safe :
  forall z : Z,
    integer_in_kernel_range z ->
    0 <= z <= runtime_signed_max.
Proof.
  intros z H.
  unfold integer_in_kernel_range, money_range, MAX_MONEY, COIN,
         runtime_signed_max in *.
  lia.
Qed.

Theorem money_range_safe :
  forall z : Z,
    money_range z ->
    0 <= z <= runtime_signed_max.
Proof.
  intros z H. apply integer_range_safe. exact H.
Qed.

(** * Block heights *)

Definition height_in_kernel_range (h : Z) : Prop :=
  0 <= h <= 21000000.

Theorem height_range_safe :
  forall h : Z,
    height_in_kernel_range h ->
    0 <= h <= runtime_signed_max.
Proof.
  intros h H.
  unfold height_in_kernel_range, runtime_signed_max in *.
  lia.
Qed.

(** * PoW targets

    A target is not an arbitrary integer: the kernel obtains it by
    decoding a compact-bits word. The runtime bound is derived from
    the encoding — a 23-bit mantissa shifted left by at most 32 bits
    — not assumed. *)

Definition compact_in_kernel_domain (nbits : Z) : Prop :=
  0 <= nbits /\ compact_size nbits <= 7.

Theorem target_range_safe :
  forall nbits : Z,
    compact_in_kernel_domain nbits ->
    0 <= compact_decode nbits <= runtime_signed_max.
Proof.
  intros nbits [Hnn Hsize].
  unfold compact_decode, compact_in_kernel_domain in *.
  unfold compact_size in *.
  set (size := nbits / 16777216) in *.
  set (word := compact_mantissa nbits) in *.
  assert (Hword : 0 <= word < 8388608).
  { unfold word, compact_mantissa. apply Z.mod_pos_bound. lia. }
  assert (Hsz0 : 0 <= size).
  { unfold size. apply Z.div_pos; lia. }
  destruct (size <=? 3) eqn:E.
  - apply Z.leb_le in E.
    assert (Hk : 0 <= 8 * (3 - size)) by lia.
    rewrite Z.shiftr_div_pow2 by exact Hk.
    split.
    + apply Z.div_pos; [lia | apply Z.pow_pos_nonneg; lia].
    + assert (Hpow : 1 <= 2 ^ (8 * (3 - size))) by (apply two_pow_ge_1; lia).
      assert (word / 2 ^ (8 * (3 - size)) <= word).
      { apply Z.div_le_upper_bound; [apply Z.pow_pos_nonneg; lia | nia]. }
      unfold runtime_signed_max. lia.
  - apply Z.leb_gt in E.
    assert (Hk : 0 <= 8 * (size - 3)) by lia.
    rewrite Z.shiftl_mul_pow2 by exact Hk.
    split.
    + apply Z.mul_nonneg_nonneg; [lia | apply Z.pow_nonneg; lia].
    + assert (Hle : 2 ^ (8 * (size - 3)) <= 2 ^ 32).
      { apply Z.pow_le_mono_r; lia. }
      assert (H32 : 2 ^ 32 = 4294967296) by reflexivity.
      assert (Hpos : 0 < 2 ^ (8 * (size - 3))) by (apply Z.pow_pos_nonneg; lia).
      unfold runtime_signed_max.
      assert (word * 2 ^ (8 * (size - 3)) <= 8388607 * 4294967296) by nia.
      lia.
Qed.

(** The domain restriction is load-bearing: drop it and the bound is
    false. Without this witness [compact_in_kernel_domain] could be
    weakened to [True] and the theorem above would still be provable
    only if it were vacuous. *)
Theorem target_domain_restriction_is_necessary :
  exists nbits : Z,
    0 <= nbits /\ runtime_signed_max < compact_decode nbits.
Proof.
  exists (32 * 16777216 + 1).
  split; [lia |].
  unfold runtime_signed_max, compact_decode, compact_size, compact_mantissa.
  vm_compute. reflexivity.
Qed.

(** * ASERT time error

    [time_error] is [(t_par - t_anc) - (h_par - h_anc) * spacing]. The
    bound follows from the header-timestamp, height and spacing ranges
    the kernel accepts; the product term is what actually needs
    bounding. *)

Definition asert_time_input_in_kernel_domain
    (t_par t_anc h_par h_anc spacing : Z) : Prop :=
  0 <= t_anc <= t_par /\
  t_par <= 2 ^ 40 /\
  0 <= h_anc <= h_par /\
  h_par <= 21000000 /\
  0 <= spacing <= 86400.

Theorem time_error_range_safe :
  forall t_par t_anc h_par h_anc spacing : Z,
    asert_time_input_in_kernel_domain t_par t_anc h_par h_anc spacing ->
    - runtime_signed_max
      <= time_error t_par t_anc h_par h_anc spacing
      <= runtime_signed_max.
Proof.
  intros t_par t_anc h_par h_anc spacing
         [Ht [Htmax [Hh [Hhmax Hsp]]]].
  unfold time_error, runtime_signed_max.
  assert (Hprod : 0 <= (h_par - h_anc) * spacing <= 21000000 * 86400).
  { split.
    - apply Z.mul_nonneg_nonneg; lia.
    - apply Z.mul_le_mono_nonneg; lia. }
  assert (H40 : 2 ^ 40 = 1099511627776) by reflexivity.
  lia.
Qed.

(** Same non-vacuity check for the ASERT input domain. *)
Theorem time_error_domain_restriction_is_necessary :
  exists t_par t_anc h_par h_anc spacing : Z,
    runtime_signed_max < time_error t_par t_anc h_par h_anc spacing.
Proof.
  exists (2 ^ 63), 0, 0, 0, 0.
  unfold runtime_signed_max, time_error.
  vm_compute. reflexivity.
Qed.

(** * Bundle

    Every numeric quantity that crosses the runtime boundary is
    accounted for, and each conjunct is now a derivation. *)

Theorem runtime_domain_closed :
  (forall z, money_range z -> 0 <= z <= runtime_signed_max) /\
  (forall h, height_in_kernel_range h -> 0 <= h <= runtime_signed_max) /\
  (forall nbits, compact_in_kernel_domain nbits ->
     0 <= compact_decode nbits <= runtime_signed_max) /\
  (forall t_par t_anc h_par h_anc spacing,
     asert_time_input_in_kernel_domain t_par t_anc h_par h_anc spacing ->
     - runtime_signed_max
       <= time_error t_par t_anc h_par h_anc spacing
       <= runtime_signed_max).
Proof.
  split; [exact money_range_safe|].
  split; [exact height_range_safe|].
  split; [exact target_range_safe|].
  exact time_error_range_safe.
Qed.

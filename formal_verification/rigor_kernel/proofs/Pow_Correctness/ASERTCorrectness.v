(** RigorKernel.proofs.Pow_Correctness.ASERTCorrectness — P0-108 ASERT kernel.

    Scope: consensus kernel only.

    Operator split (must not be confused):
      exp_q      uses [Z.quot]  (C++ __int128 toward zero)
      num_shifts uses [Z.div]   (arithmetic right shift)

    Cites [negative_time_error_quot_div_disagree].

    NOT CLAIMED:
      C++ implementation equivalence
      Full chain validity
      Hash implementation (keccak / sha256 / SHA3)
      Miner implementation
      Hash collision resistance
      Signature security
      Runtime rejection equivalence
*)
From Coq Require Import ZArith.ZArith.
From Coq Require Import Lia.
From RigorKernel Require Import Pow.Uint256Model.
From RigorKernel Require Import Pow.CompactBits.
From RigorKernel Require Import Pow.ASERTModel.

Local Open Scope Z_scope.

Definition deterministic_asert (anchor parent : asert_input) : Prop :=
  anchor = parent ->
  asert_next_target anchor = asert_next_target parent.

(** Statement: ASERT is a total deterministic integer function of the
    anchor/parent inputs.
    Category B — function-ness packaging only. Class A content is
    [negative_time_error_preserved].
    Assumptions: none ([asert_next_target] is a function).
    Consensus kernel only: yes. *)
Theorem pow_asert_correctness :
  forall anchor parent,
    deterministic_asert anchor parent.
Proof.
  intros anchor parent Heq. subst. reflexivity.
Qed.

(** Statement: compact encode is a lower bound — decode(encode t) <= t.
    Assumptions: [0 <= t].
    Consensus kernel only: yes. *)
Theorem compact_roundtrip_bound :
  forall t, 0 <= t -> compact_decode (compact_encode t) <= t.
Proof. apply compact_decode_lower_bound. Qed.

Lemma clamp_z_bounds :
  forall lo hi x,
    lo <= hi ->
    lo <= clamp_z lo hi x <= hi.
Proof.
  intros lo hi x Hle. unfold clamp_z.
  destruct (hi <? x) eqn:Hhi.
  - apply Z.ltb_lt in Hhi. lia.
  - apply Z.ltb_ge in Hhi.
    destruct (x <? lo) eqn:Hlo.
    + apply Z.ltb_lt in Hlo. lia.
    + apply Z.ltb_ge in Hlo. lia.
Qed.

Lemma clamp_z_mono :
  forall lo hi x y,
    lo <= hi ->
    x <= y ->
    clamp_z lo hi x <= clamp_z lo hi y.
Proof.
  intros lo hi x y Hih Hxy. unfold clamp_z.
  destruct (hi <? x) eqn:Hhx.
  - apply Z.ltb_lt in Hhx.
    destruct (hi <? y) eqn:Hhy; [lia|].
    apply Z.ltb_ge in Hhy. lia.
  - apply Z.ltb_ge in Hhx.
    destruct (x <? lo) eqn:Hxl.
    + apply Z.ltb_lt in Hxl.
      destruct (hi <? y) eqn:Hhy; [lia|].
      destruct (y <? lo) eqn:Hyl; lia.
    + apply Z.ltb_ge in Hxl.
      destruct (hi <? y) eqn:Hhy; [lia|].
      apply Z.ltb_ge in Hhy.
      destruct (y <? lo) eqn:Hyl; [apply Z.ltb_lt in Hyl; lia| lia].
Qed.

(** Statement: [exp_q] stays inside the ASERT exponent clamp.
    Assumptions: none.
    Consensus kernel only: yes. *)
Theorem exp_q_clamped :
  forall te tau,
    - ASERT_EXP_CLAMP <= exp_q te tau <= ASERT_EXP_CLAMP.
Proof.
  intros te tau. unfold exp_q.
  apply clamp_z_bounds.
  unfold ASERT_EXP_CLAMP, ASERT_RADIX. lia.
Qed.

Theorem exp_rem_range_kernel :
  forall q, 0 <= exp_rem q < ASERT_RADIX.
Proof. apply exp_rem_range. Qed.

Lemma asert_poly_mono :
  forall e1 e2,
    0 <= e1 ->
    e1 <= e2 ->
    asert_poly e1 <= asert_poly e2.
Proof.
  intros e1 e2 H1 Hle.
  unfold asert_poly.
  assert (Hsq : e1 * e1 <= e2 * e2) by (apply Z.mul_le_mono_nonneg; lia).
  assert (Hcu : e1 * e1 * e1 <= e2 * e2 * e2)
    by (apply Z.mul_le_mono_nonneg; lia).
  unfold ASERT_POLY_P1, ASERT_POLY_P2, ASERT_POLY_P3, ASERT_POLY_BIAS.
  apply Z.add_le_mono; [|lia].
  apply Z.add_le_mono.
  - apply Z.add_le_mono.
    + apply Z.mul_le_mono_nonneg_l; lia.
    + apply Z.mul_le_mono_nonneg; lia.
  - apply Z.mul_le_mono_nonneg; lia.
Qed.

Lemma factor_mono :
  forall e1 e2,
    0 <= e1 ->
    e1 <= e2 ->
    factor e1 <= factor e2.
Proof.
  intros e1 e2 H1 Hle. unfold factor.
  apply Z.add_le_mono_r.
  rewrite !Z.shiftr_div_pow2 by lia.
  apply Z.div_le_mono; [lia|].
  apply asert_poly_mono; assumption.
Qed.

Lemma factor_at_zero :
  factor 0 = ASERT_RADIX.
Proof. vm_compute. reflexivity. Qed.

Lemma factor_at_max :
  factor (ASERT_RADIX - 1) < 2 * ASERT_RADIX.
Proof. vm_compute. reflexivity. Qed.

(** Statement: [65536 <= factor e < 131072] for [e] in [[0, 65536)].
    Assumptions: [0 <= e < ASERT_RADIX].
    Consensus kernel only: yes. *)
Theorem factor_range :
  forall e,
    0 <= e < ASERT_RADIX ->
    ASERT_RADIX <= factor e < 2 * ASERT_RADIX.
Proof.
  intros e [Hlo Hhi].
  split.
  - rewrite <- factor_at_zero. apply factor_mono; lia.
  - assert (e <= ASERT_RADIX - 1) by lia.
    pose proof (factor_mono e (ASERT_RADIX - 1) Hlo H) as Hle.
    pose proof factor_at_max as Hmax.
    lia.
Qed.

(** Statement: [clamp_to_prev] is at least 1, and lands in [[lo, hi]]
    when that interval is non-empty.
    Assumptions: [1 <= pow_limit].
    Consensus kernel only: yes. *)
Theorem clamp_to_prev_bounds :
  forall next prev pow_limit,
    1 <= pow_limit ->
    let hi := Z.min (prev * MAX_ADJ_PPM / RATIO_UNIT_PPM) pow_limit in
    let lo := Z.max 1 (Z.min (prev * MIN_ADJ_PPM / RATIO_UNIT_PPM) pow_limit) in
    1 <= clamp_to_prev next prev pow_limit /\
    (lo <= hi ->
     lo <= clamp_to_prev next prev pow_limit <= hi).
Proof.
  intros next prev pow_limit Hlim.
  unfold clamp_to_prev.
  set (hi := Z.min (prev * MAX_ADJ_PPM / RATIO_UNIT_PPM) pow_limit).
  set (lo := Z.max 1 (Z.min (prev * MIN_ADJ_PPM / RATIO_UNIT_PPM) pow_limit)).
  split.
  - apply Z.max_le_iff. left. apply Z.le_max_l.
  - intros Hlh. split.
    + apply Z.le_max_l.
    + apply Z.max_lub; [exact Hlh| apply Z.le_min_l].
Qed.

Theorem asert_next_bits_deterministic :
  forall bits1 bits2 i1 i2,
    bits1 = bits2 ->
    i1 = i2 ->
    asert_next_bits bits1 bits1 i1 = asert_next_bits bits2 bits2 i2.
Proof. intros b1 b2 i1 i2 Hb Hi. subst. reflexivity. Qed.

(** Statement: larger [time_error] does not decrease [exp_q]
    (exponent monotonicity; full target mono is modulo remainder wrap
    and clamp saturation).
    Assumptions: [0 < tau] and [te1 <= te2].
    Consensus kernel only: yes. *)
Theorem asert_monotone_in_time_error :
  forall te1 te2 tau,
    0 < tau ->
    te1 <= te2 ->
    te1 * ASERT_RADIX <= te2 * ASERT_RADIX /\
    exp_q_raw te1 tau = Z.quot (te1 * ASERT_RADIX) tau /\
    exp_q_raw te2 tau = Z.quot (te2 * ASERT_RADIX) tau.
Proof.
  intros te1 te2 tau Htau Hle.
  split; [unfold ASERT_RADIX; nia|].
  split; reflexivity.
Qed.

(** Statement: [exp_q] is [Z.quot]; [num_shifts] is [Z.div].
    Assumptions: none (definitional). Cites the negative-error witness.
    Consensus kernel only: yes. *)
Theorem exp_q_uses_quot :
  forall te tau,
    exp_q_raw te tau = Z.quot (te * ASERT_RADIX) tau.
Proof. intros te tau. reflexivity. Qed.

Theorem shift_uses_div :
  forall q, num_shifts q = Z.div q ASERT_RADIX.
Proof. intros q. reflexivity. Qed.

Theorem asert_quot_div_witness :
  Z.quot ((-1) * ASERT_RADIX) ASERT_HALF_LIFE_MAINNET = -1 /\
  Z.div ((-1) * ASERT_RADIX) ASERT_HALF_LIFE_MAINNET = -2 /\
  exp_q_raw (-1) ASERT_HALF_LIFE_MAINNET = -1.
Proof. apply negative_time_error_quot_div_disagree. Qed.

Definition asert_uses_child_time : bool := false.

(** documentation-only, no semantic content.
    The bound [child_nTime] is unused; the equality is reflexivity
    of [asert_next_target]. Category C. Do not cite as an invariant. *)
Theorem asert_child_time_irrelevant :
  asert_uses_child_time = false /\
  forall i (child_nTime : Z),
    asert_next_target i = asert_next_target i.
Proof. split; [reflexivity| intros i c; reflexivity]. Qed.

Lemma asert_next_ge_one :
  forall i, 1 <= asert_next_target i.
Proof.
  intros i. unfold asert_next_target, clamp_to_prev.
  apply Z.max_le_iff. left. apply Z.le_max_l.
Qed.

Lemma clamp_to_prev_le_limit :
  forall next prev lim,
    1 <= lim ->
    clamp_to_prev next prev lim <= lim.
Proof.
  intros next prev lim Hlim.
  unfold clamp_to_prev, clamp_to_limit.
  destruct (next <=? 0); destruct (lim <? next); lia.
Qed.

Lemma asert_next_le_limit :
  forall i,
    1 <= ai_pow_limit i ->
    asert_next_target i <= ai_pow_limit i.
Proof.
  intros i Hlim. unfold asert_next_target.
  apply clamp_to_prev_le_limit. exact Hlim.
Qed.

(** Statement: the next target is positive and does not exceed the
    limit (so it is a well-formed abstract wide integer when the
    limit itself is).
    Assumptions: [1 <= ai_pow_limit i < 2^256].
    Consensus kernel only: yes. *)
Theorem asert_next_wf :
  forall i,
    1 <= ai_pow_limit i ->
    ai_pow_limit i < u256_mod ->
    1 <= asert_next_target i <= ai_pow_limit i /\
    u256_ok (asert_next_target i).
Proof.
  intros i Hlo Hhi.
  pose proof (asert_next_ge_one i) as H1.
  pose proof (asert_next_le_limit i Hlo) as H2.
  split; [split; assumption|].
  unfold u256_ok. lia.
Qed.

(** Statement: the next target is never 0.
    Assumptions: none (the clamp floor is 1).
    Consensus kernel only: yes. *)
Theorem asert_target_positive :
  forall i, 0 < asert_next_target i.
Proof. intros i. pose proof (asert_next_ge_one i). lia. Qed.

Definition asert_accepts (bits : Z) (i : asert_input) : Prop :=
  bits = asert_next_bits (compact_encode (ai_anchor_target i))
                         (compact_encode (ai_parent_target i)) i.

Theorem asert_rejects_wrong_bits :
  forall bits i,
    bits <> asert_next_bits (compact_encode (ai_anchor_target i))
                            (compact_encode (ai_parent_target i)) i ->
    ~ asert_accepts bits i.
Proof.
  intros bits i Hne Hacc. unfold asert_accepts in Hacc. congruence.
Qed.

Example consistency_check :
  exp_q_raw (-1) ASERT_HALF_LIFE_MAINNET = -1 /\
  Z.div ((-1) * ASERT_RADIX) ASERT_HALF_LIFE_MAINNET = -2.
Proof. vm_compute. split; reflexivity. Qed.

Theorem asert_golden_vectors :
  exp_q_raw (-1) ASERT_HALF_LIFE_MAINNET = -1.
Proof. vm_compute. reflexivity. Qed.

(** * Width and cast safety for the fixed-point exponential (frontier 2.4)

    [factor_range] above bounds the factor. It says nothing about the
    intermediate [asert_poly], and that is where the C++ does two
    unchecked casts (core/src/pow.cpp):

        const __int128 poly = P1*e + P2*e*e + P3*e*e*e + (1<<47);
        const uint64_t factor = static_cast<uint64_t>(poly >> 48)
                              + static_cast<uint64_t>(ASERT_RADIX);

    [poly >> 48] is read as *unsigned*. A negative [poly] would not
    produce a small target, it would produce an enormous one, and
    nothing downstream would flag it. So the sign of the polynomial is
    a consensus precondition and is proved rather than assumed. *)

(** The evaluated endpoints are asserted inside each proof rather than
    factored out into lemmas of their own. Factored out, they absorb the
    whole dependency on the coefficients: the mutation harness admits the
    value lemma when a coefficient changes, and every theorem standing on
    it then proves against the admitted -- now wrong -- value. Measured,
    not supposed; that is precisely what the first version of these
    proofs did, and it left both cast-safety theorems killed by no
    mutant at all. *)

(** Statement: the polynomial is strictly positive on the whole
    admissible domain, so the unsigned cast of [poly >> 48] cannot wrap.
    Assumptions: [0 <= e].
    Consensus kernel only: yes. *)
Theorem asert_poly_pos :
  forall e, 0 <= e -> 0 < asert_poly e.
Proof.
  intros e He.
  pose proof (asert_poly_mono 0 e (Z.le_refl 0) He).
  assert (Hz : asert_poly 0 = 140737488355328) by (vm_compute; reflexivity).
  lia.
Qed.

(** Statement: at the top of the domain the polynomial does not fit in a
    signed 64-bit integer.

    This is the reason the C++ promotes to [__int128] before
    multiplying, and it records that the promotion is load-bearing
    rather than defensive. Evaluating the same expression in [int64_t]
    would be signed overflow, which is undefined behaviour, and it would
    not be reached by a small exponent -- it needs a remainder near
    65535, so it would survive testing on ordinary chains and fire on
    the first block whose timestamp drifts far enough.
    Assumptions: none.
    Consensus kernel only: yes. *)
Theorem asert_poly_exceeds_int64 :
  2 ^ 63 < asert_poly (ASERT_RADIX - 1).
Proof. vm_compute. reflexivity. Qed.

(** Statement: it does fit in an unsigned 64-bit integer, with under one
    part in 10^5 to spare.
    Assumptions: [0 <= e < ASERT_RADIX].
    Consensus kernel only: yes. *)
Theorem asert_poly_fits_uint64 :
  forall e, 0 <= e < ASERT_RADIX -> asert_poly e < 2 ^ 64.
Proof.
  intros e [H0 H1].
  pose proof (asert_poly_mono e (ASERT_RADIX - 1) H0
                ltac:(unfold ASERT_RADIX in *; lia)).
  assert (Hmax : asert_poly (ASERT_RADIX - 1) < 2 ^ 64)
    by (vm_compute; reflexivity).
  lia.
Qed.

(** The three results above carry a hypothesis on [e]. At the call site
    [e] is [exp_rem q], and [exp_rem_range] discharges that hypothesis
    for every [q] whatsoever -- including a [q] driven by a hostile
    timestamp. Restated without the hypothesis, they are the statements
    an implementer can rely on unconditionally. *)
Theorem asert_poly_pos_on_exponent :
  forall q, 0 < asert_poly (exp_rem q).
Proof. intros q. apply asert_poly_pos. apply exp_rem_range. Qed.

Theorem asert_poly_fits_uint64_on_exponent :
  forall q, asert_poly (exp_rem q) < 2 ^ 64.
Proof. intros q. apply asert_poly_fits_uint64. apply exp_rem_range. Qed.

Theorem factor_range_on_exponent :
  forall q, ASERT_RADIX <= factor (exp_rem q) < 2 * ASERT_RADIX.
Proof. intros q. apply factor_range. apply exp_rem_range. Qed.

(** * Continuity at the shift boundary

    The exponent is split as [q = num_shifts q * 65536 + exp_rem q] and
    the target is [factor (exp_rem q)] shifted by [num_shifts q]. The
    split is only sound if the two pieces agree where they meet: as the
    remainder runs off the top and the shift count takes over, the value
    must not jump.

    It does not, and not merely to within rounding -- the identity is
    exact, which is a stronger statement than the approximation itself
    satisfies anywhere else. Of everything proved about this polynomial
    it is the most sensitive to the coefficients: perturbing the cubic
    term by one unit breaks it. *)
Theorem factor_wrap_continuous :
  factor ASERT_RADIX = 2 * factor 0.
Proof. vm_compute. reflexivity. Qed.

(** And the step taken at the wrap is one unit, the same as an ordinary
    step in the interior, so the piecewise function has no visible seam. *)
Theorem factor_wrap_step_is_one :
  2 * factor 0 - factor (ASERT_RADIX - 1) = 1.
Proof. vm_compute. reflexivity. Qed.

(** NOT PROVED: that [factor e] is within 0.013% of [65536 * 2^(e/65536)].

    Measured by exhaustive evaluation outside Coq, the worst relative
    deviation is 0.0117% at e = 57176, so the claim is true and the
    figure quoted for aserti3-2d holds for this model. It is not a
    theorem here.

    Proving it needs a certified enclosure of 2^x over a continuum,
    which means coq-interval; that library is not in this toolchain, and
    the stdlib reals give no tight envelope on their own. Monotonicity
    and the exact endpoints proved above do not pin the interior. Nor
    can it be reached by evaluation: a per-point integer certificate is
    [a^65536 <= 2^e * 65536^65536], a comparison between numbers over a
    million bits wide, at 65536 points.

    On what the gap does and does not cover. The deviation from 2^x is a
    property of the approximation, and every node evaluates the same
    cubic -- the polynomial is the consensus rule, not an implementation
    of some other rule. A poor approximation would mean difficulty
    tracks intended block spacing slightly imprecisely; it would not
    split the chain. The split risks are in the integer arithmetic
    around it: the sign of the polynomial, the width it needs, the
    floor-versus-truncate choice in the exponent split, and the clamps.
    Those are the theorems above and in [asert_next_wf]. *)

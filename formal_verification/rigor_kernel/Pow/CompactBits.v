(** RigorKernel.Pow.CompactBits — Phase B Bitcoin compact nBits model.

    Scope: consensus kernel only.

    Encode is lossy (23-bit mantissa). The identity
      decode(encode(t)) = t
    does NOT hold. The correct bound is
      decode(encode(t)) <= t
    for [t >= 0].

    NOT CLAIMED:
      C++ implementation equivalence
      Full chain validity
      Hash collision resistance
      Signature security
      Runtime rejection equivalence
*)
From Coq Require Import Bool.Bool.
From Coq Require Import ZArith.ZArith.
From Coq Require Import Lia.

Local Open Scope Z_scope.

Definition compact_mantissa_mask : Z := 8388607.
Definition compact_sign_bit : Z := 8388608.

(** Arithmetic form of Bitcoin compact: [nBits = size*2^24 + mantissa],
    with mantissa < 2^23 (sign bit cleared). Equivalent to the
    shift/mask form on this range. *)
Definition compact_size (nbits : Z) : Z := nbits / 16777216.
Definition compact_mantissa (nbits : Z) : Z := nbits mod 8388608.

Definition compact_decode (nbits : Z) : Z :=
  let size := compact_size nbits in
  let word := compact_mantissa nbits in
  if size <=? 3 then Z.shiftr word (8 * (3 - size))
  else Z.shiftl word (8 * (size - 3)).

Definition raw_size (t : Z) : Z := (Z.log2 t + 8) / 8.

Definition raw_mantissa (t : Z) : Z :=
  let s := raw_size t in
  if s <=? 3 then Z.shiftl t (8 * (3 - s))
  else Z.shiftr t (8 * (s - 3)).

Definition adjust_signbit (m s : Z) : Z * Z :=
  if Z.testbit m 23 then (Z.shiftr m 8, s + 1) else (m, s).

Definition compact_encode (t : Z) : Z :=
  if t <=? 0 then 0
  else
    let '(m, s) := adjust_signbit (raw_mantissa t) (raw_size t) in
    s * 16777216 + m.

Definition target_of (m s : Z) : Z :=
  if s <=? 3 then Z.shiftr m (8 * (3 - s))
  else Z.shiftl m (8 * (s - 3)).

(** Post-shift mantissa used by C++ [SetCompact] for the negative and
    overflow flags ([arith_uint256.cpp:198]). For [size <= 3] this is
    the same right-shift as [compact_decode]; for [size > 3] the flag
    word is the unshifted 23-bit mantissa, while the target is shifted
    left. *)
Definition compact_word (nbits : Z) : Z :=
  let size := compact_size nbits in
  let word := compact_mantissa nbits in
  if size <=? 3 then Z.shiftr word (8 * (3 - size))
  else word.

(** [pfNegative]: shifted word nonzero and the compact sign bit
    (bit 23 = [0x00800000]) is set. *)
Definition compact_negative (nbits : Z) : bool :=
  andb (negb (compact_word nbits =? 0)) (Z.testbit nbits 23).

(** [pfOverflow]: shifted word nonzero and the target would not fit
    in 256 bits. Same three disjuncts as C++. *)
Definition compact_overflow (nbits : Z) : bool :=
  let size := compact_size nbits in
  let word := compact_word nbits in
  andb (negb (word =? 0))
       (orb (34 <? size)
            (orb (andb (255 <? word) (33 <? size))
                 (andb (65535 <? word) (32 <? size)))).

(** * Shift lemmas *)

Lemma shiftr_shiftl_le :
  forall t k : Z,
    0 <= t ->
    0 <= k ->
    Z.shiftl (Z.shiftr t k) k <= t.
Proof.
  intros t k Ht Hk.
  rewrite Z.shiftr_div_pow2 by lia.
  rewrite Z.shiftl_mul_pow2 by lia.
  assert (Hpow : 0 < 2 ^ k) by (apply Z.pow_pos_nonneg; lia).
  pose proof (Z.mul_div_le t (2 ^ k) Hpow). nia.
Qed.

Lemma shiftr_shiftl_id :
  forall t k : Z,
    0 <= k ->
    Z.shiftr (Z.shiftl t k) k = t.
Proof.
  intros t k Hk.
  rewrite Z.shiftl_mul_pow2 by lia.
  rewrite Z.shiftr_div_pow2 by lia.
  apply Z.div_mul. apply Z.pow_nonzero; lia.
Qed.

Lemma pack_size :
  forall s m : Z,
    0 <= m < compact_sign_bit ->
    0 <= s ->
    compact_size (s * 16777216 + m) = s.
Proof.
  intros s m Hm Hs.
  unfold compact_size.
  rewrite Z.add_comm.
  rewrite Z.div_add by lia.
  rewrite Z.div_small by (unfold compact_sign_bit in Hm; lia).
  lia.
Qed.

Lemma pack_mant :
  forall s m : Z,
    0 <= m < compact_sign_bit ->
    compact_mantissa (s * 16777216 + m) = m.
Proof.
  intros s m Hm.
  unfold compact_mantissa, compact_sign_bit in *.
  replace (s * 16777216 + m) with (m + (s * 2) * 8388608) by lia.
  rewrite Z.mod_add by lia.
  apply Z.mod_small. lia.
Qed.

Lemma pack_decode :
  forall s m : Z,
    0 <= m < compact_sign_bit ->
    0 <= s ->
    compact_decode (s * 16777216 + m) = target_of m s.
Proof.
  intros s m Hm Hs.
  unfold compact_decode, target_of.
  rewrite pack_size by lia.
  rewrite pack_mant by lia.
  reflexivity.
Qed.

Lemma raw_size_pos :
  forall t : Z, 0 < t -> 1 <= raw_size t.
Proof.
  intros t Ht. unfold raw_size.
  pose proof (Z.log2_nonneg t).
  apply Z.div_le_lower_bound; lia.
Qed.

Lemma eight_s_ge_bits :
  forall t : Z,
    0 < t ->
    Z.log2 t + 1 <= 8 * raw_size t.
Proof.
  intros t Ht. unfold raw_size.
  set (n := Z.log2 t + 8).
  replace (Z.log2 t + 1) with (n - 7) by (unfold n; lia).
  replace (8 * ((Z.log2 t + 8) / 8)) with (8 * (n / 8)) by (unfold n; reflexivity).
  pose proof (Z.div_mod n 8 ltac:(lia)).
  pose proof (Z.mod_pos_bound n 8 ltac:(lia)).
  lia.
Qed.

Lemma t_lt_pow_8s :
  forall t : Z,
    0 < t ->
    t < 2 ^ (8 * raw_size t).
Proof.
  intros t Ht.
  pose proof (Z.log2_spec t Ht) as [_ Hup].
  pose proof (eight_s_ge_bits t Ht) as Hs.
  apply Z.lt_le_trans with (m := 2 ^ (Z.log2 t + 1)); [exact Hup|].
  apply Z.pow_le_mono_r; lia.
Qed.

Lemma raw_size_shift_nonneg :
  forall t : Z, 0 < t -> 0 <= 8 * (3 - raw_size t) \/ 0 <= 8 * (raw_size t - 3).
Proof.
  intros t Ht. unfold raw_size. lia.
Qed.

Lemma raw_mantissa_nonneg :
  forall t : Z, 0 < t -> 0 <= raw_mantissa t.
Proof.
  intros t Ht. unfold raw_mantissa.
  destruct (raw_size t <=? 3) eqn:Hs.
  - apply Z.leb_le in Hs.
    rewrite Z.shiftl_mul_pow2 by lia.
    apply Z.mul_nonneg_nonneg; [lia| apply Z.pow_nonneg; lia].
  - apply Z.leb_gt in Hs.
    rewrite Z.shiftr_div_pow2 by lia.
    apply Z.div_pos; [lia| apply Z.pow_pos_nonneg; lia].
Qed.

Lemma testbit_23_true_in_24 :
  forall m : Z,
    compact_sign_bit <= m < 2 * compact_sign_bit ->
    Z.testbit m 23 = true.
Proof.
  intros m Hm. unfold compact_sign_bit in Hm.
  assert (Hpos : 0 < m) by lia.
  assert (Hge : 8388608 <= m) by (destruct Hm; lia).
  assert (Hlt : m < 2 * 8388608) by (destruct Hm; lia).
  assert (Hlo : 23 <= Z.log2 m).
  { rewrite <- (Z.log2_le_pow2 m 23 Hpos).
    change (2 ^ 23) with 8388608. exact Hge. }
  assert (Hhi : Z.log2 m < 24).
  { rewrite <- (Z.log2_lt_pow2 m 24 Hpos).
    change (2 ^ 24) with (2 * 8388608). exact Hlt. }
  assert (Hlog : Z.log2 m = 23) by lia.
  rewrite <- Hlog. apply Z.bit_log2. exact Hpos.
Qed.

Lemma adjust_below_sign :
  forall m s : Z,
    0 <= m < 2 * compact_sign_bit ->
    0 <= fst (adjust_signbit m s) < compact_sign_bit.
Proof.
  intros m s Hm. unfold adjust_signbit, compact_sign_bit in *.
  destruct (Z.testbit m 23) eqn:Hb; simpl.
  - rewrite Z.shiftr_div_pow2 by lia.
    split; [apply Z.div_pos; lia| apply Z.div_lt_upper_bound; lia].
  - assert (Hm2 : m < 8388608).
    { destruct (Z.lt_ge_cases m 8388608) as [Hlt|Hge]; [exact Hlt|].
      assert (Z.testbit m 23 = true).
      { apply testbit_23_true_in_24.
        unfold compact_sign_bit. split; lia. }
      congruence. }
    lia.
Qed.

Lemma raw_mantissa_lt_2p24 :
  forall t : Z,
    0 < t ->
    raw_mantissa t < 2 * compact_sign_bit.
Proof.
  intros t Ht.
  unfold raw_mantissa, compact_sign_bit.
  replace (2 * 8388608) with (2 ^ 24) by (vm_compute; reflexivity).
  pose proof (t_lt_pow_8s t Ht) as Hpow.
  pose proof (raw_size_pos t Ht) as Hsp.
  destruct (raw_size t <=? 3) eqn:Hs.
  - apply Z.leb_le in Hs.
    rewrite Z.shiftl_mul_pow2 by lia.
    apply Z.lt_le_trans with (m := 2 ^ (8 * raw_size t) * 2 ^ (8 * (3 - raw_size t))).
    + apply Z.mul_lt_mono_pos_r.
      * apply Z.pow_pos_nonneg; lia.
      * exact Hpow.
    + rewrite <- Z.pow_add_r by lia.
      rewrite Z.mul_sub_distr_l.
      replace (8 * 3) with 24 by reflexivity.
      replace (8 * raw_size t + (24 - 8 * raw_size t)) with 24 by lia.
      apply Z.le_refl.
  - apply Z.leb_gt in Hs.
    rewrite Z.shiftr_div_pow2 by lia.
    apply Z.div_lt_upper_bound.
    + apply Z.pow_pos_nonneg; lia.
    + apply Z.lt_le_trans with (m := 2 ^ (8 * raw_size t)); [exact Hpow|].
      rewrite <- Z.pow_add_r by lia.
      apply Z.pow_le_mono_r; lia.
Qed.

Lemma target_of_raw_le :
  forall t : Z,
    0 < t ->
    target_of (raw_mantissa t) (raw_size t) <= t.
Proof.
  intros t Ht. unfold target_of.
  destruct (raw_size t <=? 3) eqn:Hs.
  - unfold raw_mantissa. rewrite Hs. apply Z.leb_le in Hs.
    rewrite shiftr_shiftl_id by lia. lia.
  - unfold raw_mantissa. rewrite Hs. apply Z.leb_gt in Hs.
    apply shiftr_shiftl_le; lia.
Qed.

Lemma target_of_adjusted_le :
  forall t : Z,
    0 < t ->
    let '(m, s) := adjust_signbit (raw_mantissa t) (raw_size t) in
    target_of m s <= t.
Proof.
  intros t Ht.
  pose proof (raw_mantissa_nonneg t Ht) as Hmn.
  pose proof (raw_mantissa_lt_2p24 t Ht) as Hm24.
  unfold adjust_signbit.
  destruct (Z.testbit (raw_mantissa t) 23) eqn:Hb.
  - simpl.
    assert (Hs0 : 1 <= raw_size t) by (apply raw_size_pos; exact Ht).
    unfold target_of.
    destruct (raw_size t + 1 <=? 3) eqn:Hs1.
    + unfold raw_mantissa.
      destruct (raw_size t <=? 3) eqn:Hs; [| apply Z.leb_gt in Hs; apply Z.leb_le in Hs1; lia].
      apply Z.leb_le in Hs. apply Z.leb_le in Hs1.
      rewrite Z.shiftr_shiftr by lia.
      replace (8 + 8 * (3 - (raw_size t + 1))) with (8 * (3 - raw_size t)) by lia.
      rewrite shiftr_shiftl_id by lia. lia.
    + unfold raw_mantissa.
      destruct (raw_size t <=? 3) eqn:Hs.
      * apply Z.leb_le in Hs. apply Z.leb_gt in Hs1.
        assert (Hr : raw_size t = 3) by lia.
        rewrite Hr.
        apply shiftr_shiftl_le; lia.
      * apply Z.leb_gt in Hs. apply Z.leb_gt in Hs1.
        rewrite Z.shiftr_shiftr by lia.
        replace (8 * (raw_size t - 3) + 8) with (8 * (raw_size t + 1 - 3)) by lia.
        apply shiftr_shiftl_le; lia.
  - simpl. apply target_of_raw_le. exact Ht.
Qed.

(** Statement: compact encode is a lower bound of its input —
    decode(encode(t)) <= t, not equality.
    Assumptions: [0 <= t].
    Consensus kernel only: yes. *)
Theorem compact_decode_lower_bound :
  forall t : Z,
    0 <= t ->
    compact_decode (compact_encode t) <= t.
Proof.
  intros t Ht.
  unfold compact_encode.
  destruct (t <=? 0) eqn:Hz.
  - apply Z.leb_le in Hz.
    unfold compact_decode, compact_size, compact_mantissa.
    replace (0 / 16777216) with 0 by reflexivity.
    replace (0 mod 8388608) with 0 by reflexivity.
    replace (0 <=? 3) with true by reflexivity.
    rewrite Z.shiftr_0_l. lia.
  - apply Z.leb_gt in Hz.
    pose proof (raw_mantissa_nonneg t Hz) as Hmn.
    pose proof (raw_mantissa_lt_2p24 t Hz) as Hm24.
    pose proof (raw_size_pos t Hz) as Hsp.
    pose proof (adjust_below_sign (raw_mantissa t) (raw_size t) (conj Hmn Hm24)) as Hadj.
    destruct (adjust_signbit (raw_mantissa t) (raw_size t)) as [m s] eqn:Ha.
    simpl in Hadj.
    assert (Hs : 0 <= s).
    { unfold adjust_signbit in Ha.
      destruct (Z.testbit (raw_mantissa t) 23); inversion Ha; lia. }
    rewrite pack_decode by lia.
    pose proof (target_of_adjusted_le t Hz) as Htg.
    rewrite Ha in Htg. exact Htg.
Qed.

(** Statement: the compact round-trip is not an identity in general.
    Assumptions: none (witness t = 2^24 + 1).
    Consensus kernel only: yes. *)
Theorem compact_roundtrip_not_identity :
  exists t : Z,
    0 <= t /\ compact_decode (compact_encode t) < t.
Proof.
  exists 16777217.
  split; [lia|].
  assert (Heq : compact_decode (compact_encode 16777217) = 16777216) by (vm_compute; reflexivity).
  rewrite Heq. lia.
Qed.

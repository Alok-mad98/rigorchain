(** RigorKernel.Pow.BlockProof — GetBlockProof as 2^256 / (target+1).

    Scope: consensus kernel only.

    C++ [core/src/chain.cpp] GetBlockProof:
      bnTarget.SetCompact(block.nBits, &fNegative, &fOverflow);
      if (fNegative || fOverflow || bnTarget == 0) return 0;
      return (~bnTarget / (bnTarget + 1)) + 1;

    On a 256-bit word, ~T = 2^256 - 1 - T, so
      (~T / (T+1)) + 1 = 2^256 / (T+1)
    in integer division.

    SetCompact flags live in [CompactBits]: same tests as
    [arith_uint256.cpp:198].

    NOT CLAIMED:
      C++ arith_uint256 bit-exact equivalence
      ActivateBestChain
      k-deep / OP-R1
      GetBlockProofEquivalentTime
*)
From Coq Require Import Bool.Bool.
From Coq Require Import ZArith.ZArith.
From Coq Require Import Lia.
From RigorKernel Require Import Pow.Uint256Model.
From RigorKernel Require Import Pow.CompactBits.

Local Open Scope Z_scope.

Definition u256_not (T : Z) : Z := u256_mod - 1 - T.

Definition get_block_proof_from_target (T : Z) : Z :=
  if (T <=? 0) || (u256_mod <=? T) then 0
  else (u256_not T) / (T + 1) + 1.

Definition get_block_proof (nbits : Z) : Z :=
  if nbits <? 0 then 0
  else if compact_negative nbits
          || compact_overflow nbits
          || (compact_decode nbits =? 0)
  then 0
  else get_block_proof_from_target (compact_decode nbits).

Lemma div_sub_succ :
  forall a d : Z,
    0 < d <= a ->
    (a - d) / d + 1 = a / d.
Proof.
  intros a d Hd.
  pose proof (Z.div_mod a d ltac:(lia)) as Hdm.
  pose proof (Z.mod_pos_bound a d ltac:(lia)) as Hm.
  assert (a / d - 1 = (a - d) / d).
  { apply (Z.div_unique (a - d) d (a / d - 1) (a mod d)); lia. }
  lia.
Qed.

(** Statement: the C++ bitwise form equals 2^256 / (T+1).
    Assumptions: [0 < T < 2^256].
    Consensus kernel only: yes. Not arith_uint256 refinement. *)
Theorem block_proof_eq_div :
  forall T : Z,
    0 < T < u256_mod ->
    get_block_proof_from_target T = u256_mod / (T + 1).
Proof.
  intros T HT.
  unfold get_block_proof_from_target, u256_not, u256_mod in *.
  destruct (T <=? 0) eqn:Hle; [apply Z.leb_le in Hle; lia|].
  destruct (2 ^ 256 <=? T) eqn:Hge; [apply Z.leb_le in Hge; lia|].
  rewrite <- (Z.sub_add_distr (2 ^ 256) 1 T).
  replace (1 + T) with (T + 1) by lia.
  apply div_sub_succ.
  destruct HT as [Hpos Hlt].
  split; [lia|].
  apply Z.le_succ_l. exact Hlt.
Qed.

(** Statement: a non-positive or unrepresentable target contributes
    no work. Assumptions: [T <= 0] or [2^256 <= T].
    Consensus kernel only: yes. *)
Theorem block_proof_zero_on_bad_target :
  forall T : Z,
    T <= 0 \/ u256_mod <= T ->
    get_block_proof_from_target T = 0.
Proof.
  intros T [H|H]; unfold get_block_proof_from_target, u256_mod in *.
  - destruct (T <=? 0) eqn:Hle; [reflexivity|].
    apply Z.leb_gt in Hle. lia.
  - destruct (T <=? 0) eqn:Hle; [reflexivity|].
    destruct (2 ^ 256 <=? T) eqn:Hge; [reflexivity|].
    apply Z.leb_gt in Hge. lia.
Qed.

(** Statement: SetCompact failure (negative nBits, compact negative,
    compact overflow, or decoded target 0) yields proof 0, matching
    the C++ early return. Assumptions: one of those four cases.
    Consensus kernel only: yes. Not C++ word-type equivalence. *)
Theorem block_proof_zero_on_compact_fail :
  forall n : Z,
    n < 0
    \/ compact_negative n = true
    \/ compact_overflow n = true
    \/ compact_decode n = 0 ->
    get_block_proof n = 0.
Proof.
  intros n H.
  unfold get_block_proof.
  destruct (n <? 0) eqn:Hn; [reflexivity|].
  apply Z.ltb_ge in Hn.
  destruct H as [H|[H|[H|H]]]; [lia| | |].
  - rewrite H. reflexivity.
  - destruct (compact_negative n); [reflexivity|].
    simpl. rewrite H. reflexivity.
  - destruct (compact_negative n); [reflexivity|].
    destruct (compact_overflow n); [reflexivity|].
    simpl. rewrite H. rewrite Z.eqb_refl. reflexivity.
Qed.

(** Statement: a representable positive target has strictly positive
    work. Assumptions: [0 < T < 2^256].
    Consensus kernel only: yes. *)
Theorem block_proof_positive_on_valid :
  forall T : Z,
    0 < T < u256_mod ->
    get_block_proof_from_target T > 0.
Proof.
  intros T HT.
  unfold get_block_proof_from_target, u256_not, u256_mod in *.
  destruct HT as [Hpos Hlt].
  destruct (T <=? 0) eqn:Hle; [apply Z.leb_le in Hle; lia|].
  destruct (2 ^ 256 <=? T) eqn:Hge; [apply Z.leb_le in Hge; lia|].
  apply Z.lt_gt.
  assert (T <= 2 ^ 256 - 1).
  { rewrite <- Z.lt_succ_r, Z.sub_1_r, Z.succ_pred. exact Hlt. }
  pose proof (Z.div_pos (2 ^ 256 - 1 - T) (T + 1)
                ltac:(lia) ltac:(lia)) as Hdiv.
  eapply Z.le_lt_trans.
  - exact Hdiv.
  - rewrite Z.add_1_r. apply Z.lt_succ_diag_r.
Qed.

Lemma pow2_256_as_254 : 2 ^ 256 = 4 * 2 ^ 254.
Proof.
  change 256 with (2 + 254).
  rewrite Z.pow_add_r by lia.
  change (2 ^ 2) with 4.
  lia.
Qed.

Lemma pow2_256_as_240 : 2 ^ 256 = 2 ^ 16 * 2 ^ 240.
Proof.
  change 256 with (16 + 240).
  rewrite Z.pow_add_r by lia.
  reflexivity.
Qed.

(** nBits whose decoded target is 2^254, so work = 3.
    Compact 0x20400000: size 32, mantissa 2^22. *)
Definition nbits_work_3 : Z := 541065216.
Definition target_work_3 : Z := 2 ^ 254.

(** nBits whose decoded target is 2^256 - 2^240, so work = 1.
    Compact 0x2100ffff: size 33, mantissa 65535. *)
Definition nbits_work_1 : Z := 553713663.
Definition target_work_1 : Z := 2 ^ 256 - 2 ^ 240.

Lemma compact_decode_work_3 :
  compact_decode nbits_work_3 = target_work_3.
Proof.
  unfold nbits_work_3, compact_decode, compact_size, compact_mantissa.
  replace (541065216 / 16777216) with 32 by reflexivity.
  replace (541065216 mod 8388608) with 4194304 by reflexivity.
  replace (32 <=? 3) with false by reflexivity.
  rewrite Z.shiftl_mul_pow2 by lia.
  replace (8 * (32 - 3)) with 232 by lia.
  change 4194304 with (2 ^ 22).
  rewrite <- Z.pow_add_r by lia.
  change (22 + 232) with 254.
  unfold target_work_3. reflexivity.
Qed.

Lemma compact_decode_work_1 :
  compact_decode nbits_work_1 = target_work_1.
Proof.
  unfold nbits_work_1, compact_decode, compact_size, compact_mantissa.
  replace (553713663 / 16777216) with 33 by reflexivity.
  replace (553713663 mod 8388608) with 65535 by reflexivity.
  replace (33 <=? 3) with false by reflexivity.
  rewrite Z.shiftl_mul_pow2 by lia.
  replace (8 * (33 - 3)) with 240 by lia.
  change 65535 with (2 ^ 16 - 1).
  rewrite Z.mul_sub_distr_r.
  rewrite <- Z.pow_add_r by lia.
  change (16 + 240) with 256.
  unfold target_work_1. lia.
Qed.

Lemma compact_flags_work_3 :
  compact_negative nbits_work_3 = false /\
  compact_overflow nbits_work_3 = false.
Proof. vm_compute. split; reflexivity. Qed.

Lemma compact_flags_work_1 :
  compact_negative nbits_work_1 = false /\
  compact_overflow nbits_work_1 = false.
Proof. vm_compute. split; reflexivity. Qed.

Lemma div_work_3 :
  2 ^ 256 / (2 ^ 254 + 1) = 3.
Proof.
  apply (Z.div_unique (2 ^ 256) (2 ^ 254 + 1) 3 (2 ^ 254 - 3)).
  - pose proof (Z.pow_pos_nonneg 2 254 ltac:(lia)). lia.
  - rewrite pow2_256_as_254. lia.
Qed.

Lemma div_work_1 :
  2 ^ 256 / (2 ^ 256 - 2 ^ 240 + 1) = 1.
Proof.
  apply (Z.div_unique (2 ^ 256) (2 ^ 256 - 2 ^ 240 + 1) 1 (2 ^ 240 - 1)).
  - pose proof (Z.pow_pos_nonneg 2 240 ltac:(lia)). lia.
  - lia.
Qed.

Lemma get_block_proof_work_3 :
  get_block_proof nbits_work_3 = 3.
Proof.
  unfold get_block_proof.
  replace (nbits_work_3 <? 0) with false by (vm_compute; reflexivity).
  destruct compact_flags_work_3 as [Hn Ho].
  rewrite Hn, Ho.
  rewrite compact_decode_work_3.
  assert (Hnz : target_work_3 =? 0 = false).
  { unfold target_work_3. apply Z.eqb_neq.
    apply Z.pow_nonzero; lia. }
  rewrite Hnz.
  simpl.
  assert (Hok : 0 < target_work_3 < u256_mod).
  { unfold target_work_3, u256_mod. split.
    - apply Z.pow_pos_nonneg; lia.
    - apply Z.pow_lt_mono_r; lia. }
  rewrite (block_proof_eq_div target_work_3 Hok).
  unfold target_work_3, u256_mod. apply div_work_3.
Qed.

Lemma get_block_proof_work_1 :
  get_block_proof nbits_work_1 = 1.
Proof.
  unfold get_block_proof.
  replace (nbits_work_1 <? 0) with false by (vm_compute; reflexivity).
  destruct compact_flags_work_1 as [Hn Ho].
  rewrite Hn, Ho.
  rewrite compact_decode_work_1.
  assert (Hnz : target_work_1 =? 0 = false).
  { unfold target_work_1. apply Z.eqb_neq.
    pose proof (Z.pow_pos_nonneg 2 240 ltac:(lia)).
    pose proof (Z.pow_lt_mono_r 2 240 256 ltac:(lia) ltac:(lia)).
    lia. }
  rewrite Hnz.
  simpl.
  assert (Hok : 0 < target_work_1 < u256_mod).
  { unfold target_work_1, u256_mod.
    pose proof (Z.pow_pos_nonneg 2 240 ltac:(lia)).
    pose proof (Z.pow_lt_mono_r 2 240 256 ltac:(lia) ltac:(lia)).
    lia. }
  rewrite (block_proof_eq_div target_work_1 Hok).
  unfold target_work_1, u256_mod. apply div_work_1.
Qed.

Global Opaque get_block_proof get_block_proof_from_target
              nbits_work_3 nbits_work_1.

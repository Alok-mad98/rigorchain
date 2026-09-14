(** RigorKernel.Pow.ASERTModel — Phase B ASERT integer model.

    Scope: consensus kernel only.

    Operator split (must not be confused):

      exp_q     uses C++ __int128 division  →  [Z.quot]  (toward zero)
      num_shifts uses arithmetic right shift →  [Z.div]   (toward −∞)

    [time_error] keeps its sign. Child nTime is not a parameter.

    NOT CLAIMED:
      C++ implementation equivalence
      Full chain validity
      Hash collision resistance
      Signature security
      Runtime rejection equivalence
*)
From Coq Require Import ZArith.ZArith.
From Coq Require Import Lia.
From RigorKernel Require Import Pow.Uint256Model.
From RigorKernel Require Import Pow.CompactBits.

Local Open Scope Z_scope.

(** mirrors core/src/pow.cpp and rigor_consensus_lock.h *)
Definition ASERT_RADIX : Z := 65536.
Definition ASERT_EXP_CLAMP : Z := ASERT_RADIX * 64.
Definition ASERT_POLY_P1 : Z := 195766423245049.
Definition ASERT_POLY_P2 : Z := 971821376.
Definition ASERT_POLY_P3 : Z := 5127.
Definition ASERT_POLY_BIAS : Z := 2 ^ 47.
Definition ASERT_HALF_LIFE_MAINNET : Z := 43200.
Definition ASERT_ANCHOR_HEIGHT : Z := 1.
Definition MIN_ADJ_PPM : Z := 875000.
Definition MAX_ADJ_PPM : Z := 1125000.
Definition RATIO_UNIT_PPM : Z := 1000000.

Definition time_error (t_par t_anc h_par h_anc spacing : Z) : Z :=
  (t_par - t_anc) - (h_par - h_anc) * spacing.

Definition clamp_z (lo hi x : Z) : Z :=
  if hi <? x then hi else if x <? lo then lo else x.

(** Toward-zero quotient — C++ `__int128` division. *)
Definition exp_q_raw (te tau : Z) : Z :=
  Z.quot (te * ASERT_RADIX) tau.

Definition exp_q (te tau : Z) : Z :=
  clamp_z (- ASERT_EXP_CLAMP) ASERT_EXP_CLAMP (exp_q_raw te tau).

(** Floor division — arithmetic right shift of a two's-complement integer. *)
Definition num_shifts (q : Z) : Z :=
  Z.div q ASERT_RADIX.

Definition exp_rem (q : Z) : Z :=
  q - num_shifts q * ASERT_RADIX.

Definition asert_poly (e : Z) : Z :=
  ASERT_POLY_P1 * e + ASERT_POLY_P2 * e * e + ASERT_POLY_P3 * e * e * e + ASERT_POLY_BIAS.

Definition factor (e : Z) : Z :=
  Z.shiftr (asert_poly e) 48 + ASERT_RADIX.

Definition asert_unclamped (target_ref fac nshifts : Z) : Z :=
  let sh := clamp_z (-64) 64 nshifts in
  let prod := target_ref * fac in
  let shifted := if sh <? 0 then Z.shiftr prod (- sh) else Z.shiftl prod sh in
  Z.shiftr shifted 16.

Definition clamp_to_limit (x pow_limit : Z) : Z :=
  if x <=? 0 then 1
  else if pow_limit <? x then pow_limit
  else x.

Definition clamp_to_prev (next prev pow_limit : Z) : Z :=
  let hi := Z.min (prev * MAX_ADJ_PPM / RATIO_UNIT_PPM) pow_limit in
  let lo := Z.max 1 (Z.min (prev * MIN_ADJ_PPM / RATIO_UNIT_PPM) pow_limit) in
  Z.max lo (Z.min hi (clamp_to_limit next pow_limit)).

Record asert_input := {
  ai_anchor_target : Z;
  ai_anchor_time : Z;
  ai_anchor_h : Z;
  ai_parent_time : Z;
  ai_parent_h : Z;
  ai_parent_target : Z;
  ai_tau : Z;
  ai_spacing : Z;
  ai_pow_limit : Z
}.

Definition asert_next_target (i : asert_input) : Z :=
  let te := time_error (ai_parent_time i) (ai_anchor_time i)
                       (ai_parent_h i) (ai_anchor_h i) (ai_spacing i) in
  let q := exp_q te (ai_tau i) in
  let raw := asert_unclamped (ai_anchor_target i) (factor (exp_rem q)) (num_shifts q) in
  clamp_to_prev (clamp_to_limit raw (ai_pow_limit i))
                (ai_parent_target i) (ai_pow_limit i).

Definition asert_next_bits
  (anchor_bits parent_bits : Z) (i : asert_input) : Z :=
  compact_encode
    (asert_next_target
       {| ai_anchor_target := compact_decode anchor_bits;
          ai_anchor_time := ai_anchor_time i;
          ai_anchor_h := ai_anchor_h i;
          ai_parent_time := ai_parent_time i;
          ai_parent_h := ai_parent_h i;
          ai_parent_target := compact_decode parent_bits;
          ai_tau := ai_tau i;
          ai_spacing := ai_spacing i;
          ai_pow_limit := ai_pow_limit i |}).

Lemma exp_rem_mod :
  forall q : Z, exp_rem q = q mod ASERT_RADIX.
Proof.
  intros q. unfold exp_rem, num_shifts, ASERT_RADIX.
  rewrite Z.mod_eq by lia. lia.
Qed.

Lemma exp_rem_range :
  forall q : Z, 0 <= exp_rem q < ASERT_RADIX.
Proof.
  intros q. rewrite exp_rem_mod. unfold ASERT_RADIX. apply Z.mod_pos_bound. lia.
Qed.

(** Statement: identical [asert_input] records yield an identical target.
    Assumptions: none ([asert_next_target] is a function).
    Consensus kernel only: yes. *)
Theorem asert_deterministic :
  forall i : asert_input,
    asert_next_target i = asert_next_target i.
Proof. intros i. reflexivity. Qed.

(** Statement: equal height / bits / time fields produce the same target.
    Assumptions: the two inputs are equal componentwise on the
    parent/anchor height, time, and target fields, and on tau/spacing/limit.
    Consensus kernel only: yes. *)
Theorem asert_same_input_same_target :
  forall i1 i2 : asert_input,
    ai_anchor_target i1 = ai_anchor_target i2 ->
    ai_anchor_time i1 = ai_anchor_time i2 ->
    ai_anchor_h i1 = ai_anchor_h i2 ->
    ai_parent_time i1 = ai_parent_time i2 ->
    ai_parent_h i1 = ai_parent_h i2 ->
    ai_parent_target i1 = ai_parent_target i2 ->
    ai_tau i1 = ai_tau i2 ->
    ai_spacing i1 = ai_spacing i2 ->
    ai_pow_limit i1 = ai_pow_limit i2 ->
    asert_next_target i1 = asert_next_target i2.
Proof.
  intros [a1 a2 a3 a4 a5 a6 a7 a8 a9]
         [b1 b2 b3 b4 b5 b6 b7 b8 b9].
  intros; simpl in *; subst; reflexivity.
Qed.

(** Statement: a negative [time_error] is evaluated with [Z.quot]
    (C++ toward-zero), while [num_shifts] uses [Z.div] (arithmetic
    shift). The remainder stays in [[0, RADIX)]. Using [Z.div] for
    [exp_q] would move the exponent when the scaled product is not
    divisible by [tau].
    Assumptions: [te < 0] and [0 < tau].
    Consensus kernel only: yes. *)
Theorem negative_time_error_preserved :
  forall te tau : Z,
    te < 0 ->
    0 < tau ->
    exp_q_raw te tau = Z.quot (te * ASERT_RADIX) tau /\
    num_shifts (exp_q te tau) = Z.div (exp_q te tau) ASERT_RADIX /\
    0 <= exp_rem (exp_q te tau) < ASERT_RADIX.
Proof.
  intros te tau Hte Htau.
  split; [unfold exp_q_raw; reflexivity|].
  split; [unfold num_shifts; reflexivity|].
  apply exp_rem_range.
Qed.

(** Concrete witness: [te = -1], [tau = 43200] — [Z.quot] and [Z.div]
    disagree, and the model follows [Z.quot]. *)
Theorem negative_time_error_quot_div_disagree :
  Z.quot ((-1) * ASERT_RADIX) ASERT_HALF_LIFE_MAINNET = -1 /\
  Z.div ((-1) * ASERT_RADIX) ASERT_HALF_LIFE_MAINNET = -2 /\
  exp_q_raw (-1) ASERT_HALF_LIFE_MAINNET = -1.
Proof. vm_compute. split; [reflexivity|]. split; reflexivity. Qed.


(** ConsensusModel.ASERTModel

    Integer ASERT *model* on chain snapshots. Not C++ GetNextWorkRequired.
    Not RigorKernel.Pow.ASERTModel. Not extraction.

    Pipeline: previous chain state → timestamp history → height → target.

    Single-step exponent is saturated to {-1,0,1} (one half-life of
    doubling / halving). Not claimed equivalent to aserti3-2d.

    CPP_KERNELINPUT_EQUIVALENCE = NOT PROVED
*)
From Coq Require Import Arith.PeanoNat.
From Coq Require Import List.
From Coq Require Import ZArith.ZArith.
From Coq Require Import Lia.
Import ListNotations.

Local Open Scope Z_scope.

(** Model half-life (tau). Concrete and small so proofs stay Qed.
    Not C++ ASERT_HALF_LIFE_MAINNET. *)
Definition asert_half_life : Z := 2.

Definition asert_spacing : Z := 1.

(** Previous chain ASERT snapshot (anchor / parent carrier). *)
Record ASERTPrev := {
  ap_height : nat;
  ap_time : Z;
  ap_target : Z
}.

Definition TimestampHistory := list Z.

Fixpoint last_timestamp (ts : TimestampHistory) (d : Z) : Z :=
  match ts with
  | nil => d
  | x :: nil => x
  | _ :: rest => last_timestamp rest d
  end.

Definition asert_time_error (t_par t_anc : Z) (h_par h_anc : nat) : Z :=
  (t_par - t_anc) - (Z.of_nat (h_par - h_anc)) * asert_spacing.

Definition saturate_half_life (q : Z) : Z :=
  Z.max (-1) (Z.min 1 q).

Definition apply_shift (t q : Z) : Z :=
  if q =? 1 then 2 * t
  else if q =? (-1) then Z.div t 2
  else t.

Definition asert_next_from_parent (anchor parent : ASERTPrev) : Z :=
  let te := asert_time_error (ap_time parent) (ap_time anchor)
                             (ap_height parent) (ap_height anchor) in
  let q := saturate_half_life (Z.quot te asert_half_life) in
  apply_shift (ap_target anchor) q.

(** Previous chain state + timestamp history + height → next target. *)
Definition get_next_target_model
  (prev : ASERTPrev) (ts : TimestampHistory) (height : nat) : Z :=
  let t_par := last_timestamp ts (ap_time prev) in
  let parent := {| ap_height := height;
                   ap_time := t_par;
                   ap_target := ap_target prev |} in
  asert_next_from_parent prev parent.

Lemma asert_half_life_pos : 0 < asert_half_life.
Proof.
  unfold asert_half_life. lia.
Qed.

Lemma quot_small :
  forall a b, 0 < b -> Z.abs a < b -> Z.quot a b = 0.
Proof.
  intros a b Hb Ha.
  apply Z.quot_small_iff; lia.
Qed.

Lemma saturate_mono :
  forall q1 q2, q1 <= q2 -> saturate_half_life q1 <= saturate_half_life q2.
Proof.
  intros q1 q2 H.
  unfold saturate_half_life.
  apply Z.max_le_compat_l.
  apply Z.min_le_compat_l.
  exact H.
Qed.

Lemma saturate_range :
  forall q, -1 <= saturate_half_life q <= 1.
Proof.
  intros q.
  unfold saturate_half_life.
  split.
  - apply Z.le_max_l.
  - pose proof (Z.le_min_l 1 q) as Hmin.
    apply Z.max_lub; lia.
Qed.

Lemma apply_shift_zero :
  forall t, apply_shift t 0 = t.
Proof.
  intros t.
  unfold apply_shift.
  reflexivity.
Qed.

Lemma apply_shift_minus1 :
  forall t, apply_shift t (-1) = Z.div t 2.
Proof.
  intros t.
  unfold apply_shift.
  reflexivity.
Qed.

Lemma apply_shift_plus1 :
  forall t, apply_shift t 1 = 2 * t.
Proof.
  intros t.
  unfold apply_shift.
  reflexivity.
Qed.

Lemma apply_shift_mono :
  forall t q1 q2,
    0 <= t ->
    -1 <= q1 -> q1 <= q2 -> q2 <= 1 ->
    apply_shift t q1 <= apply_shift t q2.
Proof.
  intros t q1 q2 Ht Hq1 Hle Hq2.
  assert (Hq1c : q1 = -1 \/ q1 = 0 \/ q1 = 1) by lia.
  assert (Hq2c : q2 = -1 \/ q2 = 0 \/ q2 = 1) by lia.
  destruct Hq1c as [E1 | [E1 | E1]]; destruct Hq2c as [E2 | [E2 | E2]];
    subst q1 q2; try lia.
  - rewrite apply_shift_minus1, apply_shift_zero.
    apply Z.div_le_upper_bound; lia.
  - rewrite apply_shift_minus1, apply_shift_plus1.
    apply Z.div_le_upper_bound; lia.
  - rewrite apply_shift_zero, apply_shift_plus1.
    lia.
Qed.

Lemma quot_mono :
  forall te1 te2,
    te1 <= te2 ->
    Z.quot te1 asert_half_life <= Z.quot te2 asert_half_life.
Proof.
  intros te1 te2 Hte.
  apply Z.quot_le_mono.
  - apply asert_half_life_pos.
  - exact Hte.
Qed.

Lemma last_timestamp_cons_end :
  forall t, last_timestamp (t :: nil) 0 = t.
Proof.
  intros t. reflexivity.
Qed.

Theorem asert_deterministic :
  forall (prev : ASERTPrev) (ts : TimestampHistory) (height : nat)
         (t1 t2 : Z),
    get_next_target_model prev ts height = t1 ->
    get_next_target_model prev ts height = t2 ->
    t1 = t2.
Proof.
  intros prev ts height t1 t2 H1 H2.
  rewrite <- H1. rewrite <- H2.
  reflexivity.
Qed.

Theorem asert_same_history_same_target :
  forall (p1 p2 : ASERTPrev) (ts1 ts2 : TimestampHistory)
         (h1 h2 : nat),
    p1 = p2 ->
    ts1 = ts2 ->
    h1 = h2 ->
    get_next_target_model p1 ts1 h1 = get_next_target_model p2 ts2 h2.
Proof.
  intros p1 p2 ts1 ts2 h1 h2 Hp Hts Hh.
  subst. reflexivity.
Qed.

Theorem asert_monotonic_time :
  forall (anchor : ASERTPrev) (t1 t2 : Z) (height : nat),
    0 <= ap_target anchor ->
    t1 <= t2 ->
    get_next_target_model anchor (t1 :: nil) height <=
    get_next_target_model anchor (t2 :: nil) height.
Proof.
  intros anchor t1 t2 height Ht Htime.
  unfold get_next_target_model, asert_next_from_parent, asert_time_error.
  simpl.
  apply apply_shift_mono.
  - exact Ht.
  - apply saturate_range.
  - apply saturate_mono.
    apply quot_mono.
    lia.
  - apply saturate_range.
Qed.

Theorem asert_bounded_adjustment :
  forall (anchor parent : ASERTPrev),
    Z.abs (asert_time_error (ap_time parent) (ap_time anchor)
                            (ap_height parent) (ap_height anchor))
    < asert_half_life ->
    asert_next_from_parent anchor parent = ap_target anchor.
Proof.
  intros anchor parent Hte.
  unfold asert_next_from_parent.
  rewrite (quot_small _ asert_half_life asert_half_life_pos Hte).
  unfold saturate_half_life.
  simpl.
  apply apply_shift_zero.
Qed.

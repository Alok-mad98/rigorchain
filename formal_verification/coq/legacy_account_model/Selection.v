(**
  LEGACY ACCOUNT MODEL — NOT THE C++ UTXO IMPLEMENTATION (W9).

  Selection over a list-of-tx [Block], not the C++ UTXO draw set.
  Replay / mint structure: ReplayReorgCore/. Draw-shape lemmas that
  still build against this file are not a C++ refinement.

  Honesty: equivalence_claimed = false;
  hash_kernel_shared_binary_with_cpp = false; refinement_proved = false.
*)
Require Import Coq.ZArith.ZArith.
Require Import Coq.Lists.List.
Import ListNotations.
Require Import Rigor.legacy_account_model.Types.
From Rigor Require Import RigorBytes.
Include RigorBytes.
Require Import Sigma.

Definition weight (t : Tx) : Z :=
  match asset_in t with
  | ORU => mint_cost t
  | RES => fee t
  end.

(** Minimum admissible convergence weight, in satoshis (1 ORU = 10^8 sat).
    Mirrors [Consensus::MIN_CONVERGENCE_AMOUNT] in core/src/consensus/params.h. *)
Definition MIN_CONVERGENCE_AMOUNT : Z := 100000000.

(** Width, in blocks, of the sliding window the candidate set F is enumerated over.
    Mirrors [Consensus::CONVERGENCE_DRAW_WINDOW] in core/src/consensus/rigor_consensus_lock.h.

    SECURITY-001-A9-C. Under W = 1 the producer of block h-1 chose F unilaterally and at zero
    marginal cost, so RES allocation tracked hashrate rather than burn. Widening the window does
    not shrink a large producer's share of F; it removes exclusivity, since candidates in
    [h-W, h-2] are buried and cannot be censored. *)
Definition CONVERGENCE_DRAW_WINDOW : Z := 32.

(** RFC-014 v1.1 §2.1. Corrected under SECURITY-001-A finding A-6, then divided by the draw
    window under SECURITY-001-A9-C.

    RFC-014 v1.0 fixed T = 2^240 by reasoning at a normalised weight f = 1, but the
    implemented [weight] is denominated in satoshis. At the minimum admissible weight the
    right-hand side of the draw inequality then exceeded the digest range by a factor of
    ~1526, so [eligible] was true for every candidate and the filter selected nothing.
    See [pre_a6_target_was_vacuous] below, which proves this.

    A-9-C then made each convergence a candidate in W consecutive draws instead of one. Dividing
    the target by the same W holds issuance fixed: the per-draw probability becomes 1/W of the
    old value while the expected number of wins per candidate is unchanged.

    RFC-017 §5.3 then moved the exponent E from 240 to 246. Unlike A-6 and A-9-C this is not a
    correction of a defect — it is a monetary decision, and it is the only one of the three that
    changes what the chain issues rather than how it selects.

    E fixes the ORU cost of one mint at C = 2^(256-E) ORU: 65536 at E = 240, 1024 at E = 246.
    Because the reward schedule W(k) is a protocol constant (ResSchedule.v) rather than a
    function of demand, C is the entire cost side of the system, and C / W(k) is the floor price
    of one RES in ORU. At E = 240 that floor was so high relative to the ORU subsidy that the
    whole chain's output funded a mint only every ~65 blocks, and cumulative RES issuance under
    100%-burn stayed under 1% of the cap — the cap described nothing. E = 246 brings the
    economically reachable ceiling and the scheduled ceiling into the same order of magnitude, so
    the cap becomes a meaningful bound on dilution.

    Neither direction is free. Every proof in this file is a statement about T alone and holds at
    both exponents; the constraint E must satisfy is that T stay non-vacuous, which
    [res_probability_target_not_vacuous] discharges by computation at the new value. The margin
    to vacuity at E = 246 is 2^15 (the boundary is E = 261). *)
Definition RES_PROBABILITY_TARGET_UNWINDOWED : Z := Z.shiftl 1 246 / MIN_CONVERGENCE_AMOUNT.

Definition RES_PROBABILITY_TARGET : Z :=
  RES_PROBABILITY_TARGET_UNWINDOWED / CONVERGENCE_DRAW_WINDOW.

Definition eligible (t : Tx) (C B : list byte) (h : Z) : Prop :=
  Z.lt (Sigma t C B h) (RES_PROBABILITY_TARGET * weight t).

Definition eligible_with_T (t : Tx) (C B : list byte) (h T : Z) : Prop :=
  Z.lt (Sigma t C B h) (T * weight t).

(** * SECURITY-001-A / A-6: non-vacuity of the draw inequality

    [Sigma] ranges over [0, 2^256). If [T * f] can reach 2^256 then [eligible] is
    unconditionally true and the draw degenerates: every convergence transaction in the
    block passes the filter, and RES issuance is decided entirely by which transactions the
    block producer chose to include. These lemmas are the formal statement of the property
    whose absence allowed the defective constant to ship. *)

Definition target_not_vacuous (T : Z) : Prop :=
  Z.lt 0 (T * MIN_CONVERGENCE_AMOUNT) /\
  Z.lt (T * MIN_CONVERGENCE_AMOUNT) (Z.shiftl 1 256).

(** The active constant is selective at the minimum admissible weight. *)
Lemma res_probability_target_not_vacuous : target_not_vacuous RES_PROBABILITY_TARGET.
Proof. unfold target_not_vacuous. split; vm_compute; reflexivity. Qed.

(** The pre-A-6 constant, retained only as a regression witness. Do not use.

    The 240 here is the A-6 defect — the undivided target — and is unrelated to the exponent E,
    which separately happened to be 240 before RFC-017. The defect was the missing division by
    [MIN_CONVERGENCE_AMOUNT], not the exponent's value. *)
Definition RES_PROBABILITY_TARGET_PRE_A6 : Z := Z.shiftl 1 240.

(** The old constant was vacuous: it exceeded the entire digest range at minimum weight. *)
Lemma pre_a6_target_was_vacuous : ~ target_not_vacuous RES_PROBABILITY_TARGET_PRE_A6.
Proof.
  unfold target_not_vacuous, RES_PROBABILITY_TARGET_PRE_A6.
  intros [_ Hlt]. vm_compute in Hlt. discriminate.
Qed.

(** Consequence: under the old constant, no digest whatsoever could be filtered out at the
    minimum weight. This is the concrete exploitability statement — [eligible] carried no
    information, so candidate admission was fully block-producer controlled. *)
Lemma pre_a6_every_digest_was_eligible :
  forall sigma : Z,
    Z.le 0 sigma ->
    Z.lt sigma (Z.shiftl 1 256) ->
    Z.lt sigma (RES_PROBABILITY_TARGET_PRE_A6 * MIN_CONVERGENCE_AMOUNT).
Proof.
  intros sigma _ Hlt.
  eapply Z.lt_trans; [ exact Hlt | ].
  unfold RES_PROBABILITY_TARGET_PRE_A6. vm_compute. reflexivity.
Qed.

(** By contrast the active constant does filter: the maximal digest is rejected at the
    minimum weight. *)
Lemma res_probability_target_rejects_max_digest :
  ~ Z.lt (Z.shiftl 1 256 - 1) (RES_PROBABILITY_TARGET * MIN_CONVERGENCE_AMOUNT).
Proof. vm_compute. discriminate. Qed.

(** * RFC-017 §5.3: what E actually prices

    Non-vacuity above is a correctness floor — it only says the filter filters. E is chosen for a
    quantitative reason, and these two lemmas pin the quantity so that changing the exponent
    without revisiting the monetary argument is a proof failure rather than a silent repricing.

    [mint_cost_is_1024_oru] is the economic content of E: the expected ORU weight consumed per
    successful draw is 2^256 / T_unwindowed = 2^(256-E) ORU. It is C in RFC-017, the numerator of
    the cost floor C / W(k), and it is a protocol constant — independent of demand, of the ORU
    subsidy schedule, and of every piece of chain state. That independence is why the cost floor
    is a schedule rather than a market outcome.

    [saturation_weight_is_32768_oru] pins the other end. Above the saturation weight the draw
    inequality holds for every digest, so additional ORU on one candidate buys no additional
    probability and the filter stops discriminating. Saturation lies a factor of 32 above C, so a
    single participant cannot buy certainty at the cost of one mint; it costs 32x. *)

Definition CONVERGENCE_SATURATION_WEIGHT : Z := 32768 * MIN_CONVERGENCE_AMOUNT.

Lemma mint_cost_is_1024_oru :
  (Z.shiftl 1 256 / RES_PROBABILITY_TARGET_UNWINDOWED = 1024 * MIN_CONVERGENCE_AMOUNT)%Z.
Proof. vm_compute. reflexivity. Qed.

(** The largest weight that still leaves the filter discriminating: the maximal digest is
    rejected here, but any larger weight admits every digest. *)
Lemma saturation_weight_is_32768_oru :
  ~ Z.lt (Z.shiftl 1 256 - 1) (RES_PROBABILITY_TARGET * CONVERGENCE_SATURATION_WEIGHT) /\
  Z.lt (Z.shiftl 1 256 - 1)
       (RES_PROBABILITY_TARGET * (CONVERGENCE_SATURATION_WEIGHT + MIN_CONVERGENCE_AMOUNT)).
Proof. split; vm_compute; [ discriminate | reflexivity ]. Qed.

(** * SECURITY-001-A9-C: issuance neutrality of the window division

    Widening F from one block to W means each convergence transaction is a candidate in W
    consecutive draws. Dividing the target by W is what keeps aggregate issuance fixed, and these
    lemmas pin the arithmetic relation the argument rests on.

    The division is a floor, not exact: the pre-windowing target is not a multiple of W (its
    residue is 27 at E = 246; it was 28 at E = 240). The bracketing lemma states precisely how
    much is lost, which bounds the deviation from exact neutrality at one part in
    [RES_PROBABILITY_TARGET_UNWINDOWED]. *)

Lemma window_is_a_power_of_two : CONVERGENCE_DRAW_WINDOW = Z.shiftl 1 5.
Proof. vm_compute. reflexivity. Qed.

(** A window of 1 would be no window at all, i.e. the F-1 defect A-9-C exists to remove. *)
Lemma window_widens_the_candidate_set : Z.lt 1 CONVERGENCE_DRAW_WINDOW.
Proof. vm_compute. reflexivity. Qed.

Lemma target_is_unwindowed_divided_by_window :
  Z.le (RES_PROBABILITY_TARGET * CONVERGENCE_DRAW_WINDOW) RES_PROBABILITY_TARGET_UNWINDOWED /\
  Z.lt RES_PROBABILITY_TARGET_UNWINDOWED ((RES_PROBABILITY_TARGET + 1) * CONVERGENCE_DRAW_WINDOW).
Proof. split; [ vm_compute; discriminate | vm_compute; reflexivity ]. Qed.

(** Dividing a non-vacuous target by a positive window cannot make it vacuous: the filter only
    becomes stricter, so A-9-C cannot reintroduce the A-6 degeneracy. *)
(** The complementary A-9-C rule — a given txid wins at most once across the
    window — lives in [WindowExclusion.v] ([txid_wins_at_most_once]). This file
    only models the width W and the T/W target. *)

Lemma windowing_preserves_non_vacuity :
  target_not_vacuous RES_PROBABILITY_TARGET_UNWINDOWED ->
  target_not_vacuous RES_PROBABILITY_TARGET.
Proof. intros _. exact res_probability_target_not_vacuous. Qed.

Definition sigma_product_lt (x y : Tx) (C B : list byte) (h : Z) : bool :=
  let sx := Sigma x C B h in
  let sy := Sigma y C B h in
  let wx := weight x in
  let wy := weight y in
  (sx * wy <? sy * wx)%Z.

Definition sigma_product_eq (x y : Tx) (C B : list byte) (h : Z) : bool :=
  let sx := Sigma x C B h in
  let sy := Sigma y C B h in
  let wx := weight x in
  let wy := weight y in
  (sx * wy =? sy * wx)%Z.

(** Strictly better draw candidate: lower cross-product, or tie then lower txid (memcmp). *)
Definition pick_better_tx (x y : Tx) (C B : list byte) (h : Z) : Tx :=
  if sigma_product_lt x y C B h then x
  else if sigma_product_lt y x C B h then y
  else if bytes_ltb (tx_id_bytes x) (tx_id_bytes y) then x
  else y.

Definition better (a b : Tx) (C B : list byte) (h : Z) : Prop :=
  pick_better_tx a b C B h = a /\ pick_better_tx a b C B h <> b.

Fixpoint select_winner (l : list Tx) (C B : list byte) (h : Z) : option Tx :=
  match l with
  | [] => None
  | x :: xs =>
      match select_winner xs C B h with
      | None => Some x
      | Some y => Some (pick_better_tx x y C B h)
      end
  end.

(** Candidates that actually enter selection: the inequality-eligible subset of F.
    [draw_candidate_root] (DrawCandidateRoot.v) hashes this list. The filter is
    the same predicate [Execute] inlines; unfolding either yields the other. *)
Definition eligible_bool (t : Tx) (C B : list byte) (h : Z) : bool :=
  if Z_lt_dec (Sigma t C B h) (RES_PROBABILITY_TARGET * weight t) then true else false.

Definition eligible_candidates (l : list Tx) (C B : list byte) (h : Z) : list Tx :=
  filter (fun t => eligible_bool t C B h) l.

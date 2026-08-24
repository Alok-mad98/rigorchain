(** ReplayReorgCore/ConnectBlockMint.v
    ConnectBlock-aligned ORU/RES mint on [apply_supply_delta] (PHASE-4E).

    Constants match [ConnectBlock.v] scaffold; replay path applies mint only here. *)
From Coq Require Import ZArith.ZArith.
From Coq Require Import Lia.
From Coq Require Import Arith.
Require Import Rigor.ReplayReorgCore.ReplayState.
From Rigor Require Import ResSchedule.

(** RFC-017. The mint amount was a constant, [cb_RES_MINT_AMOUNT = 10000] centi-RES; it is now
    the scheduled reward at the block's own height, and the cap is in 10^-6 RES atoms.

    Taking the height from [blk_header] rather than from [ReplayState] is what keeps the replay
    path sound under reorg. The amount a block mints must be a function of that block, not of the
    tip it happens to be applied to — otherwise replaying the same block on a different fork
    would mint a different amount and [supply_evolution_eq] would stop being a statement about
    the block at all. Every lemma below is therefore parameterised by [b], not by [s]. *)
Definition cb_res_mint_amount (b : BlockAbs) : Z :=
  get_res_mint_reward (Z.of_nat (hdr_height (blk_header b))).

Definition cb_MAX_RES_SUPPLY : Z := RES_MAX_SUPPLY.

Lemma cb_res_mint_amount_nonneg : forall b : BlockAbs, (0 <= cb_res_mint_amount b)%Z.
Proof. intros b. apply get_res_mint_reward_nonneg. Qed.

Lemma cb_res_mint_amount_le_genesis :
  forall b : BlockAbs, (cb_res_mint_amount b <= RES_REWARD_GENESIS)%Z.
Proof.
  intros b. unfold cb_res_mint_amount, get_res_mint_reward.
  destruct (Z.ltb (Z.of_nat (hdr_height (blk_header b))) 0);
    [unfold RES_REWARD_GENESIS, RES_COIN; lia | apply res_reward_for_epoch_le_genesis].
Qed.

(** Winner-must-mint / H-1 validity (RFC-017 §4.4, C++ ConnectBlock).

    [res_mint_authorized] is [W(h) > 0]. A won draw does *not* authorize a mint
    once the schedule has terminated: [¬authorized → mint_count = 0] even if
    [cm_draw_won = true]. This is the block-acceptance half of H-1; the supply
    half remains [cb_res_mint_delta_zero_when_amount_zero].

    Cap overshoot is an independent reject ([valid_res_cap], wired into
    [valid_block_connect]): a mint that would take cumulative supply past
    [RES_MAX_SUPPLY] is [bad-rigor-res-max-supply], not a zeroed delta. When
    [W(h) > 0] a winner must still mint, so overshoot is fail-closed.

    [cm_draw_won] is an unconstrained field of [ConnectMintSpec], not owner /
    PCORB / Selection ([OpaqueBoundary.v]). *)
Definition res_mint_authorized (b : BlockAbs) : bool :=
  Z.gtb (cb_res_mint_amount b) 0.

Definition valid_res_mint_obligation (b : BlockAbs) : bool :=
  let authorized := res_mint_authorized b in
  let won := cm_draw_won (blk_mint b) in
  let n := cm_res_mint_count (blk_mint b) in
  if authorized then
    if won then Nat.eqb n 1 else Nat.eqb n 0
  else
    Nat.eqb n 0.

(** C++ [bad-rigor-res-winner-mismatch]: [W(h) > 0], a winner exists, and
    the scored mint count is not exactly one. RFC-017 §4.4: this path is
    live only while the schedule still pays. *)
Definition winner_mismatch_path (b : BlockAbs) : bool :=
  andb (res_mint_authorized b)
       (andb (cm_draw_won (blk_mint b))
             (negb (Nat.eqb (cm_res_mint_count (blk_mint b)) 1))).

(** C++ [bad-rigor-res-amount] after schedule end: [W(h) = 0] and the block
    still records a mint. Not winner-mismatch — H-1 does not require a mint
    once the schedule is exhausted. *)
Definition unauthorized_mint_path (b : BlockAbs) : bool :=
  andb (negb (res_mint_authorized b))
       (negb (Nat.eqb (cm_res_mint_count (blk_mint b)) 0)).

(** C++ [bad-rigor-res-without-winner]: no winner, but a mint is present. *)
Definition without_winner_mint_path (b : BlockAbs) : bool :=
  andb (negb (cm_draw_won (blk_mint b)))
       (negb (Nat.eqb (cm_res_mint_count (blk_mint b)) 0)).

(** ** ORU issuance schedule (SECURITY-001-F G-2)

    The model used to clamp the coinbase ORU subsidy at zero from below and leave it unbounded
    from above, and [valid_block_connect] never looked at it — so in the model a block could mint
    any amount of ORU it liked. Production does not: [ConnectBlock] rejects
    [oru_coinbase_value > fees + GetBlockSubsidy(height)] as `bad-cb-amount`
    (`core/src/validation.cpp`). The model was wrong, not the code, and the schedule is mirrored
    here so it stays that way.

    Mirrors ORU_INITIAL_BLOCK_SUBSIDY, ORU_SUBSIDY_HALVING_INTERVAL and ORU_SUBSIDY_MAX_SHIFTS in
    `core/src/consensus/rigor_consensus_lock.h`. Amounts are satoshis. *)
Definition cb_ORU_INITIAL_SUBSIDY : Z := 100000000000%Z.  (* 1000 ORU *)
Definition cb_ORU_HALVING_INTERVAL : Z := 210000%Z.
Definition cb_ORU_MAX_HALVING_SHIFTS : Z := 3%Z.

(** Halvings elapsed at this height, capped. Kept in [Z]: the interval as a unary [nat] literal
    is 210000 successors and blows the stack in any proof that touches it. *)
Definition cb_oru_shift (height : nat) : Z :=
  Z.min (Z.of_nat height / cb_ORU_HALVING_INTERVAL) cb_ORU_MAX_HALVING_SHIFTS.

(** The subsidy a block at this height is allowed to pay: halve every interval, then stop.

    Capping the shift count at three is why RIGOR has no ORU supply cap. The subsidy floors at
    125 ORU and is paid forever, so the G-2 target `total_oru_supply <= ORU_MAX_SUPPLY` is not
    merely unproven — it is false by design. What is provable, and what is proved in
    SupplyConservation.v, is the linear bound: n blocks add at most n subsidies. *)
Definition cb_expected_oru_subsidy (height : nat) : Z :=
  Z.shiftr cb_ORU_INITIAL_SUBSIDY (cb_oru_shift height).

(** Non-negative ORU subsidy clamp. *)
Definition cb_oru_subsidy (spec : ConnectMintSpec) : Z :=
  Z.max 0 (cm_oru_subsidy spec).

(** The check [valid_block_connect] was missing: issuance is capped by the schedule. *)
Definition cb_oru_subsidy_ok (height : nat) (spec : ConnectMintSpec) : bool :=
  Z.leb (cm_oru_subsidy spec) (cb_expected_oru_subsidy height).

Lemma cb_oru_shift_bounds :
  forall h : nat, (0 <= cb_oru_shift h <= cb_ORU_MAX_HALVING_SHIFTS)%Z.
Proof.
  intros h. unfold cb_oru_shift, cb_ORU_HALVING_INTERVAL, cb_ORU_MAX_HALVING_SHIFTS.
  split.
  - apply Z.min_glb; [apply Z.div_pos; [apply Nat2Z.is_nonneg | lia] | lia].
  - apply Z.le_min_r.
Qed.

Lemma cb_oru_divisor_bounds :
  forall h : nat, (1 <= 2 ^ cb_oru_shift h <= 8)%Z.
Proof.
  intros h. destruct (cb_oru_shift_bounds h) as [Hlo Hhi].
  unfold cb_ORU_MAX_HALVING_SHIFTS in Hhi.
  split.
  - replace 1%Z with (2 ^ 0)%Z by reflexivity. apply Z.pow_le_mono_r; lia.
  - replace 8%Z with (2 ^ 3)%Z by reflexivity. apply Z.pow_le_mono_r; lia.
Qed.

Lemma cb_expected_oru_subsidy_pos :
  forall h : nat, (0 < cb_expected_oru_subsidy h)%Z.
Proof.
  intros h. unfold cb_expected_oru_subsidy, cb_ORU_INITIAL_SUBSIDY.
  destruct (cb_oru_shift_bounds h) as [Hlo _].
  destruct (cb_oru_divisor_bounds h) as [H1 H8].
  rewrite Z.shiftr_div_pow2 by exact Hlo.
  apply Z.div_str_pos. lia.
Qed.

(** No height ever pays more than the genesis-era subsidy. *)
Lemma cb_expected_oru_subsidy_le_initial :
  forall h : nat, (cb_expected_oru_subsidy h <= cb_ORU_INITIAL_SUBSIDY)%Z.
Proof.
  intros h. unfold cb_expected_oru_subsidy, cb_ORU_INITIAL_SUBSIDY.
  destruct (cb_oru_shift_bounds h) as [Hlo _].
  destruct (cb_oru_divisor_bounds h) as [H1 _].
  rewrite Z.shiftr_div_pow2 by exact Hlo.
  apply Z.div_le_upper_bound; nia.
Qed.

(** Attempted RES mint for the ConnectBlock cap check.

    Mirrors production: [res_reward_expected] is the body mint sum, which is 0
    when [mint_count = 0] and [W(h)] when a mint is present (obligation already
    forces a present mint to equal the schedule). *)
Definition cb_res_attempted_amount (b : BlockAbs) : Z :=
  if Nat.eqb (cm_res_mint_count (blk_mint b)) 0 then 0%Z
  else cb_res_mint_amount b.

Lemma cb_res_attempted_amount_nonneg :
  forall b : BlockAbs, (0 <= cb_res_attempted_amount b)%Z.
Proof.
  intros b. unfold cb_res_attempted_amount.
  destruct (Nat.eqb (cm_res_mint_count (blk_mint b)) 0);
    [lia | apply cb_res_mint_amount_nonneg].
Qed.

Lemma cb_res_attempted_amount_zero_when_no_mint :
  forall b : BlockAbs,
    cm_res_mint_count (blk_mint b) = 0 ->
    cb_res_attempted_amount b = 0%Z.
Proof.
  intros b Hn. unfold cb_res_attempted_amount. rewrite Hn. reflexivity.
Qed.

Lemma cb_res_attempted_amount_eq_W_when_mint :
  forall b : BlockAbs,
    cm_res_mint_count (blk_mint b) <> 0 ->
    cb_res_attempted_amount b = cb_res_mint_amount b.
Proof.
  intros b Hn. unfold cb_res_attempted_amount.
  destruct (Nat.eqb (cm_res_mint_count (blk_mint b)) 0) eqn:Hz; [|reflexivity].
  apply Nat.eqb_eq in Hz. contradiction.
Qed.

(** [bad-rigor-res-max-supply]: reject when cumulative supply plus the
    attempted mint would exceed the hard cap. Equal-to-cap is admitted.
    Mirrors [res_minted_before > RES_MAX_SUPPLY - res_reward_expected].

    This is the explicit reject path. The delta clamp below stays as the
    RFC-017 redundant runtime assertion. *)
Definition valid_res_cap (s : ReplayState) (b : BlockAbs) : bool :=
  res_supply_plus_mint_within_cap (rs_res_supply s) (cb_res_attempted_amount b).

Lemma valid_res_cap_iff :
  forall (s : ReplayState) (b : BlockAbs),
    valid_res_cap s b = true <->
    (rs_res_supply s + cb_res_attempted_amount b <= cb_MAX_RES_SUPPLY)%Z.
Proof.
  intros s b. unfold valid_res_cap, res_supply_plus_mint_within_cap, cb_MAX_RES_SUPPLY.
  split; apply Z.leb_le.
Qed.

Lemma valid_res_cap_exact :
  forall (s : ReplayState) (b : BlockAbs),
    (rs_res_supply s + cb_res_attempted_amount b = cb_MAX_RES_SUPPLY)%Z ->
    valid_res_cap s b = true.
Proof.
  intros s b Heq. unfold valid_res_cap, cb_MAX_RES_SUPPLY in *.
  apply res_exact_cap_admitted. exact Heq.
Qed.

Lemma valid_res_cap_overshoot :
  forall (s : ReplayState) (b : BlockAbs),
    (cb_MAX_RES_SUPPLY < rs_res_supply s + cb_res_attempted_amount b)%Z ->
    valid_res_cap s b = false.
Proof.
  intros s b Hlt. unfold valid_res_cap, cb_MAX_RES_SUPPLY in *.
  apply res_overshoot_rejected. exact Hlt.
Qed.

Lemma valid_res_cap_overshoot_one_atom :
  forall (s : ReplayState) (b : BlockAbs),
    (rs_res_supply s + cb_res_attempted_amount b = cb_MAX_RES_SUPPLY + 1)%Z ->
    valid_res_cap s b = false.
Proof.
  intros s b Heq. unfold valid_res_cap, cb_MAX_RES_SUPPLY in *.
  apply res_overshoot_one_atom_rejected. exact Heq.
Qed.

Lemma valid_res_cap_implies_pre_le_cap :
  forall (s : ReplayState) (b : BlockAbs),
    valid_res_cap s b = true ->
    (rs_res_supply s <= cb_MAX_RES_SUPPLY)%Z.
Proof.
  intros s b H. apply valid_res_cap_iff in H.
  pose proof (cb_res_attempted_amount_nonneg b). lia.
Qed.

Lemma valid_res_cap_no_mint :
  forall (s : ReplayState) (b : BlockAbs),
    cm_res_mint_count (blk_mint b) = 0 ->
    valid_res_cap s b = true <-> (rs_res_supply s <= cb_MAX_RES_SUPPLY)%Z.
Proof.
  intros s b Hn. unfold valid_res_cap.
  rewrite cb_res_attempted_amount_zero_when_no_mint by exact Hn.
  unfold res_supply_plus_mint_within_cap, cb_MAX_RES_SUPPLY.
  split.
  - intro H. apply Z.leb_le in H. lia.
  - intro H. apply Z.leb_le. lia.
Qed.

(** RES mint: at most one scheduled reward per block when draw wins and cap allows.

    The cap comparison on the delta is the RFC-017 redundant runtime clamp:
    overshoot is already rejected by [valid_res_cap] on [valid_block_connect].
    Keeping the clamp means [res_mint_respects_cap] remains a restatement of
    the delta, independent of connect. *)
Definition cb_res_mint_delta (s : ReplayState) (b : BlockAbs) : Z :=
  if cm_draw_won (blk_mint b) then
    if Z.leb (rs_res_supply s + cb_res_mint_amount b) cb_MAX_RES_SUPPLY then
      cb_res_mint_amount b
    else
      0%Z
  else
    0%Z.

(** H-1 supply half: when the schedule pays nothing, a won draw still contributes
    zero RES. Block acceptance of a zero-mint winner at [W(h) = 0] is
    [valid_res_mint_obligation] (wired into [valid_block_connect]). *)
Lemma cb_res_mint_delta_zero_when_amount_zero :
  forall (s : ReplayState) (b : BlockAbs),
    cb_res_mint_amount b = 0%Z ->
    cb_res_mint_delta s b = 0%Z.
Proof.
  intros s b H. unfold cb_res_mint_delta.
  destruct (cm_draw_won (blk_mint b)); [| reflexivity].
  rewrite H.
  destruct (Z.leb (rs_res_supply s + 0)%Z cb_MAX_RES_SUPPLY); reflexivity.
Qed.

Definition cb_oru_mint_delta (s : ReplayState) (b : BlockAbs) : Z :=
  cb_oru_subsidy (blk_mint b).

Definition protocol_oru_mint_delta (s : ReplayState) (b : BlockAbs) : Z :=
  cb_oru_mint_delta s b.

Definition protocol_res_mint_delta (s : ReplayState) (b : BlockAbs) : Z :=
  cb_res_mint_delta s b.

Definition protocol_mint_total_delta (s : ReplayState) (b : BlockAbs) : Z :=
  (protocol_oru_mint_delta s b + protocol_res_mint_delta s b)%Z.

(** Single authorized supply channel for replay [apply_block]. *)
Definition apply_connectblock_supply_delta (s : ReplayState) (b : BlockAbs)
  : ReplayState :=
  {| rs_utxo := rs_utxo s;
     rs_chain := rs_chain s;
     rs_oru_supply := (rs_oru_supply s + cb_oru_mint_delta s b)%Z;
     rs_res_supply := (rs_res_supply s + cb_res_mint_delta s b)%Z;
     rs_history := rs_history s |}.

Lemma protocol_mint_total_delta_non_negative :
  forall (s : ReplayState) (b : BlockAbs),
    (0 <= protocol_mint_total_delta s b)%Z.
Proof.
  intros s b. pose proof (cb_res_mint_amount_nonneg b) as Hw.
  unfold protocol_mint_total_delta.
  unfold protocol_oru_mint_delta, protocol_res_mint_delta.
  unfold cb_oru_mint_delta, cb_res_mint_delta, cb_oru_subsidy.
  destruct (cm_draw_won (blk_mint b)).
  - destruct (Z.leb (rs_res_supply s + cb_res_mint_amount b) cb_MAX_RES_SUPPLY); lia.
  - lia.
Qed.

Lemma cb_res_mint_delta_bounded :
  forall (s : ReplayState) (b : BlockAbs),
    (cb_res_mint_delta s b = 0%Z \/
     cb_res_mint_delta s b = cb_res_mint_amount b)%Z.
Proof.
  intros s b. unfold cb_res_mint_delta.
  destruct (cm_draw_won (blk_mint b)); [|left; reflexivity].
  destruct (Z.leb (rs_res_supply s + cb_res_mint_amount b) cb_MAX_RES_SUPPLY);
    [right; reflexivity | left; reflexivity].
Qed.

(** RFC-017 strengthening. The delta is bounded above by the genesis reward at every height, so
    the per-block mint has a closed bound that does not depend on the height at all. *)
Lemma cb_res_mint_delta_le_genesis :
  forall (s : ReplayState) (b : BlockAbs),
    (cb_res_mint_delta s b <= RES_REWARD_GENESIS)%Z.
Proof.
  intros s b.
  destruct (cb_res_mint_delta_bounded s b) as [H | H]; rewrite H.
  - unfold RES_REWARD_GENESIS, RES_COIN. discriminate.
  - unfold cb_res_mint_amount, get_res_mint_reward.
    destruct (Z.ltb _ 0); [ unfold RES_REWARD_GENESIS, RES_COIN; discriminate | ].
    apply res_reward_for_epoch_le_genesis.
Qed.

Lemma apply_connectblock_supply_delta_oru :
  forall (s : ReplayState) (b : BlockAbs),
    rs_oru_supply (apply_connectblock_supply_delta s b) =
      (rs_oru_supply s + cb_oru_mint_delta s b)%Z.
Proof.
  intros. unfold apply_connectblock_supply_delta. reflexivity.
Qed.

Lemma apply_connectblock_supply_delta_res :
  forall (s : ReplayState) (b : BlockAbs),
    rs_res_supply (apply_connectblock_supply_delta s b) =
      (rs_res_supply s + cb_res_mint_delta s b)%Z.
Proof.
  intros. unfold apply_connectblock_supply_delta. reflexivity.
Qed.

Lemma apply_connectblock_supply_delta_total :
  forall (s : ReplayState) (b : BlockAbs),
    (rs_oru_supply (apply_connectblock_supply_delta s b) +
     rs_res_supply (apply_connectblock_supply_delta s b))%Z =
      (rs_oru_supply s + rs_res_supply s +
       protocol_mint_total_delta s b)%Z.
Proof.
  intros s b.
  unfold apply_connectblock_supply_delta, protocol_mint_total_delta.
  unfold protocol_oru_mint_delta, protocol_res_mint_delta.
  unfold cb_oru_mint_delta, cb_res_mint_delta, cb_oru_subsidy.
  destruct (cm_draw_won (blk_mint b)); simpl.
  - destruct (Z.leb (rs_res_supply s + cb_res_mint_amount b) cb_MAX_RES_SUPPLY); lia.
  - lia.
Qed.

Lemma apply_connectblock_supply_delta_preserves_other :
  forall (s : ReplayState) (b : BlockAbs),
    rs_utxo (apply_connectblock_supply_delta s b) = rs_utxo s /\
    rs_chain (apply_connectblock_supply_delta s b) = rs_chain s /\
    rs_history (apply_connectblock_supply_delta s b) = rs_history s.
Proof.
  intros. unfold apply_connectblock_supply_delta. repeat split; reflexivity.
Qed.

Definition supply_evolution_eq (s s' : ReplayState) (b : BlockAbs) : Prop :=
  (rs_oru_supply s' + rs_res_supply s')%Z =
    (rs_oru_supply s + rs_res_supply s + protocol_mint_total_delta s b)%Z.

Lemma apply_connectblock_supply_delta_supply_eq :
  forall (s : ReplayState) (b : BlockAbs),
    supply_evolution_eq s (apply_connectblock_supply_delta s b) b.
Proof.
  intros s b. unfold supply_evolution_eq.
  rewrite apply_connectblock_supply_delta_total. reflexivity.
Qed.

Lemma default_mint_zero_delta :
  forall (s : ReplayState) (hdr : BlockHeaderAbs) (txs : list TxAbs),
    protocol_mint_total_delta s
      {| blk_header := hdr; blk_txs := txs; blk_mint := default_connect_mint |} =
      0%Z.
Proof.
  intros s hdr txs. unfold protocol_mint_total_delta.
  unfold protocol_oru_mint_delta, protocol_res_mint_delta.
  unfold cb_oru_mint_delta, cb_res_mint_delta, cb_oru_subsidy.
  simpl. lia.
Qed.

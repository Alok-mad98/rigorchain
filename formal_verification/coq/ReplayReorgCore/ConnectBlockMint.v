(** ReplayReorgCore/ConnectBlockMint.v
    ConnectBlock-aligned ORU/RES mint on [apply_supply_delta] (PHASE-4E).

    Constants match [ConnectBlock.v] scaffold; replay path applies mint only here. *)
From Coq Require Import ZArith.ZArith.
From Coq Require Import Lia.
Require Import Rigor.ReplayReorgCore.ReplayState.

(** RFC-015 / ConnectBlock scaffold constants (centi-RES units for cap). *)
Definition cb_RES_MINT_AMOUNT : Z := 10000%Z.
Definition cb_MAX_RES_SUPPLY : Z := (21000000 * 100)%Z.

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

(** RES mint: at most one [cb_RES_MINT_AMOUNT] per block when draw wins and cap allows. *)
Definition cb_res_mint_delta (s : ReplayState) (b : BlockAbs) : Z :=
  if cm_draw_won (blk_mint b) then
    if Z.leb (rs_res_supply s + cb_RES_MINT_AMOUNT) cb_MAX_RES_SUPPLY then
      cb_RES_MINT_AMOUNT
    else
      0%Z
  else
    0%Z.

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
  intros s b. unfold protocol_mint_total_delta.
  unfold protocol_oru_mint_delta, protocol_res_mint_delta.
  unfold cb_oru_mint_delta, cb_res_mint_delta, cb_oru_subsidy.
  destruct (cm_draw_won (blk_mint b)); simpl.
  - destruct (Z.leb (rs_res_supply s + cb_RES_MINT_AMOUNT) cb_MAX_RES_SUPPLY) eqn:E.
    + rewrite Z.leb_le in E. unfold cb_RES_MINT_AMOUNT. lia.
    + simpl. lia.
  - simpl. lia.
Qed.

Lemma cb_res_mint_delta_bounded :
  forall (s : ReplayState) (b : BlockAbs),
    (cb_res_mint_delta s b = 0%Z \/
     cb_res_mint_delta s b = cb_RES_MINT_AMOUNT)%Z.
Proof.
  intros s b. unfold cb_res_mint_delta.
  destruct (cm_draw_won (blk_mint b)); [|left; reflexivity].
  destruct (Z.leb (rs_res_supply s + cb_RES_MINT_AMOUNT) cb_MAX_RES_SUPPLY);
    [right; reflexivity | left; reflexivity].
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
  - destruct (Z.leb (rs_res_supply s + cb_RES_MINT_AMOUNT) cb_MAX_RES_SUPPLY); lia.
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

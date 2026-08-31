(** fee-burn/Accounting.v
    RFC-018 RES-transfer ORU fee burn (circulation sink).

    Mirrors [Consensus::OruResTransferFeeBurnShare] in
    [core/src/consensus/rigor_consensus_lock.h]. Call sites
    ([ConnectBlock], [CreateNewBlock]) apply the share only when
    [IsORBClaimTx] (input domain = ASSET_RES). Pure ORU spends,
    Convergence, and ORU-funded RES mints contribute burn 0.

    Fees are ORU-denominated. RES supply is not reduced by fees.
    Burn is implemented by lowering the coinbase claimable cap; the
    unclaimed share never enters the UTXO set (no OP_RETURN parking).

    Not a C++ refinement. [equivalence_claimed = false]. *)
From Coq Require Import ZArith.ZArith.
From Coq Require Import Lia.
From Coq Require Import Bool.
From Coq Require Import List.
Import ListNotations.

Local Open Scope Z_scope.

(** * Parameters (RFC-018 §2) *)

Definition ORU_RES_TRANSFER_FEE_BURN_NUM : Z := 1.
Definition ORU_RES_TRANSFER_FEE_BURN_DEN : Z := 2.

Lemma burn_share_params_ok :
  0 < ORU_RES_TRANSFER_FEE_BURN_NUM <= ORU_RES_TRANSFER_FEE_BURN_DEN.
Proof. unfold ORU_RES_TRANSFER_FEE_BURN_NUM, ORU_RES_TRANSFER_FEE_BURN_DEN. lia. Qed.

(** Floor share of an ORU fee that coinbase must not claim.
    Identical to C++ [OruResTransferFeeBurnShare]: [fee <= 0] burns nothing;
    otherwise [floor(fee * NUM / DEN)]. With the locked 1/2 share this is
    [floor(fee / 2)] on non-negative fees. *)
Definition oru_res_transfer_fee_burn_share (fee : Z) : Z :=
  if fee <=? 0 then 0
  else (fee * ORU_RES_TRANSFER_FEE_BURN_NUM) / ORU_RES_TRANSFER_FEE_BURN_DEN.

(** * Transaction fee class (ConnectBlock eligibility) *)

Inductive TxFeeClass : Set :=
  | TxFeeORUOnly
  | TxFeeResTransfer
  | TxFeeMint
  | TxFeeConverge.

Definition is_res_transfer (c : TxFeeClass) : bool :=
  match c with
  | TxFeeResTransfer => true
  | _ => false
  end.

(** Burn amount actually accumulated for this tx. The share function is
    total; classification is the call-site guard. *)
Definition calculate_fee_burn (fee : Z) (c : TxFeeClass) : Z :=
  if is_res_transfer c then oru_res_transfer_fee_burn_share fee else 0.

Definition calculate_miner_fee (fee : Z) (c : TxFeeClass) : Z :=
  fee - calculate_fee_burn fee c.

(** Task-named projections for the RFC-018 share (RES-transfer case). *)
Definition ORU_fee (fee : Z) : Z := fee.
Definition ORU_burn (fee : Z) : Z := oru_res_transfer_fee_burn_share fee.
Definition ORU_miner_fee (fee : Z) : Z := fee - ORU_burn fee.

(** * Conservation *)

Lemma fee_conservation :
  forall fee : Z,
    ORU_fee fee = ORU_burn fee + ORU_miner_fee fee.
Proof.
  intros fee.
  unfold ORU_fee, ORU_miner_fee.
  lia.
Qed.

Lemma fee_conservation_classified :
  forall (fee : Z) (c : TxFeeClass),
    fee = calculate_fee_burn fee c + calculate_miner_fee fee c.
Proof.
  intros fee c.
  unfold calculate_miner_fee.
  lia.
Qed.

Lemma burn_share_nonneg :
  forall fee : Z, 0 <= oru_res_transfer_fee_burn_share fee.
Proof.
  intros fee.
  unfold oru_res_transfer_fee_burn_share, ORU_RES_TRANSFER_FEE_BURN_NUM,
         ORU_RES_TRANSFER_FEE_BURN_DEN.
  destruct (fee <=? 0) eqn:E; [lia|].
  apply Z.leb_gt in E.
  apply Z.div_pos; lia.
Qed.

Lemma burn_share_le_fee :
  forall fee : Z, 0 <= fee -> oru_res_transfer_fee_burn_share fee <= fee.
Proof.
  intros fee Hf.
  unfold oru_res_transfer_fee_burn_share, ORU_RES_TRANSFER_FEE_BURN_NUM,
         ORU_RES_TRANSFER_FEE_BURN_DEN.
  destruct (fee <=? 0) eqn:E; [lia|].
  apply Z.leb_gt in E.
  pose proof (Z.div_le_upper_bound (fee * 1) 2 fee).
  lia.
Qed.

Lemma calculate_fee_burn_nonneg :
  forall fee c, 0 <= calculate_fee_burn fee c.
Proof.
  intros fee c.
  unfold calculate_fee_burn.
  destruct (is_res_transfer c); [apply burn_share_nonneg | lia].
Qed.

Lemma calculate_fee_burn_le_fee :
  forall fee c, 0 <= fee -> calculate_fee_burn fee c <= fee.
Proof.
  intros fee c Hf.
  unfold calculate_fee_burn.
  destruct (is_res_transfer c); [apply burn_share_le_fee; exact Hf | lia].
Qed.

Lemma miner_fee_nonneg :
  forall fee c, 0 <= fee -> 0 <= calculate_miner_fee fee c.
Proof.
  intros fee c Hf.
  unfold calculate_miner_fee.
  pose proof (calculate_fee_burn_le_fee fee c Hf).
  lia.
Qed.

Lemma non_res_transfer_burns_nothing :
  forall fee c,
    is_res_transfer c = false ->
    calculate_fee_burn fee c = 0 /\ calculate_miner_fee fee c = fee.
Proof.
  intros fee c H.
  destruct c; try discriminate H.
  all: unfold calculate_fee_burn, calculate_miner_fee; simpl;
       split; [reflexivity | rewrite Z.sub_0_r; reflexivity].
Qed.

(** * Unspendable burn (never a UTXO) *)

(** Fee split as coinbase accounting. Burn has no output constructor:
    [fs_burn_utxo] is identically [None]. Only the complementary miner
    share may become a coinbase ORU output. *)
Record FeeSplitResult : Set := {
  fs_burn : Z;
  fs_miner : Z;
  fs_burn_utxo : option Z;
  fs_miner_utxo : option Z
}.

Definition split_fee (fee : Z) (c : TxFeeClass) : FeeSplitResult :=
  let b := calculate_fee_burn fee c in
  let m := calculate_miner_fee fee c in
  {| fs_burn := b;
     fs_miner := m;
     fs_burn_utxo := None;
     fs_miner_utxo := if 0 <? m then Some m else None |}.

Lemma burn_is_unspendable :
  forall (fee : Z) (c : TxFeeClass),
    fs_burn_utxo (split_fee fee c) = None.
Proof.
  intros fee c.
  unfold split_fee.
  reflexivity.
Qed.

Lemma miner_utxo_is_claimable_share :
  forall (fee : Z) (c : TxFeeClass),
    fs_miner_utxo (split_fee fee c) = None
    \/ fs_miner_utxo (split_fee fee c) = Some (calculate_miner_fee fee c).
Proof.
  intros fee c.
  unfold split_fee; simpl.
  destruct (0 <? calculate_miner_fee fee c); [right|left]; reflexivity.
Qed.

Lemma split_fee_conserves :
  forall fee c,
    fs_burn (split_fee fee c) + fs_miner (split_fee fee c) = fee.
Proof.
  intros fee c.
  unfold split_fee; simpl.
  unfold calculate_miner_fee.
  lia.
Qed.

(** * Burn is not miner-claimable *)

(** ConnectBlock claimable cap: [(Σ ORU fees − burn share) + subsidy].
    Overclaim is [bad-cb-amount]; underclaim is additional voluntary burn. *)
Definition coinbase_oru_cap (fees_oru burn_acc subsidy : Z) : Z :=
  (fees_oru - burn_acc) + subsidy.

Lemma burn_not_claimable :
  forall fee : Z,
    0 <= fee ->
    ORU_miner_fee fee = fee - ORU_burn fee
    /\ ORU_miner_fee fee <= fee
    /\ (0 < ORU_burn fee -> ORU_miner_fee fee < fee).
Proof.
  intros fee Hf.
  unfold ORU_miner_fee, ORU_burn.
  pose proof (burn_share_nonneg fee).
  pose proof (burn_share_le_fee fee Hf).
  repeat split; lia.
Qed.

Lemma burn_not_claimable_classified :
  forall (fee : Z) (c : TxFeeClass) (claimed : Z),
    0 <= fee ->
    claimed <= calculate_miner_fee fee c ->
    claimed + calculate_fee_burn fee c <= fee
    /\ (0 < calculate_fee_burn fee c -> claimed < fee).
Proof.
  intros fee c claimed Hf Hle.
  unfold calculate_miner_fee in Hle.
  pose proof (calculate_fee_burn_nonneg fee c).
  split; lia.
Qed.

Lemma coinbase_cap_excludes_burn :
  forall fees_oru burn_acc subsidy,
    coinbase_oru_cap fees_oru burn_acc subsidy + burn_acc =
      fees_oru + subsidy.
Proof.
  intros. unfold coinbase_oru_cap. lia.
Qed.

Lemma overclaim_includes_burn :
  forall fee c,
    0 <= fee ->
    0 < calculate_fee_burn fee c ->
    fee > calculate_miner_fee fee c.
Proof.
  intros fee c Hf Hb.
  unfold calculate_miner_fee.
  lia.
Qed.

(** * Determinism *)

(** The burn amount is a pure function of [(fee, class)]. Height, nBits,
    and any other block-header field are unused: RFC-018 is genesis-active
    with no versionbits gate. *)
Definition fee_burn_in_context
  (fee : Z) (c : TxFeeClass) (_height : nat) (_nbits : Z) : Z :=
  calculate_fee_burn fee c.

Lemma fee_burn_deterministic :
  forall (fee : Z) (c : TxFeeClass) (h1 h2 : nat) (n1 n2 : Z),
    fee_burn_in_context fee c h1 n1 = fee_burn_in_context fee c h2 n2.
Proof.
  intros. unfold fee_burn_in_context. reflexivity.
Qed.

Lemma fee_burn_function_of_fee_and_class :
  forall fee c h n,
    fee_burn_in_context fee c h n = calculate_fee_burn fee c.
Proof.
  intros. reflexivity.
Qed.

Lemma same_tx_same_burn :
  forall fee c,
    calculate_fee_burn fee c = calculate_fee_burn fee c.
Proof. reflexivity. Qed.

(** * Finite golden pins (share function; RES-transfer) *)

Definition ORU_COIN : Z := 100000000.
Definition MAX_MONEY : Z := 10000000000 * ORU_COIN.

Lemma burn_pin_zero :
  oru_res_transfer_fee_burn_share 0 = 0.
Proof. reflexivity. Qed.

Lemma burn_pin_one :
  oru_res_transfer_fee_burn_share 1 = 0.
Proof. reflexivity. Qed.

Lemma burn_pin_two :
  oru_res_transfer_fee_burn_share 2 = 1.
Proof. reflexivity. Qed.

Lemma burn_pin_three :
  oru_res_transfer_fee_burn_share 3 = 1.
Proof. reflexivity. Qed.

Lemma burn_pin_seven :
  oru_res_transfer_fee_burn_share 7 = 3.
Proof. reflexivity. Qed.

Lemma burn_pin_thousand :
  oru_res_transfer_fee_burn_share 1000 = 500.
Proof. reflexivity. Qed.

Lemma burn_pin_one_oru :
  oru_res_transfer_fee_burn_share ORU_COIN = ORU_COIN / 2.
Proof. vm_compute. reflexivity. Qed.

Lemma burn_pin_tail_subsidy :
  oru_res_transfer_fee_burn_share (125 * ORU_COIN) = (125 * ORU_COIN) / 2.
Proof. vm_compute. reflexivity. Qed.

Lemma burn_pin_max_money :
  oru_res_transfer_fee_burn_share MAX_MONEY = MAX_MONEY / 2.
Proof. vm_compute. reflexivity. Qed.

Lemma burn_pin_max_money_odd :
  oru_res_transfer_fee_burn_share (MAX_MONEY - 1) = (MAX_MONEY - 1) / 2.
Proof. vm_compute. reflexivity. Qed.

Lemma conservation_pins :
  ORU_burn 0 + ORU_miner_fee 0 = 0
  /\ ORU_burn 1 + ORU_miner_fee 1 = 1
  /\ ORU_burn 3 + ORU_miner_fee 3 = 3
  /\ ORU_burn MAX_MONEY + ORU_miner_fee MAX_MONEY = MAX_MONEY.
Proof.
  unfold ORU_burn, ORU_miner_fee.
  vm_compute.
  repeat split; reflexivity.
Qed.

Lemma oru_only_pin_no_burn :
  calculate_fee_burn 1000 TxFeeORUOnly = 0
  /\ calculate_miner_fee 1000 TxFeeORUOnly = 1000.
Proof. split; reflexivity. Qed.

Lemma res_transfer_pin_half :
  calculate_fee_burn 1000 TxFeeResTransfer = 500
  /\ calculate_miner_fee 1000 TxFeeResTransfer = 500.
Proof. split; reflexivity. Qed.

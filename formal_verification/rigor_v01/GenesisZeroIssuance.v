(** RIGOR v0.1 — Genesis ORU zero issuance (P0-037 model; P0-038 landed).

    GenesisReward(0) = 0
    CumulativeIssuance(0) = 0
    h >= 1  →  existing frozen ORU schedule (ORU_INITIAL_BLOCK_SUBSIDY >> shifts)

    C++ [GetBlockSubsidy(0)] returns 0 as of P0-038. This module is still not a
    [ConnectBlock] refinement.

      cpp_refinement_proved = false
      CPP_REFINEMENT = NOT_PROVED

    Compatible with P0-036 [genesis_supply] (all zeros) and [apply_delta].
    Does not change W, T, RES W(0), maturity, or the h>=1 ORU schedule.

    No [Axiom] / [Admitted] / [Parameter]. *)
From Coq Require Import List.
From Coq Require Import Bool.Bool.
From Coq Require Import ZArith.ZArith.
From Coq Require Import Lia.
Import ListNotations.
Require Import RigorV01.MintSupplyConservation.
Require Import RigorV01.ConnectBlockSupply.
Require Import RigorV01.WindowGeneric.
Require Import RigorV01.WindowSpecialization.
From Rigor Require Import ResSchedule.

Local Open Scope Z_scope.

Definition cpp_refinement_proved : bool := false.

Theorem genesis_zero_cpp_refinement_not_proved :
  cpp_refinement_proved = false.
Proof. reflexivity. Qed.

(** * Landed C++ schedule image (P0-038)

    Mirrors [GetBlockSubsidy]: height 0 is 0; height >= 1 uses nBlockSubsidyBase
    = 1000 ORU, interval 210000, max 3 shifts. *)

Definition ORU_INITIAL_SATS : Z := 100000000000. (* 1000 ORU *)

Definition current_cpp_oru_subsidy (h : Z) : Z :=
  if h =? 0 then 0 else Z.shiftr ORU_INITIAL_SATS (Z.min (h / 210000) 3).

Definition proposed_oru_issuance (h : Z) : Z := current_cpp_oru_subsidy h.

Definition GenesisReward (h : Z) : Z := proposed_oru_issuance h.

Theorem genesis_reward_zero :
  GenesisReward 0 = 0.
Proof. reflexivity. Qed.

Theorem current_cpp_genesis_is_zero :
  current_cpp_oru_subsidy 0 = 0.
Proof. reflexivity. Qed.

(** Height >= 1 is the frozen schedule, not a doubled "compensation" subsidy. *)
Theorem proposed_agrees_after_genesis :
  forall h : Z, 0 < h -> proposed_oru_issuance h = current_cpp_oru_subsidy h.
Proof. intros. reflexivity. Qed.

Theorem height_1_reward_unchanged :
  GenesisReward 1 = ORU_INITIAL_SATS.
Proof. reflexivity. Qed.

Theorem height_2_reward_unchanged :
  GenesisReward 2 = ORU_INITIAL_SATS.
Proof. reflexivity. Qed.

Theorem no_height_1_compensation :
  GenesisReward 1 <> ORU_INITIAL_SATS + ORU_INITIAL_SATS.
Proof. discriminate. Qed.

Theorem no_height_2_compensation :
  GenesisReward 2 <> ORU_INITIAL_SATS + ORU_INITIAL_SATS.
Proof. discriminate. Qed.

(** * Cumulative issuance *)

Fixpoint cumulative_issuance (n : nat) : Z :=
  match n with
  | O => GenesisReward 0
  | S k => cumulative_issuance k + GenesisReward (Z.of_nat (S k))
  end.

Theorem genesis_supply_zero_cumulative :
  cumulative_issuance 0%nat = 0.
Proof. reflexivity. Qed.

Theorem cumulative_height_1_is_one_subsidy :
  cumulative_issuance 1%nat = ORU_INITIAL_SATS.
Proof. reflexivity. Qed.

Theorem cumulative_height_2_is_two_subsidies :
  cumulative_issuance 2%nat = ORU_INITIAL_SATS + ORU_INITIAL_SATS.
Proof. reflexivity. Qed.

(** * Unified supply: genesis delta is the zero vector *)

Definition genesis_block_delta : BlockDelta :=
  ordinary_delta 0 0 0 0.

Lemma genesis_zero_delta_admissible :
  delta_admissible genesis_block_delta = true.
Proof. vm_compute. reflexivity. Qed.

Theorem genesis_has_no_oru_issuance :
  bd_subsidy genesis_block_delta = 0 /\
  bd_valid_mint genesis_block_delta = 0 /\
  bd_converge genesis_block_delta = 0 /\
  bd_fee_burn genesis_block_delta = 0.
Proof. repeat split; reflexivity. Qed.

Theorem genesis_apply_leaves_zero_supply :
  apply_delta genesis_supply genesis_block_delta = Some genesis_supply.
Proof.
  unfold apply_delta.
  rewrite genesis_zero_delta_admissible.
  reflexivity.
Qed.

Theorem genesis_supply_zero :
  us_ledger_oru genesis_supply = 0 /\
  us_circulating_oru genesis_supply = 0 /\
  us_res genesis_supply = 0 /\
  us_fee_burn_acc genesis_supply = 0.
Proof. repeat split; reflexivity. Qed.

Theorem genesis_unified_supply_stays_zero :
  forall s,
    apply_delta genesis_supply genesis_block_delta = Some s ->
    us_ledger_oru s = 0 /\ us_circulating_oru s = 0 /\ us_res s = 0.
Proof.
  intros s H.
  rewrite genesis_apply_leaves_zero_supply in H.
  inversion H. subst s.
  repeat split; reflexivity.
Qed.

(** P0-036 conservation at genesis: Supply(0) = 0 + 0 + 0 - 0 - 0. *)
Theorem genesis_conservation_zero :
  forall s,
    apply_delta genesis_supply genesis_block_delta = Some s ->
    total_circulating s =
      total_circulating genesis_supply
        + bd_subsidy genesis_block_delta
        + bd_valid_mint genesis_block_delta
        - bd_converge genesis_block_delta
        - bd_fee_burn genesis_block_delta.
Proof.
  intros s H. apply mint_supply_conservation. exact H.
Qed.

Definition genesis_abstract_block : AbstractBlock :=
  {| ab_height := 0;
     ab_subsidy := 0;
     ab_valid_mint := 0;
     ab_converge := 0;
     ab_fee_burn := 0;
     ab_ordinary_only := true;
     ab_mint_authorized := false;
     ab_txs_valid := true |}.

Theorem genesis_connectblock_supply_zero :
  apply_block_supply genesis_supply genesis_abstract_block = Some genesis_supply.
Proof.
  unfold apply_block_supply, abstract_to_delta, genesis_abstract_block.
  exact genesis_apply_leaves_zero_supply.
Qed.

(** Height 1 applies one frozen subsidy, not a 2000-ORU catch-up. *)
Definition height1_block_delta : BlockDelta :=
  ordinary_delta 1 ORU_INITIAL_SATS 0 0.

Lemma get_res_mint_reward_height_1 :
  get_res_mint_reward 1 = RES_REWARD_GENESIS.
Proof. vm_compute. reflexivity. Qed.

Lemma height1_delta_admissible :
  delta_admissible height1_block_delta = true.
Proof. vm_compute. reflexivity. Qed.

Theorem height1_apply_issues_one_subsidy :
  forall s,
    apply_delta genesis_supply height1_block_delta = Some s ->
    us_ledger_oru s = ORU_INITIAL_SATS /\
    us_circulating_oru s = ORU_INITIAL_SATS.
Proof.
  intros s H.
  unfold apply_delta in H.
  rewrite height1_delta_admissible in H.
  inversion H. subst s. simpl. split; reflexivity.
Qed.

(** * No hidden premine / allocation / mint in the proposed genesis construction *)

Record GenesisAllocation := {
  ga_coinbase_oru : Z;
  ga_treasury_oru : Z;
  ga_utxo_extra_oru : Z;
  ga_mint_oru : Z
}.

Definition genesis_has_no_oru_allocation (g : GenesisAllocation) : bool :=
  (ga_coinbase_oru g =? 0) &&
  (ga_treasury_oru g =? 0) &&
  (ga_utxo_extra_oru g =? 0) &&
  (ga_mint_oru g =? 0).

Definition proposed_clean_genesis : GenesisAllocation :=
  {| ga_coinbase_oru := 0;
     ga_treasury_oru := 0;
     ga_utxo_extra_oru := 0;
     ga_mint_oru := 0 |}.

Theorem genesis_has_no_oru_allocation_proposed :
  genesis_has_no_oru_allocation proposed_clean_genesis = true.
Proof. reflexivity. Qed.

(** * Negative predicates — illegal genesis states *)

Definition genesis_reward_nonzero (r : Z) : bool := negb (r =? 0).

Theorem landed_genesis_reward_is_zero :
  genesis_reward_nonzero (current_cpp_oru_subsidy 0) = false.
Proof. reflexivity. Qed.

Definition genesis_hidden_allocation (g : GenesisAllocation) : bool :=
  negb (genesis_has_no_oru_allocation g).

Theorem zero_coinbase_plus_treasury_is_hidden_premine :
  genesis_hidden_allocation
    {| ga_coinbase_oru := 0;
       ga_treasury_oru := ORU_INITIAL_SATS;
       ga_utxo_extra_oru := 0;
       ga_mint_oru := 0 |} = true.
Proof. reflexivity. Qed.

Definition genesis_mint_nonzero (m : Z) : bool := negb (m =? 0).

Theorem genesis_mint_must_be_zero :
  genesis_mint_nonzero 0 = false.
Proof. reflexivity. Qed.

Definition genesis_supply_mismatch (ledger circulating : Z) : bool :=
  negb (ledger =? circulating).

(** Landed C++: GetBlockSubsidy(0)=0 and genesis coinbase is skipped, so
    ledger and circulating UTXO both start at 0. *)
Theorem current_ledger_utxo_aligned :
  genesis_supply_mismatch (current_cpp_oru_subsidy 0) 0 = false.
Proof. reflexivity. Qed.

Theorem proposed_ledger_utxo_aligned :
  genesis_supply_mismatch (GenesisReward 0) 0 = false.
Proof. reflexivity. Qed.

(** * W / T / RES / maturity are not functions of Genesis ORU *)

Theorem W32_impact_none :
  WindowSpecialization.PRODUCTION_W = 32.
Proof. reflexivity. Qed.

Theorem maturity_impact_none :
  WindowGeneric.COINBASE_MATURITY = 100.
Proof. reflexivity. Qed.

Theorem res_w0_impact_none :
  get_res_mint_reward 0 = RES_REWARD_GENESIS.
Proof. exact reward_at_height_zero. Qed.

Theorem res_reward_not_used_as_oru_compensation :
  GenesisReward 1 = ORU_INITIAL_SATS /\
  get_res_mint_reward 1 = RES_REWARD_GENESIS.
Proof.
  split.
  - reflexivity.
  - exact get_res_mint_reward_height_1.
Qed.

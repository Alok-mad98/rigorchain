(** RIGOR v0.1 — abstract ConnectBlock supply interface (P0-036).

    Intended refinement (NOT PROVED):

      C++ ConnectBlock(block, state)
            ↓  monetary slice
      apply_block_supply (abstract_of block) state

    This module *names* the obligation and implements the Coq transition.
    P0-040 adds a named C++ field map ([CppConnectBlockSlice]).
    It does not identify validation.cpp with apply_block_supply.

      cpp_refinement_proved = false

    Frozen production rules are not modified. No [Axiom] / [Admitted]. *)
From Coq Require Import List.
From Coq Require Import Bool.Bool.
From Coq Require Import ZArith.ZArith.
From Coq Require Import Lia.
Import ListNotations.
Require Import RigorV01.MintSupplyConservation.
Require Import RigorV01.ResMintCorrectness.
From Rigor Require Import ResSchedule.

Local Open Scope Z_scope.

Definition cpp_refinement_proved : bool := false.

Theorem connectblock_refinement_not_proved :
  cpp_refinement_proved = false.
Proof. reflexivity. Qed.

(** * Abstract block — the monetary fields ConnectBlock must feed the model *)

Record AbstractBlock := {
  ab_height : Z;
  ab_subsidy : Z;
  ab_valid_mint : Z;
  ab_converge : Z;
  ab_fee_burn : Z;
  ab_ordinary_only : bool;
  ab_mint_authorized : bool;
  ab_txs_valid : bool
}.

Definition abstract_to_delta (b : AbstractBlock) : BlockDelta :=
  {| bd_height := ab_height b;
     bd_subsidy := ab_subsidy b;
     bd_valid_mint := ab_valid_mint b;
     bd_converge := ab_converge b;
     bd_fee_burn := ab_fee_burn b;
     bd_ordinary := ab_ordinary_only b;
     bd_mint_authorized := ab_mint_authorized b;
     bd_has_invalid := negb (ab_txs_valid b) |}.

Definition apply_block_supply (s : UnifiedSupply) (b : AbstractBlock)
  : option UnifiedSupply :=
  apply_delta s (abstract_to_delta b).

(** Future refinement hole (unimplemented; not a Coq Parameter):
    extract AbstractBlock from CBlock + ConnectBlock accumulators, then
    prove C++ ConnectBlock monetary slice = [apply_block_supply]. *)

Definition ConnectBlock_supply_obligation
    (cpp_apply : UnifiedSupply -> AbstractBlock -> option UnifiedSupply) : Prop :=
  forall s b, cpp_apply s b = apply_block_supply s b.

Theorem apply_block_supply_is_apply_delta :
  forall s b,
    apply_block_supply s b = apply_delta s (abstract_to_delta b).
Proof. intros. reflexivity. Qed.

Theorem connectblock_invalid_tx_rejected :
  forall s b,
    ab_txs_valid b = false ->
    apply_block_supply s b = None.
Proof.
  intros s b H.
  unfold apply_block_supply, abstract_to_delta.
  apply invalid_tx_cannot_enter_supply. simpl. rewrite H. reflexivity.
Qed.

Theorem connectblock_unauthorized_mint_rejected :
  forall s b,
    ab_mint_authorized b = false ->
    ab_valid_mint b <> 0 ->
    apply_block_supply s b = None.
Proof.
  intros s b Ha Hnz.
  unfold apply_block_supply, abstract_to_delta.
  apply unauthorized_mint_rejected; simpl; assumption.
Qed.

Theorem connectblock_ordinary_cannot_mint :
  forall s b s',
    apply_block_supply s b = Some s' ->
    ab_ordinary_only b = true ->
    us_res s' = us_res s.
Proof.
  intros s b s' H Ho.
  unfold apply_block_supply in H.
  eapply ordinary_tx_cannot_mint; [exact H | simpl; exact Ho].
Qed.

Theorem connectblock_conservation :
  forall s b s',
    apply_block_supply s b = Some s' ->
    total_circulating s' =
      total_circulating s + ab_subsidy b + ab_valid_mint b
        - ab_converge b - ab_fee_burn b.
Proof.
  intros s b s' H.
  unfold apply_block_supply in H.
  apply mint_supply_conservation in H. simpl in H. exact H.
Qed.

Theorem connectblock_fee_burn_not_circulating :
  forall s b s',
    apply_block_supply s b = Some s' ->
    us_circulating_oru s' =
      us_circulating_oru s + ab_subsidy b - ab_converge b - ab_fee_burn b.
Proof.
  intros s b s' H.
  unfold apply_block_supply in H.
  apply fee_burn_does_not_enter_circulating in H. simpl in H. exact H.
Qed.

Theorem connectblock_maturity_still_100 :
  coinbase_maturity = 100.
Proof. reflexivity. Qed.

(** Fields that must appear in any future C++ refinement of this slice. *)
Inductive ConnectBlockSupplyField :=
| FieldSubsidy
| FieldMint
| FieldConverge
| FieldFeeBurn
| FieldOrdinaryTx
| FieldInvalidTx
| FieldMaturity
| FieldTotalSupply.

Definition connectblock_supply_fields : list ConnectBlockSupplyField :=
  [FieldSubsidy; FieldMint; FieldConverge; FieldFeeBurn;
   FieldOrdinaryTx; FieldInvalidTx; FieldMaturity; FieldTotalSupply].

Theorem connectblock_supply_fields_complete :
  length connectblock_supply_fields = 8%nat.
Proof. reflexivity. Qed.

(** * P0-040 C++ field map (validation.cpp ConnectBlock monetary slice)

    Named correspondence, not VST:

      GetBlockSubsidy(h)              → ab_subsidy
      res_mint_coinbase
        (written m_block_res_mint_sum) → ab_valid_mint
      block_converge_sum
        (written m_block_converge_sum) → ab_converge
      nFeesOruResTransferBurn          → ab_fee_burn

    Observational GetChainOruTotal writes parent + subsidy − converge
    (RFC-002 ledger). Circulating subtracts fee burn (RFC-018). *)

Record CppConnectBlockSlice := {
  cpp_height : Z;
  cpp_GetBlockSubsidy : Z;
  cpp_res_mint_coinbase : Z;
  cpp_block_converge_sum : Z;
  cpp_nFeesOruResTransferBurn : Z;
  cpp_res_mint_authorized : bool;
  cpp_ordinary_only : bool;
  cpp_txs_valid : bool
}.

Definition cpp_slice_to_abstract (s : CppConnectBlockSlice) : AbstractBlock :=
  {| ab_height := cpp_height s;
     ab_subsidy := cpp_GetBlockSubsidy s;
     ab_valid_mint := cpp_res_mint_coinbase s;
     ab_converge := cpp_block_converge_sum s;
     ab_fee_burn := cpp_nFeesOruResTransferBurn s;
     ab_ordinary_only := cpp_ordinary_only s;
     ab_mint_authorized := cpp_res_mint_authorized s;
     ab_txs_valid := cpp_txs_valid s |}.

Definition apply_cpp_connectblock_slice (st : UnifiedSupply) (s : CppConnectBlockSlice)
  : option UnifiedSupply :=
  apply_block_supply st (cpp_slice_to_abstract s).

Theorem cpp_slice_is_apply_block_supply :
  forall st s,
    apply_cpp_connectblock_slice st s =
    apply_block_supply st (cpp_slice_to_abstract s).
Proof. intros. reflexivity. Qed.

(** Observational ledger (C++ GetChainOruTotal) does not fold fee burn. *)
Definition cpp_observational_oru_delta (s : CppConnectBlockSlice) : Z :=
  cpp_GetBlockSubsidy s - cpp_block_converge_sum s.

Theorem connectblock_observational_oru_matches_ledger :
  forall st s st',
    apply_cpp_connectblock_slice st s = Some st' ->
    us_ledger_oru st' = us_ledger_oru st + cpp_observational_oru_delta s.
Proof.
  intros st s st' H.
  unfold apply_cpp_connectblock_slice in H.
  pose proof (converge_reduces_oru st (abstract_to_delta (cpp_slice_to_abstract s)) st' H) as Hr.
  simpl in Hr. unfold cpp_observational_oru_delta. lia.
Qed.

Theorem connectblock_fee_burn_not_in_observational_ledger :
  forall st s st',
    apply_cpp_connectblock_slice st s = Some st' ->
    us_ledger_oru st' =
      us_ledger_oru st + cpp_GetBlockSubsidy s - cpp_block_converge_sum s.
Proof.
  intros st s st' H.
  apply connectblock_observational_oru_matches_ledger in H.
  unfold cpp_observational_oru_delta in H.
  lia.
Qed.

Theorem cpp_slice_valid_res_mint :
  forall st s st',
    apply_cpp_connectblock_slice st s = Some st' ->
    us_res st' = us_res st + cpp_res_mint_coinbase s.
Proof.
  intros st s st' H.
  unfold apply_cpp_connectblock_slice in H.
  apply fee_burn_does_not_reduce_res in H. simpl in H. exact H.
Qed.

Theorem cpp_slice_converge :
  forall st s st',
    apply_cpp_connectblock_slice st s = Some st' ->
    us_ledger_oru st' =
      us_ledger_oru st + cpp_GetBlockSubsidy s - cpp_block_converge_sum s.
Proof. apply connectblock_fee_burn_not_in_observational_ledger. Qed.

Theorem cpp_slice_fee_burn_circulating :
  forall st s st',
    apply_cpp_connectblock_slice st s = Some st' ->
    us_circulating_oru st' =
      us_circulating_oru st + cpp_GetBlockSubsidy s
        - cpp_block_converge_sum s - cpp_nFeesOruResTransferBurn s.
Proof.
  intros st s st' H.
  unfold apply_cpp_connectblock_slice in H.
  apply fee_burn_does_not_enter_circulating in H. simpl in H. exact H.
Qed.

Theorem cpp_slice_maturity_100 :
  coinbase_maturity = 100.
Proof. exact connectblock_maturity_still_100. Qed.

Theorem cpp_slice_invalid_tx_rejected :
  forall st s,
    cpp_txs_valid s = false ->
    apply_cpp_connectblock_slice st s = None.
Proof.
  intros st s H.
  unfold apply_cpp_connectblock_slice.
  apply connectblock_invalid_tx_rejected. exact H.
Qed.

Theorem cpp_slice_unauthorized_mint_rejected :
  forall st s,
    cpp_res_mint_authorized s = false ->
    cpp_res_mint_coinbase s <> 0 ->
    apply_cpp_connectblock_slice st s = None.
Proof.
  intros st s Ha Hnz.
  unfold apply_cpp_connectblock_slice.
  apply connectblock_unauthorized_mint_rejected; assumption.
Qed.

Theorem cpp_slice_ordinary_cannot_mint :
  forall st s st',
    apply_cpp_connectblock_slice st s = Some st' ->
    cpp_ordinary_only s = true ->
    us_res st' = us_res st.
Proof.
  intros st s st' H Ho.
  unfold apply_cpp_connectblock_slice in H.
  eapply connectblock_ordinary_cannot_mint; eauto.
Qed.

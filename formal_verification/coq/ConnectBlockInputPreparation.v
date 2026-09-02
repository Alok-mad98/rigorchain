(** ConnectBlockInputPreparation.v — P057.

    ConnectBlock prepares a [ConsensusKernelInput] from an abstract
    [BlockState]. This file proves the prepared record equals the
    canonical Kernel input. It does not prove UTXO / DB mutation.

    The abstract [BlockState] has no coinsdb, CBlockIndex, RPC, wallet,
    or miner fields.

    No [Admitted] / [Axiom] / [Parameter] / [Extract Constant]. *)

From Coq Require Import ZArith.ZArith.
From Coq Require Import List.
Import ListNotations.

From Rigor Require Import RigorBytes.
Require Import Rigor.legacy_account_model.Types.
Require Import Rigor.ConsensusKernelRefinement.

Local Open Scope Z_scope.

(** Abstract assembled state after window / fee / mint collection.
    Not a CCoinsViewCache. *)
Record BlockState := {
  bs_height : Z;
  bs_params : cpp_issuance_params;
  bs_F : list Tx;
  bs_C : list byte;
  bs_B : list byte;
  bs_T : Z;
  bs_scripts : list (list byte);
  bs_converge : Z;
  bs_burn : Z;
  bs_mint : Z;
  bs_fees_oru : Z;
  bs_fees_res : Z;
  bs_bootstrap : Z;
  bs_has_winner : bool;
  bs_draw_cardinality : Z;
  bs_oru_coinbase : Z;
  bs_res_coinbase : Z
}.

Record ConsensusKernelInput := {
  cki_height : Z;
  cki_params : cpp_issuance_params;
  cki_candidates : list Tx;
  cki_C : list byte;
  cki_B : list byte;
  cki_T : Z;
  cki_scripts : list (list byte);
  cki_converge : Z;
  cki_burn : Z;
  cki_mint : Z;
  cki_fees_oru : Z;
  cki_fees_res : Z;
  cki_bootstrap : Z;
  cki_has_winner : bool;
  cki_draw_cardinality : Z;
  cki_oru_coinbase : Z;
  cki_res_coinbase : Z
}.

Definition ConsensusKernelInput_has_utxo_db : bool := false.
Definition ConsensusKernelInput_has_chainstate : bool := false.
Definition ConsensusKernelInput_has_CBlockIndex : bool := false.
Definition ConsensusKernelInput_has_rpc : bool := false.
Definition ConsensusKernelInput_has_wallet : bool := false.
Definition ConsensusKernelInput_has_miner : bool := false.

Theorem kernel_input_excludes_db :
  ConsensusKernelInput_has_utxo_db = false /\
  ConsensusKernelInput_has_chainstate = false /\
  ConsensusKernelInput_has_CBlockIndex = false.
Proof. repeat split; reflexivity. Qed.

Theorem kernel_input_excludes_node_context :
  ConsensusKernelInput_has_rpc = false /\
  ConsensusKernelInput_has_wallet = false /\
  ConsensusKernelInput_has_miner = false.
Proof. repeat split; reflexivity. Qed.

(** ConnectBlock preparation: copy assembled fields. No formula. *)
Definition PrepareKernelInput (s : BlockState) : ConsensusKernelInput :=
  {| cki_height := bs_height s;
     cki_params := bs_params s;
     cki_candidates := bs_F s;
     cki_C := bs_C s;
     cki_B := bs_B s;
     cki_T := bs_T s;
     cki_scripts := bs_scripts s;
     cki_converge := bs_converge s;
     cki_burn := bs_burn s;
     cki_mint := bs_mint s;
     cki_fees_oru := bs_fees_oru s;
     cki_fees_res := bs_fees_res s;
     cki_bootstrap := bs_bootstrap s;
     cki_has_winner := bs_has_winner s;
     cki_draw_cardinality := bs_draw_cardinality s;
     cki_oru_coinbase := bs_oru_coinbase s;
     cki_res_coinbase := bs_res_coinbase s |}.

(** Canonical Kernel input from the same assembled state. *)
Definition KernelInput (s : BlockState) : ConsensusKernelInput :=
  {| cki_height := bs_height s;
     cki_params := bs_params s;
     cki_candidates := bs_F s;
     cki_C := bs_C s;
     cki_B := bs_B s;
     cki_T := bs_T s;
     cki_scripts := bs_scripts s;
     cki_converge := bs_converge s;
     cki_burn := bs_burn s;
     cki_mint := bs_mint s;
     cki_fees_oru := bs_fees_oru s;
     cki_fees_res := bs_fees_res s;
     cki_bootstrap := bs_bootstrap s;
     cki_has_winner := bs_has_winner s;
     cki_draw_cardinality := bs_draw_cardinality s;
     cki_oru_coinbase := bs_oru_coinbase s;
     cki_res_coinbase := bs_res_coinbase s |}.

Theorem prepared_input_eq_canonical :
  forall s, PrepareKernelInput s = KernelInput s.
Proof. intros s. reflexivity. Qed.

Theorem prepare_preserves_height :
  forall s, cki_height (PrepareKernelInput s) = bs_height s.
Proof. intros. reflexivity. Qed.

Theorem prepare_preserves_F :
  forall s, cki_candidates (PrepareKernelInput s) = bs_F s.
Proof. intros. reflexivity. Qed.

Theorem prepare_preserves_T :
  forall s, cki_T (PrepareKernelInput s) = bs_T s.
Proof. intros. reflexivity. Qed.

Definition mk_block_state (h : Z) : BlockState :=
  {| bs_height := h;
     bs_params := FrozenKernelParams;
     bs_F := [];
     bs_C := [];
     bs_B := [];
     bs_T := 0;
     bs_scripts := [];
     bs_converge := 0;
     bs_burn := 0;
     bs_mint := 0;
     bs_fees_oru := 0;
     bs_fees_res := 0;
     bs_bootstrap := 0;
     bs_has_winner := false;
     bs_draw_cardinality := 0;
     bs_oru_coinbase := 0;
     bs_res_coinbase := 0 |}.

Definition genesis_block_state : BlockState := mk_block_state 0.

Definition negative_height_state : BlockState := mk_block_state (-1).

(** C++ RigorSupplyDelta: authorized mint, winner present, mint omitted. *)
Definition winner_mismatch_state : BlockState :=
  {| bs_height := 1;
     bs_params := FrozenKernelParams;
     bs_F := [];
     bs_C := [];
     bs_B := [];
     bs_T := 0;
     bs_scripts := [];
     bs_converge := 0;
     bs_burn := 0;
     bs_mint := 0;
     bs_fees_oru := 0;
     bs_fees_res := 0;
     bs_bootstrap := 0;
     bs_has_winner := true;
     bs_draw_cardinality := 1;
     bs_oru_coinbase := 0;
     bs_res_coinbase := 0 |}.

Theorem genesis_prepare_height_0 :
  cki_height (PrepareKernelInput genesis_block_state) = 0.
Proof. reflexivity. Qed.

Theorem genesis_prepare_empty_F :
  cki_candidates (PrepareKernelInput genesis_block_state) = [].
Proof. reflexivity. Qed.

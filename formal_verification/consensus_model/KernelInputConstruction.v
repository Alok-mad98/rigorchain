(** ConsensusModel.KernelInputConstruction

    Complete KernelInputV2 construction from Block / UTXO / ValidationResult.
    Field sources are named below. Not UnifiedTransition.KernelInput.
    Not C++ marshalling. Not extraction.

    CPP_KERNELINPUT_EQUIVALENCE = NOT PROVED
*)
From Coq Require Import List.
From Coq Require Import ZArith.ZArith.
From ConsensusModel Require Import TransactionModel.
From ConsensusModel Require Import BlockModel.
From ConsensusModel Require Import UTXOState.
From ConsensusModel Require Import ValidationResult.
From ConsensusModel Require Import ValidationModel.
From ConsensusModel Require Import KernelInputV2.
Import ListNotations.

Local Open Scope Z_scope.

(** Amount summary: outgoing is the block's output nValues.
    Incoming is looked-up UTXO nValues. Fees = incoming - outgoing.
    Subsidy / burn / resource mint are not computed here (remain 0
    on the ExistingKernelInput view). *)
Fixpoint zsum (xs : list Z) : Z :=
  match xs with
  | nil => 0
  | x :: rest => x + zsum rest
  end.

Definition tx_outgoing (tx : Transaction) : Z :=
  zsum (map amount (outputs tx)).

Definition input_value (u : UTXOState) (i : TxInput) : Z :=
  match lookup u (previous_outpoint i) with
  | None => 0
  | Some o => amount o
  end.

Definition tx_incoming (tx : Transaction) (u : UTXOState) : Z :=
  zsum (map (input_value u) (inputs tx)).

Fixpoint txs_outgoing (txs : list Transaction) : Z :=
  match txs with
  | nil => 0
  | tx :: rest => tx_outgoing tx + txs_outgoing rest
  end.

Fixpoint txs_incoming (txs : list Transaction) (u : UTXOState) : Z :=
  match txs with
  | nil => 0
  | tx :: rest => tx_incoming tx u + txs_incoming rest u
  end.

Definition block_outgoing (b : Block) : Z :=
  txs_outgoing (transactions b).

Definition block_incoming (b : Block) (u : UTXOState) : Z :=
  txs_incoming (transactions b) u.

Definition block_fees (b : Block) (u : UTXOState) : Z :=
  block_incoming b u - block_outgoing b.

Record AmountSummary := {
  as_incoming : Z;
  as_outgoing : Z;
  as_fees : Z
}.

Definition amount_summary (b : Block) (u : UTXOState) : AmountSummary :=
  {| as_incoming := block_incoming b u;
     as_outgoing := block_outgoing b;
     as_fees := block_fees b u |}.

Lemma amount_summary_fees :
  forall b u,
    as_fees (amount_summary b u) =
    as_incoming (amount_summary b u) - as_outgoing (amount_summary b u).
Proof.
  intros b u.
  reflexivity.
Qed.

(** make_kernel_input_v2 field sources:
      kiv2_block    : Block argument
      kiv2_utxo     : UTXOState argument
      kiv2_result   : ValidationResult (tx/inputs/script/amount flags)
      kiv2_pow_ok   : pow_ok argument (header PoW; not CheckProofOfWork)
      kiv2_asert_ok : asert_ok argument (ASERT nBits; not C++ ASERT)
    Fees / amount summary live on [amount_summary], not on KernelInputV2. *)
Definition make_kernel_input_v2
  (b : Block) (u : UTXOState) (r : ValidationResult)
  (pow_ok asert_ok : bool) : KernelInputV2 :=
  {| kiv2_block := b;
     kiv2_utxo := u;
     kiv2_result := r;
     kiv2_pow_ok := pow_ok;
     kiv2_asert_ok := asert_ok |}.

Definition make_kernel_input_v2_from_block
  (b : Block) (u : UTXOState) (pow_ok asert_ok : bool) : KernelInputV2 :=
  make_kernel_input_v2 b u (validate_block b u) pow_ok asert_ok.

(** ExistingKernelInput view with explicit amount/fee/flag sources.
    Does not replace UnifiedTransition.Record KernelInput. *)
Definition existing_kernel_input_of
  (k : KernelInputV2) (b : Block) (u : UTXOState) : ExistingKernelInput :=
  {| ki_utxo_ok := inputs_valid (kiv2_result k);
     ki_incoming := as_incoming (amount_summary b u);
     ki_outgoing := as_outgoing (amount_summary b u);
     ki_fees := as_fees (amount_summary b u);
     ki_burn := 0;
     ki_subsidy := 0;
     ki_res_mint := 0;
     ki_script_ok := script_valid (kiv2_result k);
     ki_pow_ok := kiv2_pow_ok k;
     ki_asert_ok := kiv2_asert_ok k |}.

Theorem accepted_block_kernel_input_consistent :
  forall (b : Block) (u : UTXOState) (pow_ok asert_ok : bool),
    let r := validate_block b u in
    accepted_validation r ->
    let k := make_kernel_input_v2 b u r pow_ok asert_ok in
    let sm := amount_summary b u in
    let e := existing_kernel_input_of k b u in
    kiv2_block k = b /\
    kiv2_utxo k = u /\
    kiv2_result k = r /\
    kiv2_pow_ok k = pow_ok /\
    kiv2_asert_ok k = asert_ok /\
    tx_valid (kiv2_result k) = tx_valid r /\
    inputs_valid (kiv2_result k) = inputs_valid r /\
    script_valid (kiv2_result k) = script_valid r /\
    amount_valid (kiv2_result k) = amount_valid r /\
    as_fees sm = as_incoming sm - as_outgoing sm /\
    ki_pow_ok e = pow_ok /\
    ki_asert_ok e = asert_ok /\
    ki_utxo_ok e = inputs_valid r /\
    ki_script_ok e = script_valid r /\
    ki_fees e = as_fees sm /\
    ki_incoming e = as_incoming sm /\
    ki_outgoing e = as_outgoing sm.
Proof.
  intros b u pow_ok asert_ok Hacc.
  unfold make_kernel_input_v2, amount_summary, existing_kernel_input_of.
  simpl.
  repeat split; reflexivity.
Qed.

(** ConsensusModel.ValidationResult

    Result flags after object-level checks. These bools are *outcomes*,
    not substitutes for Block / Transaction / UTXOState.

    C++ correspondence (do not edit these functions in this TASK):
      tx_valid      ~ CheckTransaction
      inputs_valid  ~ Consensus::CheckTxInputs
      script_valid  ~ VerifyWitnessProgram / CheckInputScripts
      amount_valid  ~ CheckTransaction amount / AddMoneyOverflows / MAX_MONEY

    NOT CLAIMED: soundness, ConnectBlock, KernelInput marshalling.
*)
From Coq Require Import Bool.Bool.

Record ValidationResult := {
  tx_valid : bool;
  inputs_valid : bool;
  script_valid : bool;
  amount_valid : bool
}.

Definition validation_all_ok (r : ValidationResult) : bool :=
  andb (tx_valid r)
    (andb (inputs_valid r)
      (andb (script_valid r) (amount_valid r))).

Definition accepted_validation (r : ValidationResult) : Prop :=
  validation_all_ok r = true.

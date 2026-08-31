(** ConnectBlock.v — F-1 (TASK-P0-016).

    Schedule constants only. The PHASE-3 account-model step that *skipped*
    a mint on cap overshoot lives in
    [legacy_account_model/ConnectBlockScaffold.v] and must not be used as
    ConnectBlock. Production and [apply_block] reject overshoot.

    Monetary proofs: [ResMintObligation.cap_overshoot_rejects_apply_block].
    Draw: [Execution.Execute] / [Selection.select_winner]. *)
Require Import Coq.ZArith.ZArith.
From Rigor Require Import ResSchedule.

Definition RES_MINT_AMOUNT (height : Z) : Z := get_res_mint_reward height.
Definition MAX_RES_SUPPLY : Z := RES_MAX_SUPPLY.

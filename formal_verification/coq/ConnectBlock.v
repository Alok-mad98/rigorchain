(** ConnectBlock.v — F-1 (TASK-P0-016).

    Schedule constants only. The PHASE-3 account-model step that *skipped*
    a mint on cap overshoot lives in
    [legacy_account_model/ConnectBlockScaffold.v] and must not be used as
    ConnectBlock. Production and [apply_block] reject overshoot.

    Monetary proofs: [ResSchedule.cap_is_upper_bound].
    RFC-020: there is no per-block RES grant. *)
Require Import Coq.ZArith.ZArith.
From Rigor Require Import ResSchedule.

Definition RES_DESTROY_COST (height : Z) : Z := res_destroy_cost_at_height height.
Definition RES_MINT_AMOUNT (height : Z) : Z := 0.
Definition MAX_RES_SUPPLY : Z := RES_MAX_SUPPLY.

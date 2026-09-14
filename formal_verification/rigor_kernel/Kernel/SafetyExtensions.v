(** RigorKernel.Kernel.SafetyExtensions — P0-120 low-cost safety corollaries.

    Scope: Coq consensus kernel only.

    Closed safety extensions that do not need a network model,
    liveness, cryptographic primitives, or a refinement:

      1. Intra-block duplicate-input rejection
         ([apply_block_accepted_inputs_unique] /
          [apply_block_rejects_duplicate_input]).
      2. Per-mint RES cap by the genesis schedule value
         ([res_mint_bounded_by_genesis]).
      3. Schedule-side aggregate cap
         ([res_total_issuance_capped] in Supply/SupplySchedule.v).

    These are local / schedule invariants. They are not reorg
    common-prefix safety, not S_RES runtime rejection, and not
    C++ ConnectBlock equivalence.

    NOT CLAIMED:
      C++ implementation equivalence
      Full chain validity
      Reorg / common-prefix / finality
      Liveness / fairness
      Cryptographic security
      Full extraction refinement
      Production-code refinement
      Runtime S_RES reject (C-3)
*)
From Coq Require Import List.
From Coq Require Import Lia.
From Coq Require Import ZArith.ZArith.
From RigorKernel.proofs.UTXO_Soundness Require Import UTXOSoundness.
From RigorKernel Require Import Supply.SupplySchedule.
From RigorKernel.proofs.RES_Issuance Require Import RESIssuance.
From RigorKernel Require Import Kernel.RESValidator.

Local Open Scope Z_scope.

(** * Intra-block duplicate-input rejection

    [apply_block] already refuses a collected-input list that is
    not duplicate-free. The two-transaction special case
    [apply_block_no_double_spend] is kept; these theorems are the
    general statement for an arbitrary block.

    Main reader-facing fact: an accepted block has unique inputs.
    The contrapositive is [apply_block_rejects_duplicate_input].

    This is a local conflict check on one candidate block. It is
    not a theorem about two tips, discarded branches, or k-deep
    common prefix. *)

Theorem apply_block_accepted_inputs_unique :
  forall u b u',
    apply_block u b = Some u' ->
    NoDup (collect_inputs b).
Proof.
  intros u b u' H.
  unfold apply_block in H.
  destruct (nodupb (collect_inputs b)) eqn:Hb.
  - apply nodupb_ok. exact Hb.
  - discriminate.
Qed.

Theorem apply_block_rejects_duplicate_input :
  forall u b,
    ~ NoDup (collect_inputs b) ->
    apply_block u b = None.
Proof.
  intros u b Hnd.
  unfold apply_block.
  destruct (nodupb (collect_inputs b)) eqn:Hb.
  - apply nodupb_ok in Hb. contradiction.
  - reflexivity.
Qed.

(** * Per-mint RES cap

    [res_created] already bounds the mint by [RES_MAX_SUPPLY]. That
    is the derived cap, not a per-block reward.

    This is not a running [S_RES] reject (C-3 / OP-D1 remains open
    by design). The aggregate schedule cap is
    [res_total_issuance_capped]. *)

Theorem res_mint_bounded_by_genesis :
  forall tx,
    res_created tx ->
    0 < mint_amount tx <= RES_MAX_SUPPLY.
Proof.
  intros tx Hc.
  unfold res_created in Hc.
  destruct Hc as [_ [_ [Hpos [Hcap _]]]].
  split; [exact Hpos | exact Hcap].
Qed.

(** Executable form: a passing [check_res] implies the same per-mint
    bounds, including the strict lower bound. *)
Theorem check_res_mint_bounded_by_genesis :
  forall s,
    check_res s = true ->
    0 < mint_amount s <= RES_MAX_SUPPLY.
Proof.
  intros s H.
  apply check_res_reflect in H.
  apply res_mint_bounded_by_genesis in H.
  exact H.
Qed.

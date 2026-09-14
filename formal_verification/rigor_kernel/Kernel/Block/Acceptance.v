(** Block acceptance as model composition.

    UTXO transition + PoW predicate + supply schedule + script class.
    Not a node; not ConnectBlock.

    NOT CLAIMED: P2P, reorg, C++ refinement, binary equivalence.
*)
From Coq Require Import List.
From Coq Require Import ZArith.ZArith.
From RigorKernel.proofs.UTXO_Soundness Require Import UTXOSoundness.
From RigorKernel.proofs.UTXO_Soundness Require Import UTXOTransitionSoundness.
From RigorKernel Require Import Supply.SupplySchedule.
From RigorKernel Require Import Script.ScriptClassifier.
From RigorKernel Require Import Pow.TargetRelation.
From RigorKernel Require Import Kernel.Block.Types.

Local Open Scope Z_scope.

Definition accept_utxo (b : BlockTransition) : Prop :=
  utxo_wf (bt_utxo_pre b) /\
  Forall valid_tx (bb_txs (bt_body b)) /\
  exists u', apply_block_transition (bt_utxo_pre b) (bb_txs (bt_body b)) = Some u'.

Definition accept_pow (b : BlockTransition) : Prop :=
  meets_target (bh_digest (bt_header b)) (bh_target (bt_header b)).

Definition accept_supply (b : BlockTransition) : Prop :=
  0 <= res_destroy_cost_at_height (bh_height (bt_header b)).

Definition accept_script (b : BlockTransition) : Prop :=
  classify_script (bb_script (bt_body b)) = bb_expected (bt_body b).

(** Named conjunct so M19 can drop PoW without rewriting the others. *)
Definition require_pow (b : BlockTransition) : Prop :=
  accept_pow b.

Definition block_accepted (b : BlockTransition) : Prop :=
  accept_utxo b /\ require_pow b /\ accept_supply b /\ accept_script b.

Theorem block_acceptance_sound :
  forall b : BlockTransition,
    block_accepted b ->
    accept_utxo b /\
    accept_pow b /\
    accept_supply b /\
    accept_script b.
Proof.
  intros b [Hu [Hp [Hs Hsc]]].
  unfold require_pow in Hp.
  split; [|split; [|split]]; assumption.
Qed.

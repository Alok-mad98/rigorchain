(** UTXO value-flow coupled to the ORU economic pool.

    Inputs, outputs, fees, burn, and subsidy appear in one identity.
    No value is created except subsidy (plus incoming transfer).

    NOT CLAIMED: ConnectBlock, full monetary system, node.
*)
From Coq Require Import ZArith.ZArith.
From Coq Require Import Lia.
From RigorKernel Require Import Kernel.UnifiedState.
From RigorKernel Require Import Kernel.UnifiedTransition.

Local Open Scope Z_scope.

(** Named residual. M30 adds a hidden [+1] (value without input/subsidy). *)
Definition coupled_oru_delta (i : KernelInput) : Z :=
  ki_subsidy i + ki_incoming i - ki_outgoing i - ki_burn i.

Theorem transaction_value_flow_preserves_economic_state :
  forall (s : KernelState) (i : KernelInput) (s' : KernelState),
    kernel_apply s i s' ->
    oru_stock (ks_oru s')
      = oru_stock (ks_oru s) + coupled_oru_delta i /\
    econ_subsidy (ks_econ s') = ki_subsidy i /\
    econ_fees (ks_econ s') = ki_fees i /\
    econ_burn (ks_econ s') = ki_burn i /\
    coupled_oru_delta i + ki_outgoing i + ki_burn i
      = ki_subsidy i + ki_incoming i.
Proof.
  intros s i s' Happ.
  apply kernel_apply_is_graph in Happ as [_ ->].
  unfold kernel_step_of, oru_transition, econ_transition, coupled_oru_delta.
  simpl. repeat split; lia.
Qed.

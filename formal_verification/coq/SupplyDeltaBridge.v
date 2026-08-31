(** SupplyDeltaBridge.v — P052 supply-layer mapping.

    Coq [apply_delta] and C++ [RigorSupplyDelta] are not the same layer.

      C++ oru_circulating_delta  ↔  ledger_oru_delta  (RFC-002: subsidy − converge)
      Coq apply_delta circulating  =  subsidy − converge − fee_burn  (RFC-018)

    SUPPLY_LAYER_SEMANTICS = ALIGNED_BY_LAYER, not IDENTICAL.

    Does not prove C++ = Coq. No [Axiom] / [Admitted] / [Parameter]. *)
From Coq Require Import ZArith.ZArith.
From Coq Require Import Lia.
From Rigor.FeeBurn Require Import Accounting.
From Rigor.FeeBurn Require Import BlockSupply.
From RigorV01 Require Import MintSupplyConservation.

Local Open Scope Z_scope.

Definition refinement_proved : bool := false.
Definition equivalence_proved : bool := false.

Theorem supply_bridge_refinement_not_proved : refinement_proved = false.
Proof. reflexivity. Qed.

Theorem supply_bridge_equivalence_not_proved : equivalence_proved = false.
Proof. reflexivity. Qed.

(** Coq UTXO / mint-conservation transition. *)
Definition canonical_supply_delta := apply_delta.

(** C++ [RigorSupplyDelta.oru_circulating_delta] maps here — RFC-002 ledger. *)
Definition cpp_oru_circulating_delta : Z -> Z -> Z := ledger_oru_delta.

(** C++ [RigorSupplyDelta.oru_claimable] maps to [coinbase_oru_cap]. *)
Definition cpp_oru_claimable : Z -> Z -> Z -> Z := coinbase_oru_cap.

(** Structural view of the C++ Kernel outputs. Uses existing identities only. *)
Record cpp_supply_delta := {
  csd_oru_subsidy : Z;
  csd_oru_claimable : Z;
  csd_oru_circulating_delta : Z;
  csd_res_delta : Z
}.

Definition view_cpp_supply_delta
  (subsidy converge fees burn mint : Z) : cpp_supply_delta :=
  {| csd_oru_subsidy := subsidy;
     csd_oru_claimable := coinbase_oru_cap fees burn subsidy;
     csd_oru_circulating_delta := ledger_oru_delta subsidy converge;
     csd_res_delta := mint |}.

Theorem cpp_circulating_is_ledger_not_utxo :
  forall subsidy converge,
    cpp_oru_circulating_delta subsidy converge = subsidy - converge.
Proof. intros. reflexivity. Qed.

Theorem cpp_claimable_is_cap :
  forall fees burn subsidy,
    cpp_oru_claimable fees burn subsidy = (fees - burn) + subsidy.
Proof. intros. reflexivity. Qed.

Theorem layers_differ_when_burn_positive :
  forall subsidy converge burn,
    0 < burn ->
    circulating_oru_delta subsidy converge burn <>
    ledger_oru_delta subsidy converge.
Proof.
  intros subsidy converge burn Hb.
  unfold circulating_oru_delta, ledger_oru_delta.
  lia.
Qed.

Theorem apply_delta_ledger_matches_cpp_field :
  forall s d s',
    apply_delta s d = Some s' ->
    us_ledger_oru s' =
      us_ledger_oru s + ledger_oru_delta (bd_subsidy d) (bd_converge d).
Proof.
  intros s d s' H.
  apply converge_reduces_oru in H.
  unfold ledger_oru_delta.
  lia.
Qed.

Theorem genesis_cpp_circulating_delta_zero :
  cpp_oru_circulating_delta 0 0 = 0.
Proof. reflexivity. Qed.

Definition SUPPLY_LAYER_SEMANTICS_IDENTICAL : bool := false.
Definition SUPPLY_LAYER_SEMANTICS_ALIGNED_BY_LAYER : bool := true.

Theorem supply_layers_not_identical :
  SUPPLY_LAYER_SEMANTICS_IDENTICAL = false.
Proof. reflexivity. Qed.

Theorem supply_layers_aligned_by_layer :
  SUPPLY_LAYER_SEMANTICS_ALIGNED_BY_LAYER = true.
Proof. reflexivity. Qed.

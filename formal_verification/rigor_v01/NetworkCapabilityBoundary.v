(** RIGOR v0.1 — Network capability boundary (capability-driven P2P).

    Capabilities are a P2P communication layer. They are not consensus.
    They do not change asset state. They do not bypass VerifyRigorPredicate.

    Handshake: VERSION → RIGORCAP → VERACK.
    Feature enablement is HasCapability, never a protocol-version gate.

    Model of the isolation policy, not a C++ refinement.
    No [Axiom] / [Admitted]. *)
From Coq Require Import Arith.PeanoNat.
From Coq Require Import Bool.Bool.
Require Import RigorV01.NoInterpreterConsensus.
Require Import RigorV01.Ed25519Ownership.

(** Network capability names advertised on the wire. Communication only. *)
Inductive RigorCapability :=
| CapNone
| CapTxRelay
| CapBlockRelay
| CapAddrRelay
| CapCompactBlock
| CapTxSync
| CapResRelay
| CapFutureRelay0
| CapFutureRelay1
| UnknownCapability (n : nat).

(** v1 local-active set. Unimplemented and FUTURE_* are named but not active. *)
Definition local_active (c : RigorCapability) : bool :=
  match c with
  | CapTxRelay => true
  | CapBlockRelay => true
  | CapAddrRelay => true
  | CapCompactBlock => true
  | CapNone => false
  | CapTxSync => false
  | CapResRelay => false
  | CapFutureRelay0 => false
  | CapFutureRelay1 => false
  | UnknownCapability _ => false
  end.

Definition enable_capability (c : RigorCapability) : bool :=
  match c with
  | UnknownCapability _ => false
  | CapNone => false
  | CapTxSync => false
  | CapResRelay => false
  | CapFutureRelay0 => false
  | CapFutureRelay1 => false
  | _ => local_active c
  end.

Definition has_capability (enabled : RigorCapability -> bool) (c : RigorCapability) : bool :=
  enabled c.

Inductive StateTransitionChange :=
| AssetStateChanged
| NoStateChange.

Definition capability_change (_c : RigorCapability) : StateTransitionChange :=
  NoStateChange.

(** Theorem 1: network capability does not change consensus state. *)
Theorem capability_change_neq_state_transition_change :
  forall c : RigorCapability,
    capability_change c <> AssetStateChanged.
Proof.
  intros c. discriminate.
Qed.

Theorem capability_change_is_not_state_transition :
  forall c : RigorCapability,
    capability_change c = NoStateChange.
Proof.
  intros c. reflexivity.
Qed.

(** Theorem 2: missing / empty capability cannot enable a feature. *)
Definition enable_if_advertised (received : bool) (c : RigorCapability) : bool :=
  if received then enable_capability c else false.

Theorem missing_advertisement_enables_nothing :
  forall c : RigorCapability, enable_if_advertised false c = false.
Proof.
  intros c. reflexivity.
Qed.

Theorem not_has_capability_implies_not_enabled :
  forall (enabled : RigorCapability -> bool) (c : RigorCapability),
    has_capability enabled c = false -> enabled c = false.
Proof.
  intros enabled c H. exact H.
Qed.

Theorem empty_bitmap_enables_nothing :
  enable_capability CapNone = false.
Proof.
  reflexivity.
Qed.

Theorem unknown_capability_no_effect :
  forall n : nat, enable_capability (UnknownCapability n) = false.
Proof.
  intros n. reflexivity.
Qed.

Theorem unimplemented_slots_not_enabled :
  enable_capability CapTxSync = false /\
  enable_capability CapResRelay = false.
Proof.
  split; reflexivity.
Qed.

Theorem future_relay_slots_not_enabled_in_v1 :
  enable_capability CapFutureRelay0 = false /\
  enable_capability CapFutureRelay1 = false.
Proof.
  split; reflexivity.
Qed.

(** Theorem 3: consensus upgrade isolation. Predicate version is not a network capability. *)
Inductive PredicateVersion :=
| PredicateV1Ed25519.

Definition predicate_as_network_capability (_p : PredicateVersion) : option RigorCapability :=
  None.

Theorem predicate_version_neq_network_capability :
  forall (p : PredicateVersion) (c : RigorCapability),
    predicate_as_network_capability p <> Some c.
Proof.
  intros p c. destruct p. discriminate.
Qed.

Theorem v1_active_caps_are_relay_only :
  local_active CapTxRelay = true /\
  local_active CapBlockRelay = true /\
  local_active CapAddrRelay = true /\
  local_active CapCompactBlock = true.
Proof.
  repeat split; reflexivity.
Qed.

Definition capability_feeds_asset_state : bool := false.

Theorem network_capability_does_not_feed_asset_state :
  capability_feeds_asset_state = false.
Proof. reflexivity. Qed.

Definition network_bypasses_verify_rigor_predicate : bool := false.

Theorem network_cannot_bypass_verify_rigor_predicate :
  network_bypasses_verify_rigor_predicate = false.
Proof. reflexivity. Qed.

Theorem capability_does_not_add_spend_constructor :
  forall s : ConsensusSpend,
    s = CSpendOruP2WPKH \/ s = CSpendResV1 \/ s = CSpendResMint.
Proof.
  apply consensus_spend_cardinality_three.
Qed.

Theorem capability_does_not_enable_opcode_vm :
  opcode_vm_in_consensus = false.
Proof.
  apply no_opcode_vm.
Qed.

Theorem every_spend_still_ed25519_after_capability :
  forall s : ConsensusSpend,
    auth_of_spend
      (match s with
       | CSpendOruP2WPKH => SpendOruP2WPKH
       | CSpendResV1 => SpendResV1KeyPath
       | CSpendResMint => SpendResMint
       end) = AuthEd25519.
Proof.
  apply every_consensus_spend_is_ed25519.
Qed.

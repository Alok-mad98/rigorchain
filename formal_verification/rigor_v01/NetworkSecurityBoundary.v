(** RIGOR v0.1 — Network security boundary.

    P2P is a communication layer. It cannot become a consensus transition.
    Invalid relay objects are rejected before validation.
    A capability bit is not a predicate version.

    Handshake: VERSION → RIGORCAP → VERACK.
    Witness is mandatory on the block-relay path.

    Model of the isolation policy, not a C++ refinement.
    No [Axiom] / [Admitted]. *)
From Coq Require Import Arith.PeanoNat.
From Coq Require Import Bool.Bool.
Require Import RigorV01.NoInterpreterConsensus.
Require Import RigorV01.Ed25519Ownership.
Require Import RigorV01.NetworkCapabilityBoundary.

(** Wire commands. Communication only. *)
Inductive P2PMessage :=
| MsgVersion
| MsgRigorcap
| MsgVerack
| MsgBlock
| MsgTx
| MsgCmpctBlock
| MsgGetBlockTxn
| MsgBlockTxn
| MsgFilterLoad
| MsgUnknown.

Inductive NetworkObject :=
| ValidWitnessBlock
| StrippedBlock
| ValidTx
| InvalidTx
| BloomFilterCommand
| UnknownCommand.

Inductive RelayDecision :=
| AcceptForValidation
| RejectDisconnect.

Definition decide_relay (o : NetworkObject) : RelayDecision :=
  match o with
  | ValidWitnessBlock => AcceptForValidation
  | ValidTx => AcceptForValidation
  | StrippedBlock => RejectDisconnect
  | InvalidTx => RejectDisconnect
  | BloomFilterCommand => RejectDisconnect
  | UnknownCommand => RejectDisconnect
  end.

Definition p2p_feeds_predicate_transition : bool := false.

(** Theorem 1: a P2P message is not a predicate transition. *)
Definition p2p_as_predicate_transition (_m : P2PMessage) : option StateTransitionChange :=
  None.

Theorem p2p_message_neq_predicate_transition :
  forall m : P2PMessage,
    p2p_as_predicate_transition m <> Some AssetStateChanged.
Proof.
  intros m. discriminate.
Qed.

Theorem p2p_message_does_not_feed_predicate_transition :
  p2p_feeds_predicate_transition = false.
Proof.
  reflexivity.
Qed.

Theorem p2p_cannot_bypass_verify_rigor_predicate :
  network_bypasses_verify_rigor_predicate = false.
Proof.
  apply network_cannot_bypass_verify_rigor_predicate.
Qed.

(** Theorem 2: invalid relay cannot enter validation. *)
Theorem invalid_network_object_rejected :
  forall o : NetworkObject,
    (o = StrippedBlock \/ o = InvalidTx \/ o = BloomFilterCommand \/ o = UnknownCommand) ->
    decide_relay o = RejectDisconnect.
Proof.
  intros o [H | [H | [H | H]]]; subst; reflexivity.
Qed.

Theorem stripped_block_not_accepted :
  decide_relay StrippedBlock <> AcceptForValidation.
Proof.
  discriminate.
Qed.

Theorem bloom_command_not_accepted :
  decide_relay BloomFilterCommand <> AcceptForValidation.
Proof.
  discriminate.
Qed.

Theorem witness_block_may_enter_validation :
  decide_relay ValidWitnessBlock = AcceptForValidation.
Proof.
  reflexivity.
Qed.

(** Theorem 3: a capability cannot unlock consensus. CAP_X ≠ PredicateVersion. *)
Inductive NetworkPredicateVersion :=
| NetworkPredV1Ed25519.

Definition capability_as_predicate_version (_c : RigorCapability) : option NetworkPredicateVersion :=
  None.

Definition network_predicate_as_capability (_p : NetworkPredicateVersion) : option RigorCapability :=
  None.

Theorem capability_neq_predicate_version :
  forall (c : RigorCapability) (p : NetworkPredicateVersion),
    capability_as_predicate_version c <> Some p.
Proof.
  intros c p. discriminate.
Qed.

Theorem predicate_version_neq_network_capability_restated :
  forall (p : NetworkPredicateVersion) (c : RigorCapability),
    network_predicate_as_capability p <> Some c.
Proof.
  intros p c. discriminate.
Qed.

Theorem compact_block_cap_does_not_change_asset_state :
  capability_change CapCompactBlock = NoStateChange.
Proof.
  apply capability_change_is_not_state_transition.
Qed.

Theorem every_spend_still_ed25519_after_network_hardening :
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

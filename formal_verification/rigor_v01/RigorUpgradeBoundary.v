(** RIGOR v0.1 — Upgrade boundary (P0-027).

    A future upgrade adds a ScriptPubKey template the classifier can return.
    [PredFuture] exists in the type so the model can name slots that are not
    live; [predicate_of_spk] never produces one.

    That policy is not encoded as a Boolean constant. A definition
    [soft_fork_opcode_extension := false] would let us prove only
    [false = false], which no future edit of this file could break.

    Model of the upgrade policy, not a C++ refinement.
    No [Axiom] / [Admitted]. *)
From Coq Require Import Arith.PeanoNat.
From Coq Require Import List.
Import ListNotations.
Require Import RigorV01.AssetClassification.
Require Import RigorV01.RigorUpgradeNamespace.
Require Import RigorV01.RigorOpcodeNamespace.
Require Import RigorV01.RigorPredicateRegistry.

(** Theorem 3 (restated). v0.1 lookup never yields a future slot, so
    activating a named slot is a source change to [predicate_of_spk],
    not a reinterpretation of a reserved byte. *)
Theorem future_id_never_classified :
  forall spk n, predicate_of_spk spk <> Some (PredFuture n).
Proof. apply predicate_of_spk_never_future. Qed.

Theorem future_slot_not_live :
  forall n : nat, in_v01_registry (PredFuture n) = false.
Proof. apply future_slot_not_in_v01_registry. Qed.

Theorem checksig_byte_stays_reserved_inert :
  classify_opcode reserved_legacy_checksig_byte = ReservedInert.
Proof. apply checksig_byte_is_reserved_inert_here. Qed.

Theorem reserved_checksig_is_not_a_predicate :
  lookup_predicate [reserved_legacy_checksig_byte] = None.
Proof. apply checksig_is_not_a_predicate. Qed.

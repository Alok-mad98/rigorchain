(** RIGOR v0.1 — Opcode namespace closure (P0-027).

    C++ [RigorOpcodeClass]: every byte is a push marker, asset marker,
    reserved-inert, burned, or invalid. None of those classes names an
    operation, so 0xac is inert rather than an executable predicate.

    Model of the namespace, not a C++ refinement.
    No [Axiom] / [Admitted]. *)
From Coq Require Import List.
From Coq Require Import Arith.PeanoNat.
From Coq Require Import Bool.Bool.
Import ListNotations.
Require Import RigorV01.AssetClassification.
Require Import RigorV01.RigorUpgradeNamespace.

Inductive RigorOpcodeClass :=
| PushMarker
| AssetMarker
| ReservedInert
| Burned
| Invalid.

Definition classify_opcode (b : byte) : RigorOpcodeClass :=
  match byte_class b with
  | BytePushMarker => PushMarker
  | ByteLiveMarker => AssetMarker
  | ByteReservedInert => ReservedInert
  | ByteBurned => Burned
  | ByteVacant | ByteInvalidSentinel => Invalid
  end.

Definition opcode_class_eqb (a b : RigorOpcodeClass) : bool :=
  match a, b with
  | PushMarker, PushMarker => true
  | AssetMarker, AssetMarker => true
  | ReservedInert, ReservedInert => true
  | Burned, Burned => true
  | Invalid, Invalid => true
  | _, _ => false
  end.

Definition count_opcode_class (c : RigorOpcodeClass) : nat :=
  length (filter (fun b => opcode_class_eqb (classify_opcode b) c) (seq 0 256)).

(** Theorem 1 (restated). The earlier form defined [opcode_is_instruction] to
    be [false] and then proved [false = false]; it could not fail, so it
    asserted the absence of a VM rather than establishing anything. This form
    pins the partition that a VM would have to change, and matches
    [byte_namespace_partition_is_frozen] in C++ exactly. Enforcement that no
    evaluator exists lives in test/lint/lint-no-script-vm.py. *)
Theorem opcode_namespace_partition_is_frozen :
  count_opcode_class PushMarker = 95%nat /\
  count_opcode_class AssetMarker = 3%nat /\
  count_opcode_class Burned = 1%nat /\
  count_opcode_class ReservedInert = 91%nat /\
  count_opcode_class Invalid = 66%nat.
Proof. repeat split; vm_compute; reflexivity. Qed.

Theorem checksig_byte_is_reserved_inert_here :
  classify_opcode reserved_legacy_checksig_byte = ReservedInert.
Proof. vm_compute. reflexivity. Qed.

Theorem live_asset_markers :
  classify_opcode RIGOR_MARKER_RETURN = AssetMarker /\
  classify_opcode RIGOR_MARKER_RES_MINT = AssetMarker /\
  classify_opcode RIGOR_MARKER_CONVERGE = AssetMarker.
Proof. repeat split; vm_compute; reflexivity. Qed.

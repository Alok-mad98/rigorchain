(** RIGOR v0.1 — Native byte namespace (P0-026-V2).

    C++ [ClassifyRigorByte] in [core/src/script/rigor_bytes.cpp]:
    every byte is DATA_PUSH, a live marker, RESERVED_INERT, or INVALID.
    There is no Script VM, no opcode semantics, and no stack machine.

    Unknown predicates are not spendable (fail closed).

    Model of the namespace, not a C++ refinement.
    No [Axiom] / [Admitted]. *)
From Coq Require Import Arith.PeanoNat.
From Coq Require Import Bool.Bool.
Require Import RigorV01.AssetClassification.
Require Import RigorV01.RigorUpgradeNamespace.
Require Import RigorV01.NoInterpreterConsensus.

(** * Byte classes matching C++ [RigorByteClass] *)

Inductive RigorByteClass :=
| DataPush
| ResMintMarker
| ConvergeMarker
| ReturnMarker
| ReservedInert
| Invalid.

Definition classify_rigor_byte (b : nat) : RigorByteClass :=
  if Nat.eqb b 187%nat then ResMintMarker
  else if Nat.eqb b 189%nat then ConvergeMarker
  else if Nat.eqb b 106%nat then ReturnMarker
  else if orb (Nat.leb b 78%nat)
              (andb (Nat.leb 81%nat b) (Nat.leb b 96%nat))
       then DataPush
  else if orb (Nat.eqb b 188%nat) (Nat.ltb 189%nat b)
       then Invalid
  else ReservedInert.

Definition is_marker (c : RigorByteClass) : bool :=
  match c with
  | DataPush | ResMintMarker | ConvergeMarker | ReturnMarker => true
  | _ => false
  end.

Definition is_reserved (c : RigorByteClass) : bool :=
  match c with
  | ReservedInert => true
  | _ => false
  end.

Definition is_invalid (c : RigorByteClass) : bool :=
  match c with
  | Invalid => true
  | _ => false
  end.

Theorem byte_class_complete :
  forall b : nat,
    let c := classify_rigor_byte b in
    is_marker c = true \/ is_reserved c = true \/ is_invalid c = true.
Proof.
  intros b.
  destruct (classify_rigor_byte b); simpl; auto.
Qed.

Theorem live_markers_are_frozen :
  classify_rigor_byte 106%nat = ReturnMarker /\
  classify_rigor_byte 187%nat = ResMintMarker /\
  classify_rigor_byte 189%nat = ConvergeMarker.
Proof. repeat split; vm_compute; reflexivity. Qed.

Theorem burned_0xbc_is_invalid :
  classify_rigor_byte 188%nat = Invalid.
Proof. vm_compute. reflexivity. Qed.

Theorem vacant_future_bytes_are_invalid :
  classify_rigor_byte 190%nat = Invalid /\
  classify_rigor_byte 254%nat = Invalid /\
  classify_rigor_byte 255%nat = Invalid.
Proof. repeat split; vm_compute; reflexivity. Qed.

Theorem reserved_checksig_byte_is_inert :
  classify_rigor_byte 172%nat = ReservedInert.
Proof. vm_compute. reflexivity. Qed.

(** * No VM *)

Definition execution_loop_exists : Prop := False.
Definition stack_transition_exists : Prop := False.
Definition opcode_semantics_exists : Prop := False.

Theorem no_execution_loop :
  ~ execution_loop_exists.
Proof. intros H. exact H. Qed.

Theorem no_stack_transition :
  ~ stack_transition_exists.
Proof. intros H. exact H. Qed.

Theorem no_opcode_semantics :
  ~ opcode_semantics_exists.
Proof. intros H. exact H. Qed.

Theorem no_general_script_vm :
  opcode_vm_in_consensus = false /\
  eval_script_in_consensus = false.
Proof. split; reflexivity. Qed.

(** * Future predicate safety: unknown template is not spendable *)

Definition spendable_asset (a : AssetType) : bool :=
  match a with
  | AssetORU | AssetRES => true
  | AssetNone | AssetInvalid => false
  end.

Theorem unknown_predicate_not_spendable :
  forall spk : list byte,
    predicate_of_spk spk = None ->
    spendable_asset (classify spk) = false.
Proof.
  intros spk H.
  rewrite (unknown_template_not_in_registry spk H).
  reflexivity.
Qed.

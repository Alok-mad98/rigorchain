(** RIGOR v0.1 — Upgrade namespace without a Script VM.

    Every byte is Marker, Reserved-inert, Burned, Vacant, or Invalid.
    No byte is an Instruction / Stack op / Computation / Branch / Loop.

    PredicateRegistry is a closed set of ScriptPubKey templates, not an
    opcode interpreter. Bytes that are not live markers have no consensus
    meaning as operations.

    Future predicates may be added as new registry constructors. That does
    not enable opcode evaluation.

    Model of the namespace, not a C++ refinement.
    No [Axiom] / [Admitted]. *)
From Coq Require Import List.
From Coq Require Import Bool.Bool.
From Coq Require Import Arith.PeanoNat.
Import ListNotations.
Require Import RigorV01.AssetClassification.

(** * Byte classes — the only allowed roles *)

Inductive ByteClass :=
| BytePushMarker
| ByteLiveMarker
| ByteReservedInert
| ByteBurned
| ByteVacant
| ByteInvalidSentinel.

Definition byte_class (b : byte) : ByteClass :=
  if Nat.eqb b 255%nat then ByteInvalidSentinel
  else if Nat.eqb b 188%nat then ByteBurned (* 0xbc burned *)
  else if orb (Nat.eqb b RIGOR_MARKER_RETURN)
              (orb (Nat.eqb b RIGOR_MARKER_RES_MINT) (Nat.eqb b RIGOR_MARKER_CONVERGE))
       then ByteLiveMarker
  else if orb (Nat.eqb b RIGOR_PUSH_0)
              (orb (andb (Nat.leb 1%nat b) (Nat.leb b 75%nat))
                   (orb (andb (Nat.leb RIGOR_PUSHDATA1 b) (Nat.leb b RIGOR_PUSHDATA4))
                        (andb (Nat.leb RIGOR_PUSH_1 b) (Nat.leb b RIGOR_PUSH_16))))
       then BytePushMarker
  else if andb (Nat.leb 190%nat b) (Nat.leb b 254%nat) (* 0xbe..0xfe *)
       then ByteVacant
  else ByteReservedInert.

(** "No byte is an instruction" is not stated here as a theorem. A predicate
    defined to return [false] would let us prove only [false = false], which no
    future change to this development could break; it would assert the property
    rather than establish it.

    What this model can pin is the partition itself: which bytes carry consensus
    meaning, and how many of each kind exist. Editing [byte_class] breaks the
    counts below. The identical counts are asserted on the C++ side by
    [byte_namespace_partition_is_frozen]
    (core/src/test/rigor_opcode_namespace_tests.cpp), so model and
    implementation must move together.

    The absence of an evaluator is enforced outside both, by
    test/lint/lint-no-script-vm.py, which fails the build if evaluator symbols
    reappear under the consensus directories. *)

Definition class_eqb (a b : ByteClass) : bool :=
  match a, b with
  | BytePushMarker, BytePushMarker => true
  | ByteLiveMarker, ByteLiveMarker => true
  | ByteReservedInert, ByteReservedInert => true
  | ByteBurned, ByteBurned => true
  | ByteVacant, ByteVacant => true
  | ByteInvalidSentinel, ByteInvalidSentinel => true
  | _, _ => false
  end.

Definition count_byte_class (c : ByteClass) : nat :=
  length (filter (fun b => class_eqb (byte_class b) c) (seq 0 256)).

(** Only 98 of 256 bytes carry meaning: 95 data pushes and 3 asset markers. *)
Theorem byte_namespace_partition_is_frozen :
  count_byte_class BytePushMarker = 95%nat /\
  count_byte_class ByteLiveMarker = 3%nat /\
  count_byte_class ByteBurned = 1%nat /\
  count_byte_class ByteReservedInert = 91%nat /\
  count_byte_class ByteVacant = 65%nat /\
  count_byte_class ByteInvalidSentinel = 1%nat.
Proof. repeat split; vm_compute; reflexivity. Qed.

(** The five classes exhaust the byte space: no byte escapes classification. *)
Theorem byte_namespace_is_total :
  (count_byte_class BytePushMarker + count_byte_class ByteLiveMarker +
   count_byte_class ByteBurned + count_byte_class ByteReservedInert +
   count_byte_class ByteVacant + count_byte_class ByteInvalidSentinel)%nat
  = 256%nat.
Proof. vm_compute. reflexivity. Qed.

(** * v0.1 predicate registry — templates, not opcodes *)

Inductive PredicateId :=
| PredOruP2WPKH
| PredResV1
| PredResMint
| PredConvergeBurn
| PredNullData
| PredFuture (slot : nat).

Definition in_v01_registry (p : PredicateId) : bool :=
  match p with
  | PredFuture _ => false
  | _ => true
  end.

Definition predicate_of_spk (spk : list byte) : option PredicateId :=
  match parse_witness_program spk with
  | Some (0%nat, prog) =>
      if Nat.eqb (length prog) 20%nat then Some PredOruP2WPKH else None
  | Some (1%nat, prog) =>
      if is_native_res_program prog then Some PredResV1 else None
  | Some _ => None
  | None =>
      if is_res_mint_script spk then Some PredResMint
      else if is_converge_script spk then Some PredConvergeBurn
      else if is_op_return spk then Some PredNullData
      else None
  end.

Theorem unknown_template_not_in_registry :
  forall spk : list byte,
    predicate_of_spk spk = None ->
    classify spk = AssetInvalid.
Proof.
  intros spk H.
  unfold predicate_of_spk, classify in *.
  destruct (parse_witness_program spk) as [[v prog]|].
  - destruct v as [|v'].
    + destruct (Nat.eqb (length prog) 20%nat) eqn:E; [discriminate H|reflexivity].
    + destruct v' as [|v''].
      * destruct (is_native_res_program prog) eqn:N; [discriminate H|reflexivity].
      * reflexivity.
  - destruct (is_res_mint_script spk) eqn:M; [discriminate H|].
    destruct (is_converge_script spk) eqn:C; [discriminate H|].
    destruct (is_op_return spk) eqn:R; [discriminate H|reflexivity].
Qed.

Theorem v01_predicates_are_registered :
  in_v01_registry PredOruP2WPKH = true /\
  in_v01_registry PredResV1 = true /\
  in_v01_registry PredResMint = true /\
  in_v01_registry PredConvergeBurn = true /\
  in_v01_registry PredNullData = true.
Proof. repeat split; reflexivity. Qed.

Theorem future_slot_not_in_v01_registry :
  forall n : nat, in_v01_registry (PredFuture n) = false.
Proof. intros n. reflexivity. Qed.

(** * Legacy Bitcoin instruction bytes have no consensus meaning *)

Definition reserved_legacy_checksig_byte : byte := RIGOR_RESERVED_LEGACY_CHECKSIG.
Definition reserved_legacy_if_byte : byte := 99%nat. (* 0x63 *)
Definition reserved_legacy_nop1_byte : byte := 176%nat. (* 0xb0 *)

Theorem checksig_byte_is_reserved_inert :
  byte_class reserved_legacy_checksig_byte = ByteReservedInert.
Proof. vm_compute. reflexivity. Qed.

Theorem checksig_script_not_a_predicate :
  predicate_of_spk [reserved_legacy_checksig_byte] = None.
Proof. vm_compute. reflexivity. Qed.

Theorem checksig_classifies_invalid :
  classify [reserved_legacy_checksig_byte] = AssetInvalid.
Proof. vm_compute. reflexivity. Qed.

Theorem if_byte_is_reserved_inert :
  byte_class reserved_legacy_if_byte = ByteReservedInert.
Proof. vm_compute. reflexivity. Qed.

Theorem nop1_is_reserved_inert :
  byte_class reserved_legacy_nop1_byte = ByteReservedInert.
Proof. vm_compute. reflexivity. Qed.

(** Reserved / vacant / burned bytes are not live markers. These fail if
    [byte_class] is edited so that 0xac or 0xbe..0xfe become templates. The
    absence of an evaluator is not stated here: it is enforced by
    test/lint/lint-no-script-vm.py. *)
Theorem reserved_inert_is_not_a_live_marker :
  forall b : byte,
    byte_class b = ByteReservedInert -> byte_class b <> ByteLiveMarker.
Proof. intros b H. rewrite H. discriminate. Qed.

Theorem vacant_is_not_a_live_marker :
  forall b : byte,
    byte_class b = ByteVacant -> byte_class b <> ByteLiveMarker.
Proof. intros b H. rewrite H. discriminate. Qed.

Theorem burned_is_not_a_live_marker :
  forall b : byte,
    byte_class b = ByteBurned -> byte_class b <> ByteLiveMarker.
Proof. intros b H. rewrite H. discriminate. Qed.

(** * Future predicates are registry extensions, not opcode activations *)

Theorem predicate_of_spk_never_future :
  forall spk n, predicate_of_spk spk <> Some (PredFuture n).
Proof.
  intros spk n.
  unfold predicate_of_spk.
  destruct (parse_witness_program spk) as [[v prog]|]; try discriminate.
  - destruct v as [|v']; try discriminate.
    + destruct (Nat.eqb (length prog) 20%nat); discriminate.
    + destruct v' as [|]; try discriminate.
      destruct (is_native_res_program prog); discriminate.
  - destruct (is_res_mint_script spk); try discriminate.
    destruct (is_converge_script spk); try discriminate.
    destruct (is_op_return spk); discriminate.
Qed.

Theorem future_activation_is_not_checksig :
  classify [reserved_legacy_checksig_byte] = AssetInvalid.
Proof. apply checksig_classifies_invalid. Qed.

(** Live markers keep their template role. *)
Theorem return_is_live_marker :
  byte_class RIGOR_MARKER_RETURN = ByteLiveMarker.
Proof. vm_compute. reflexivity. Qed.

Theorem res_mint_is_live_marker :
  byte_class RIGOR_MARKER_RES_MINT = ByteLiveMarker.
Proof. vm_compute. reflexivity. Qed.

Theorem converge_is_live_marker :
  byte_class RIGOR_MARKER_CONVERGE = ByteLiveMarker.
Proof. vm_compute. reflexivity. Qed.

Theorem burned_cvg_claim_not_a_predicate :
  predicate_of_spk [188%nat] = None.
Proof. vm_compute. reflexivity. Qed.

Theorem live_canonicals_are_registered :
  predicate_of_spk canonical_p2wpkh = Some PredOruP2WPKH /\
  predicate_of_spk canonical_native_res = Some PredResV1 /\
  predicate_of_spk canonical_res_mint = Some PredResMint /\
  predicate_of_spk canonical_converge = Some PredConvergeBurn /\
  predicate_of_spk canonical_op_return = Some PredNullData.
Proof. vm_compute. repeat split; reflexivity. Qed.

Theorem p2pkh_not_in_registry :
  predicate_of_spk canonical_p2pkh = None.
Proof. vm_compute. reflexivity. Qed.

Theorem p2wsh_not_in_registry :
  predicate_of_spk canonical_p2wsh = None.
Proof. vm_compute. reflexivity. Qed.

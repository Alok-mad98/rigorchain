(** RIGOR v0.1 — Property 2: Script-as-Asset Integrity.

    Same scriptPubKey ⇒ same asset and same ownership predicate.
    Distinct spendable scriptPubKeys ⇒ distinct ownership predicates
    (the predicate includes lock kind + committed material; unique
    encoding of each live template).

    Matches C++: no stored asset tag on CTxOut / Coin. *)
From Coq Require Import List.
From Coq Require Import Bool.Bool.
From Coq Require Import Arith.PeanoNat.
Import ListNotations.
Require Import RigorV01.AssetClassification.

(** Spendable lock kinds. CONVERGE is an ORU-domain burn, not spendable.
    NONE / INVALID are not spendable. *)
Inductive LockKind :=
| LockOruP2WPKH
| LockResV1
| LockResMint
| LockConvergeBurn
| LockUnspendable.

Definition lock_kind_eqb (a b : LockKind) : bool :=
  match a, b with
  | LockOruP2WPKH, LockOruP2WPKH => true
  | LockResV1, LockResV1 => true
  | LockResMint, LockResMint => true
  | LockConvergeBurn, LockConvergeBurn => true
  | LockUnspendable, LockUnspendable => true
  | _, _ => false
  end.

(** Ownership predicate: lock kind plus the committed key material.
    ORU / mint / converge commit 20-byte HASH160(pk).
    RES v1 commits the 32-byte pk itself (Option B). *)
Inductive OwnershipPredicate :=
| OwnOruP2WPKH (pkh : list byte)
| OwnResV1 (pk : list byte)
| OwnResMint (pkh : list byte)
| OwnConverge (pkh : list byte)
| OwnNone
| OwnInvalid.

Definition own_eqb (a b : OwnershipPredicate) : bool :=
  match a, b with
  | OwnOruP2WPKH h1, OwnOruP2WPKH h2 =>
      if list_eq_dec Nat.eq_dec h1 h2 then true else false
  | OwnResV1 p1, OwnResV1 p2 =>
      if list_eq_dec Nat.eq_dec p1 p2 then true else false
  | OwnResMint h1, OwnResMint h2 =>
      if list_eq_dec Nat.eq_dec h1 h2 then true else false
  | OwnConverge h1, OwnConverge h2 =>
      if list_eq_dec Nat.eq_dec h1 h2 then true else false
  | OwnNone, OwnNone => true
  | OwnInvalid, OwnInvalid => true
  | _, _ => false
  end.

Definition ownership_of (spk : list byte) : OwnershipPredicate :=
  match parse_witness_program spk with
  | Some (0%nat, prog) =>
      if Nat.eqb (length prog) 20%nat then OwnOruP2WPKH prog else OwnInvalid
  | Some (1%nat, prog) =>
      if is_native_res_program prog then OwnResV1 prog else OwnInvalid
  | Some _ => OwnInvalid
  | None =>
      match parse_res_mint_pubkey_hash spk with
      | Some h => OwnResMint h
      | None =>
          if is_converge_script spk then
            match spk with
            | _plen :: rest =>
                match rev rest with
                | _op :: hash_rev => OwnConverge (rev hash_rev)
                | nil => OwnInvalid
                end
            | nil => OwnInvalid
            end
          else if is_op_return spk then OwnNone
          else OwnInvalid
      end
  end.

Theorem same_script_same_ownership :
  forall s1 s2 : list byte,
    s1 = s2 -> ownership_of s1 = ownership_of s2.
Proof. intros s1 s2 H. rewrite H. reflexivity. Qed.

Theorem same_script_same_asset_and_ownership :
  forall s1 s2 : list byte,
    s1 = s2 ->
    classify s1 = classify s2 /\ ownership_of s1 = ownership_of s2.
Proof. intros s1 s2 H. rewrite H. split; reflexivity. Qed.

(** Distinct constructors are distinct predicates even when HASH160
    material coincides (ORU pkh vs mint pkh vs converge pkh). *)
Theorem oru_mint_predicates_distinct :
  forall h : list byte,
    OwnOruP2WPKH h <> OwnResMint h.
Proof. intros h H. inversion H. Qed.

Theorem oru_converge_predicates_distinct :
  forall h : list byte,
    OwnOruP2WPKH h <> OwnConverge h.
Proof. intros h H. inversion H. Qed.

Theorem mint_converge_predicates_distinct :
  forall h : list byte,
    OwnResMint h <> OwnConverge h.
Proof. intros h H. inversion H. Qed.

Theorem res_v1_not_hash160_lock :
  forall pk h : list byte,
    OwnResV1 pk <> OwnOruP2WPKH h /\
    OwnResV1 pk <> OwnResMint h.
Proof. intros pk h. split; intros H; inversion H. Qed.

Lemma canonical_p2wpkh_own :
  ownership_of canonical_p2wpkh = OwnOruP2WPKH (repeat_byte 20 0%nat).
Proof. vm_compute. reflexivity. Qed.

Lemma canonical_native_res_own :
  ownership_of canonical_native_res = OwnResV1 rfc8032_pk1.
Proof. vm_compute. reflexivity. Qed.

Lemma canonical_mint_own :
  ownership_of canonical_res_mint = OwnResMint (repeat_byte 20 1%nat).
Proof. vm_compute. reflexivity. Qed.

Lemma canonical_converge_own :
  ownership_of canonical_converge = OwnConverge (repeat_byte 20 0%nat).
Proof. vm_compute. reflexivity. Qed.

(** Different live templates cannot collapse to one predicate. *)
Theorem p2wpkh_and_mint_not_same_predicate :
  ownership_of canonical_p2wpkh <> ownership_of canonical_res_mint.
Proof. vm_compute. discriminate. Qed.

Theorem p2wpkh_and_res_v1_not_same_predicate :
  ownership_of canonical_p2wpkh <> ownership_of canonical_native_res.
Proof. vm_compute. discriminate. Qed.

Theorem res_v1_and_mint_not_same_predicate :
  ownership_of canonical_native_res <> ownership_of canonical_res_mint.
Proof. vm_compute. discriminate. Qed.

(** Classification and ownership are derived from the same script bytes.
    Native RES ownership material *is* the program (Option B). *)
Theorem res_v1_ownership_is_program :
  ownership_of canonical_native_res = OwnResV1 (res_witness_program rfc8032_pk1).
Proof. vm_compute. reflexivity. Qed.

Definition is_spendable_own (o : OwnershipPredicate) : bool :=
  match o with
  | OwnOruP2WPKH _ | OwnResV1 _ | OwnResMint _ => true
  | OwnConverge _ | OwnNone | OwnInvalid => false
  end.

Lemma p2wpkh_spendable :
  is_spendable_own (ownership_of canonical_p2wpkh) = true.
Proof. vm_compute. reflexivity. Qed.

Lemma res_v1_spendable :
  is_spendable_own (ownership_of canonical_native_res) = true.
Proof. vm_compute. reflexivity. Qed.

Lemma mint_spendable :
  is_spendable_own (ownership_of canonical_res_mint) = true.
Proof. vm_compute. reflexivity. Qed.

Lemma converge_not_spendable :
  is_spendable_own (ownership_of canonical_converge) = false.
Proof. vm_compute. reflexivity. Qed.

Lemma opret_not_spendable :
  is_spendable_own (ownership_of canonical_op_return) = false.
Proof. vm_compute. reflexivity. Qed.

Lemma p2wsh_not_spendable :
  is_spendable_own (ownership_of canonical_p2wsh) = false.
Proof. vm_compute. reflexivity. Qed.

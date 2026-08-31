(** RIGOR v0.1 — Property 3 / 5: Ed25519 is the unique authorization primitive.

    Every spendable path is:

      VerifyEd25519(signature, signed_message, pk)

    There is no multisig path, no opcode authorization path, and no
    script-interpreter path. The cryptographic algorithm itself is
    C++ libsodium; this module proves the *boundary*: every live
    constructor requires the Ed25519 gate, and no other constructor
    exists.

    Not a C++ refinement. No [Axiom] / [Admitted]. *)
From Coq Require Import List.
From Coq Require Import Bool.Bool.
From Coq Require Import Arith.PeanoNat.
Import ListNotations.
Require Import RigorV01.AssetClassification.
Require Import RigorV01.ScriptAssetBoundary.

Definition ed25519_sig_size : nat := 64%nat.
Definition ed25519_pk_size : nat := 32%nat.
Definition sighash_size : nat := 32%nat.

(** Encoding gate matching C++ size checks. RFC 8032 verification is
    production [ed25519::VerifyDetached]; Coq does not reimplement it. *)
Definition ed25519_encoding_ok (sig pk msg : list byte) : bool :=
  andb (Nat.eqb (length sig) ed25519_sig_size)
       (andb (Nat.eqb (length pk) ed25519_pk_size)
             (Nat.eqb (length msg) sighash_size)).

(** Unique authorization method in the v0.1 consensus model. *)
Inductive AuthMethod :=
| AuthEd25519.

(** Exactly three spendable consensus paths (C++ [VerifyRigorPredicate] dispatch). *)
Inductive SpendPath :=
| SpendOruP2WPKH
| SpendResV1KeyPath
| SpendResMint.

Definition auth_of_spend (p : SpendPath) : AuthMethod := AuthEd25519.

Theorem every_spendable_path_is_ed25519 :
  forall p : SpendPath, auth_of_spend p = AuthEd25519.
Proof. intros p. destruct p; reflexivity. Qed.

Theorem spend_paths_exhaustive :
  forall p : SpendPath,
    p = SpendOruP2WPKH \/ p = SpendResV1KeyPath \/ p = SpendResMint.
Proof.
  intros p. destruct p; [left; reflexivity | right; left; reflexivity | right; right; reflexivity].
Qed.

(** Key-binding per path. Option B: RES v1 requires pk == program. *)
Definition oru_key_ok (program pk : list byte) : bool :=
  (* program is HASH160(pk); Coq does not compute HASH160. Shape: 20 vs 32. *)
  andb (Nat.eqb (length program) 20%nat) (Nat.eqb (length pk) 32%nat).

Definition res_v1_key_ok (program pk : list byte) : bool :=
  if list_eq_dec Nat.eq_dec program pk then true else false.

Definition mint_key_ok (pkh pk : list byte) : bool :=
  (* pkh is HASH160(pk). Shape only. *)
  andb (Nat.eqb (length pkh) 20%nat) (Nat.eqb (length pk) 32%nat).

Theorem res_v1_key_ok_is_program_eq_pk :
  forall pk : list byte, res_v1_key_ok pk pk = true.
Proof. intros pk. unfold res_v1_key_ok. destruct (list_eq_dec Nat.eq_dec pk pk); [reflexivity|contradiction]. Qed.

Theorem res_v1_key_ok_rejects_mismatch :
  forall pk other : list byte,
    pk <> other -> res_v1_key_ok pk other = false.
Proof.
  intros pk other Hne.
  unfold res_v1_key_ok.
  destruct (list_eq_dec Nat.eq_dec pk other); [contradiction|reflexivity].
Qed.

(** A spend succeeds in this model only if encoding + key-binding hold.
    The crypto accept bit is an additional conjunct supplied by C++. *)
Record SpendAttempt := {
  sa_path : SpendPath;
  sa_sig : list byte;
  sa_pk : list byte;
  sa_msg : list byte;
  sa_program_or_pkh : list byte;
  sa_crypto_accept : bool
}.

Definition key_binding_ok (a : SpendAttempt) : bool :=
  match sa_path a with
  | SpendOruP2WPKH => oru_key_ok (sa_program_or_pkh a) (sa_pk a)
  | SpendResV1KeyPath => res_v1_key_ok (sa_program_or_pkh a) (sa_pk a)
  | SpendResMint => mint_key_ok (sa_program_or_pkh a) (sa_pk a)
  end.

Definition spend_authorized (a : SpendAttempt) : bool :=
  andb (ed25519_encoding_ok (sa_sig a) (sa_pk a) (sa_msg a))
       (andb (key_binding_ok a) (sa_crypto_accept a)).

Theorem spend_authorized_requires_ed25519_encoding :
  forall a : SpendAttempt,
    spend_authorized a = true ->
    ed25519_encoding_ok (sa_sig a) (sa_pk a) (sa_msg a) = true.
Proof.
  intros a H.
  unfold spend_authorized in H.
  apply andb_true_iff in H. destruct H as [He _]. exact He.
Qed.

Theorem spend_authorized_requires_crypto_accept :
  forall a : SpendAttempt,
    spend_authorized a = true ->
    sa_crypto_accept a = true.
Proof.
  intros a H.
  unfold spend_authorized in H.
  apply andb_true_iff in H. destruct H as [_ H2].
  apply andb_true_iff in H2. destruct H2 as [_ Hc]. exact Hc.
Qed.

Theorem spend_authorized_requires_key_binding :
  forall a : SpendAttempt,
    spend_authorized a = true ->
    key_binding_ok a = true.
Proof.
  intros a H.
  unfold spend_authorized in H.
  apply andb_true_iff in H. destruct H as [_ H2].
  apply andb_true_iff in H2. destruct H2 as [Hk _]. exact Hk.
Qed.

(** Property 5 (model): a created spendable UTXO is spendable only through
    the Ed25519 attempt on its ownership predicate. No bypass constructor. *)
Definition ownership_path (o : OwnershipPredicate) : option SpendPath :=
  match o with
  | OwnOruP2WPKH _ => Some SpendOruP2WPKH
  | OwnResV1 _ => Some SpendResV1KeyPath
  | OwnResMint _ => Some SpendResMint
  | OwnConverge _ | OwnNone | OwnInvalid => None
  end.

Theorem spendable_own_has_ed25519_path :
  forall o : OwnershipPredicate,
    is_spendable_own o = true ->
    exists p : SpendPath, ownership_path o = Some p /\ auth_of_spend p = AuthEd25519.
Proof.
  intros o H.
  destruct o; simpl in H; try discriminate H.
  - exists SpendOruP2WPKH. split; reflexivity.
  - exists SpendResV1KeyPath. split; reflexivity.
  - exists SpendResMint. split; reflexivity.
Qed.

Theorem unspendable_own_has_no_path :
  forall o : OwnershipPredicate,
    is_spendable_own o = false ->
    ownership_path o = None.
Proof.
  intros o H. destruct o; simpl in *; try reflexivity; discriminate H.
Qed.

(** Option B at the spend gate: RES v1 authorized spend needs pk = program. *)
Theorem res_v1_authorized_implies_pk_eq_program :
  forall a : SpendAttempt,
    sa_path a = SpendResV1KeyPath ->
    spend_authorized a = true ->
    sa_program_or_pkh a = sa_pk a.
Proof.
  intros a Hp Hok.
  unfold spend_authorized, key_binding_ok in Hok.
  rewrite Hp in Hok.
  apply andb_true_iff in Hok. destruct Hok as [_ H2].
  apply andb_true_iff in H2. destruct H2 as [Hk _].
  unfold res_v1_key_ok in Hk.
  destruct (list_eq_dec Nat.eq_dec (sa_program_or_pkh a) (sa_pk a)); [assumption|discriminate Hk].
Qed.

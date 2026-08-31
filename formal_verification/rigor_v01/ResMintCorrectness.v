(** RIGOR v0.1 — RES_MINT correctness.

    Live C++:
      scriptPubKey = <maturity> <20-byte HASH160(pk)> RIGOR_MARKER_RES_MINT
      create predicate = spend predicate = HASH160(pk)
      NOT HASH160(v1 program) as a second object (Option A).
      Under Option B, HASH160(v1 program) = HASH160(pk) identically.

    Unique parser: [ParseResMintScript] / [VerifyResMintSpend].
    There is no Script VM on the RES mint path.

    Maturity (two layers, both required):
      1. Coinbase maturity: RES_MINT_MATURITY = COINBASE_MATURITY = 100
         ([Coin::RequiresMaturity] = [IsCoinBase]; mint is coinbase-only).
      2. Script lock: spend_height >= maturity_height in the script.

    Owner: [CheckResRewardOwner] compares declared HASH160 to the 20-byte
    field in the mint script (same bytes [VerifyResMintSpend] checks).

    Signature domain: SigVersion::RES_MINT / tag RGRMNT01.

    Not a C++ refinement. HASH160 is not computed here. No [Axiom] / [Admitted]. *)
From Coq Require Import List.
From Coq Require Import Bool.Bool.
From Coq Require Import Arith.PeanoNat.
From Coq Require Import ZArith.ZArith.
Import ListNotations.
Require Import RigorV01.AssetClassification.
Require Import RigorV01.ScriptAssetBoundary.
Require Import RigorV01.Ed25519Ownership.
Require Import RigorV01.SignatureDomainSeparation.

(** 20-byte owner commitment. Coq does not implement RIPEMD160(SHA256).
    The live rule is HASH160(pk32), never HASH160(SHA256(pk)). *)
Definition owner_pkh_size : nat := 20%nat.

Record ResMintLock := {
  mint_maturity : Z;
  mint_pkh : list byte
}.

Definition mint_lock_ok (l : ResMintLock) : bool :=
  Nat.eqb (length (mint_pkh l)) owner_pkh_size.

(** Create predicate = spend predicate = the 20-byte HASH160(pk) field. *)
Definition mint_create_predicate (l : ResMintLock) : list byte := mint_pkh l.
Definition mint_spend_predicate (l : ResMintLock) : list byte := mint_pkh l.

Theorem mint_create_eq_spend :
  forall l : ResMintLock,
    mint_create_predicate l = mint_spend_predicate l.
Proof. intros. reflexivity. Qed.

Theorem mint_single_parser :
  forall l : ResMintLock,
    mint_lock_ok l = true ->
    mint_create_predicate l = mint_pkh l /\
    mint_spend_predicate l = mint_pkh l.
Proof. intros l _. split; reflexivity. Qed.

(** Forbidden Option A alias: HASH160(SHA256(pk)) as a distinct object.
    It is not a constructor of this model. *)
Inductive MintOwnerEncoding :=
| MintOwnerHash160OfPk
| MintOwnerHash160OfSha256Program.

Definition live_mint_owner_encoding : MintOwnerEncoding := MintOwnerHash160OfPk.

Theorem mint_owner_is_not_option_a :
  live_mint_owner_encoding <> MintOwnerHash160OfSha256Program.
Proof. discriminate. Qed.

Theorem mint_owner_is_hash160_of_pk :
  live_mint_owner_encoding = MintOwnerHash160OfPk.
Proof. reflexivity. Qed.

(** Under Option B, HASH160(v1 program) and HASH160(pk) are the same
    bytes because program = pk. *)
Theorem option_b_collapses_program_hash_to_pk_hash :
  forall pk : list byte,
    res_witness_program pk = pk.
Proof. apply option_b_program_is_pubkey. Qed.

(** Coinbase / script maturity. *)
Definition coinbase_maturity : Z := 100%Z.
Definition res_mint_maturity_blocks : Z := coinbase_maturity.

Theorem res_mint_maturity_locked_to_coinbase :
  res_mint_maturity_blocks = coinbase_maturity.
Proof. reflexivity. Qed.

Definition coinbase_mature (coin_height spend_height : Z) : bool :=
  Z.leb (coin_height + coinbase_maturity) spend_height.

Definition script_lock_satisfied (l : ResMintLock) (spend_height : Z) : bool :=
  Z.leb (mint_maturity l) spend_height.

Definition mint_spend_height_ok (l : ResMintLock) (coin_height spend_height : Z) : bool :=
  andb (coinbase_mature coin_height spend_height)
       (script_lock_satisfied l spend_height).

Theorem mint_spend_requires_both_maturity_layers :
  forall l ch sh,
    mint_spend_height_ok l ch sh = true ->
    coinbase_mature ch sh = true /\ script_lock_satisfied l sh = true.
Proof.
  intros l ch sh H.
  unfold mint_spend_height_ok in H.
  apply andb_true_iff in H. exact H.
Qed.

Lemma script_lock_rejects_early_spend :
  forall (l : ResMintLock) (h : Z),
    (h < mint_maturity l)%Z ->
    script_lock_satisfied l h = false.
Proof.
  intros l h Hlt.
  unfold script_lock_satisfied.
  apply Z.leb_gt. exact Hlt.
Qed.

(** CheckResRewardOwner: declared 20-byte equals mint script 20-byte. *)
Definition check_res_reward_owner (declared : list byte) (l : ResMintLock) : bool :=
  andb (mint_lock_ok l)
       (if list_eq_dec Nat.eq_dec declared (mint_pkh l) then true else false).

Theorem reward_owner_is_mint_field :
  forall d l,
    check_res_reward_owner d l = true ->
    d = mint_pkh l.
Proof.
  intros d l H.
  unfold check_res_reward_owner in H.
  apply andb_true_iff in H. destruct H as [_ Heq].
  destruct (list_eq_dec Nat.eq_dec d (mint_pkh l)); [assumption|discriminate Heq].
Qed.

Theorem reward_owner_equals_spend_predicate :
  forall d l,
    check_res_reward_owner d l = true ->
    d = mint_spend_predicate l.
Proof.
  intros d l H.
  apply reward_owner_is_mint_field in H. exact H.
Qed.

(** Mint script classifies as RES, ownership is OwnResMint. *)
Lemma canonical_mint_is_res_asset :
  classify canonical_res_mint = AssetRES.
Proof. apply mint_is_res. Qed.

Lemma canonical_mint_own_is_mint :
  ownership_of canonical_res_mint = OwnResMint (repeat_byte 20 1%nat).
Proof. apply canonical_mint_own. Qed.

Lemma canonical_mint_path_is_res_mint :
  ownership_path (ownership_of canonical_res_mint) = Some SpendResMint.
Proof. vm_compute. reflexivity. Qed.

(** Signature domain for mint is the mint tag, not ORU or RES v1. *)
Theorem mint_sighash_tag :
  tag_of_path SpendResMint = tag_mint.
Proof. reflexivity. Qed.

Theorem mint_tag_not_oru :
  tag_of_path SpendResMint <> tag_of_path SpendOruP2WPKH.
Proof. vm_compute. discriminate. Qed.

Theorem mint_tag_not_res_v1 :
  tag_of_path SpendResMint <> tag_of_path SpendResV1KeyPath.
Proof. vm_compute. discriminate. Qed.

(** Mint authorization still requires Ed25519. *)
Theorem mint_path_is_ed25519 :
  auth_of_spend SpendResMint = AuthEd25519.
Proof. reflexivity. Qed.

(** Unique spend attempt for a mint lock: path = SpendResMint, pkh binding. *)
Definition mint_attempt (l : ResMintLock) (sig pk msg : list byte) (crypto : bool)
  : SpendAttempt :=
  {| sa_path := SpendResMint;
     sa_sig := sig;
     sa_pk := pk;
     sa_msg := msg;
     sa_program_or_pkh := mint_pkh l;
     sa_crypto_accept := crypto |}.

Theorem mint_attempt_uses_spend_predicate :
  forall l sig pk msg crypto,
    sa_program_or_pkh (mint_attempt l sig pk msg crypto) = mint_spend_predicate l.
Proof. intros. reflexivity. Qed.

Theorem mint_attempt_not_oru_or_res_v1 :
  forall l sig pk msg crypto,
    sa_path (mint_attempt l sig pk msg crypto) <> SpendOruP2WPKH /\
    sa_path (mint_attempt l sig pk msg crypto) <> SpendResV1KeyPath.
Proof. intros. split; discriminate. Qed.

(** No second mint interpreter: the only mint constructor is SpendResMint. *)
Inductive MintInterpreter :=
| MintVerifyResMintSpend.

Theorem unique_mint_interpreter :
  forall i : MintInterpreter, i = MintVerifyResMintSpend.
Proof. intros i. destruct i. reflexivity. Qed.

(** * P0-023 Proof 3 — mint cannot bypass coinbase maturity.

    C++ [AccumulateTxInputPools]: coinbase spend requires
    [nSpendHeight - coin_height >= nCoinbaseMaturity] (100).
    Script lock is a second conjunct ([VerifyResMintSpend]). *)
Theorem mint_cannot_bypass_coinbase_maturity :
  forall (l : ResMintLock) (coin_height spend_height : Z),
    (spend_height < coin_height + coinbase_maturity)%Z ->
    mint_spend_height_ok l coin_height spend_height = false.
Proof.
  intros l ch sh Hlt.
  unfold mint_spend_height_ok, coinbase_mature.
  assert (Z.leb (ch + coinbase_maturity) sh = false) as Hm.
  { apply Z.leb_gt. exact Hlt. }
  rewrite Hm. reflexivity.
Qed.

Theorem mint_script_lock_cannot_bypass_maturity_height :
  forall (l : ResMintLock) (h : Z),
    (h < mint_maturity l)%Z ->
    script_lock_satisfied l h = false.
Proof. apply script_lock_rejects_early_spend. Qed.

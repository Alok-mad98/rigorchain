(** MakeKernelInput.v — marshalling boundary.

    Models the gates C++ MakeKernelInput applies before it may set
    crypto_accept. This is the raw-input → TransactionKernelInput
    formal boundary. It does not parse CScript; script classification,
    Hash160, and Ed25519 remain oracles, matching production.

    Connected to P058: to_tki is a field-wise projection of the raw
    record, with tki_crypto_accept := make_crypto_accept r. Gate theorems
    are proved here. C++ MakeKernelInput field agreement is
    VECTOR-TESTED (test_makekernelinput_field_map), not a Coq proof that
    the C++ marshaller equals this function.

    Bounded exhaustive domain: all_raw enumerates the abstract
    RawPredicateInput state space used by F-2.

    F-3: raw_sighash is a well-formed SighashPreimage record. Path tags
    (ORU / RES / MINT) are those of SighashPreimage.v. The signed
    message of a marshalled spend is sighash_digest of that record.
    Different 8-byte tags imply different preimages. crypto_accept
    requires sighash_path_consistent: mint cannot accept an ORU tag.
    path_sighash_tag matches C++ KernelAssignedSighashTag: unknown and
    non-native scripts get nil, not tag_oru. all_raw always writes the
    assigned tag (DEFINITIONAL). L-8 is load-bearing on tag_mismatch_pins.
    C++ VerifyRigorPredicateBoundary also rejects a mismatched tag.
    This is not a proof that C++ SignatureHash equals Coq for every
    transaction. C++ byte equality is VECTOR-TESTED against
    SignatureHash (MAKEKERNELINPUT_SIGHASH_EQ_SIGNATUREHASH).

    Production authority remains RigorConsensusKernel(C++).
    The spend-boundary theorem stays in TransactionKernelRefinement.v
    (over an already-marshalled TransactionKernelInput).

      extracted_kernel_production          = false
      compcert_binary_verified_kernel      = false
      compcert_vst_deferred                = true
      vst_scope_makekernelinput            = true
      vst_scope_full_kernel                = false

    No [Admitted] / [Axiom] / [Parameter] / [Extract Constant]. *)

From Coq Require Import ZArith.ZArith.
From Coq Require Import NArith.
From Coq Require Import Bool.Bool.
From Coq Require Import List.
From Coq Require Import Arith.PeanoNat.
Import ListNotations.

Require Import TransactionKernelRefinement.
From RigorV01 Require Import AssetClassification.
From RigorV01 Require Import SighashPreimage.
From Rigor Require Import RigorHash.

Local Open Scope Z_scope.

(** * Honesty *)

Definition extracted_kernel_production : bool := false.
Definition extracted_kernel_linked : bool := extracted_kernel_production.
Definition compcert_binary_verified_kernel : bool := false.

Theorem extracted_kernel_not_production : extracted_kernel_production = false.
Proof. reflexivity. Qed.

Theorem extracted_kernel_not_linked : extracted_kernel_linked = false.
Proof. reflexivity. Qed.

Theorem compcert_binary_not_verified : compcert_binary_verified_kernel = false.
Proof. reflexivity. Qed.

(** CompCert/VST is deferred. The only future candidate is MakeKernelInput
    (pure marshalling, no global state). Not the full kernel, not
    predicate_verify.cpp, not validation.cpp. *)
Definition compcert_vst_deferred : bool := true.
Definition vst_scope_makekernelinput : bool := true.
Definition vst_scope_full_kernel : bool := false.

Theorem vst_deferred_and_narrow :
  compcert_vst_deferred = true /\
  vst_scope_makekernelinput = true /\
  vst_scope_full_kernel = false /\
  compcert_binary_verified_kernel = false.
Proof. repeat split; reflexivity. Qed.

(** Frozen flag bits. Same positions as TransactionKernelRefinement.flag_*. *)

Definition raw_flag_sigpushonly (f : Z) : bool := Z.testbit f 5.
Definition raw_flag_witness (f : Z) : bool := Z.testbit f 11.

Definition witness_v0_keyhash_len : Z := 20.
Definition witness_v1_res_len : Z := 32.

(** * Raw spend, after the C++ classifiers but before crypto_accept. *)

Record RawPredicateInput := {
  raw_flags : Z;
  raw_scriptSig_empty : bool;
  raw_scriptSig_push_only : bool;
  raw_is_mint : bool;
  raw_witness_version : Z;
  raw_witness_program_len : Z;
  raw_is_res_native_v1 : bool;
  raw_encoding_ok : bool;
  raw_keyhash_match : bool;
  raw_witness_two : bool;
  raw_witness_items_ok : bool;
  raw_mint_parsed : bool;
  raw_mint_mature : bool;
  raw_crypto_ok : bool;
  raw_pubkey_eq_program : bool;
  raw_scriptPubKey : list byte;
  raw_scriptSig : list byte;
  raw_witness : list (list byte);
  raw_pubkey : list byte;
  raw_signature : list byte;
  raw_spend_height : Z;
  raw_stack_clean : bool;
  raw_sighash : SighashFields
}.

Definition pushonly_gate_ok (r : RawPredicateInput) : bool :=
  if raw_flag_sigpushonly (raw_flags r) then raw_scriptSig_push_only r else true.

Definition witness_version_ok (r : RawPredicateInput) : bool :=
  let v := raw_witness_version r in
  if v =? 0 then true
  else if v =? 1 then raw_witness_program_len r =? witness_v1_res_len
  else false.

Definition witness_gate_ok (r : RawPredicateInput) : bool :=
  andb (pushonly_gate_ok r)
    (andb (witness_version_ok r)
      (andb (raw_flag_witness (raw_flags r)) (raw_scriptSig_empty r))).

Fixpoint bytes_eqb (a b : list byte) : bool :=
  match a, b with
  | nil, nil => true
  | x :: xs, y :: ys => andb (Nat.eqb x y) (bytes_eqb xs ys)
  | _, _ => false
  end.

Definition path_sighash_tag (is_mint : bool) (ver : Z) (native : bool) : list byte :=
  if is_mint then tag_mint
  else if ver =? 0 then tag_oru
  else if (ver =? 1) && native then tag_res
  else nil.

Definition sighash_path_consistent (r : RawPredicateInput) : bool :=
  bytes_eqb (sh_tag (raw_sighash r))
            (path_sighash_tag (raw_is_mint r) (raw_witness_version r) (raw_is_res_native_v1 r)).

(** crypto_accept is false unless every pre-A1 decode / flag gate holds
    and the sighash tag matches the spend path. raw_crypto_ok is the
    Ed25519 oracle and is ignored when a gate fails. *)
Definition make_crypto_accept (r : RawPredicateInput) : bool :=
  if raw_is_mint r then
    andb (sighash_path_consistent r)
      (andb (pushonly_gate_ok r)
        (andb (raw_scriptSig_push_only r)
          (andb (raw_mint_parsed r)
            (andb (raw_mint_mature r)
              (andb (raw_encoding_ok r)
                (andb (raw_keyhash_match r) (raw_crypto_ok r)))))))
  else
    andb (sighash_path_consistent r)
      (andb (witness_gate_ok r)
        (andb (raw_encoding_ok r)
          (andb (raw_witness_two r)
            (andb (raw_witness_items_ok r)
              (andb (if raw_witness_version r =? 0 then
                       andb (raw_witness_program_len r =? witness_v0_keyhash_len)
                            (raw_keyhash_match r)
                     else if raw_witness_version r =? 1 then
                       andb (raw_is_res_native_v1 r) (raw_pubkey_eq_program r)
                     else false)
                    (raw_crypto_ok r)))))).

Definition zeros32 : list byte := repeat_byte 32 0%nat.

Definition canonical_sighash (is_mint : bool) (ver : Z) (native : bool) (script : list byte) : SighashFields := {|
  sh_tag := path_sighash_tag is_mint ver native;
  sh_chain_id := zeros32;
  sh_version := 2;
  sh_hash_prevouts := zeros32;
  sh_hash_sequence := zeros32;
  sh_hash_outputs := zeros32;
  sh_prevout_hash := zeros32;
  sh_prevout_n := 0;
  sh_script := script;
  sh_amount := 0;
  sh_sequence := 0;
  sh_locktime := 0
|}.

Definition to_tki (r : RawPredicateInput) : TransactionKernelInput := {|
  tki_scriptPubKey := raw_scriptPubKey r;
  tki_scriptSig := raw_scriptSig r;
  tki_witness := raw_witness r;
  tki_pubkey := raw_pubkey r;
  tki_signature := raw_signature r;
  tki_flags := raw_flags r;
  tki_spend_height := raw_spend_height r;
  tki_scriptSig_push_only := raw_scriptSig_push_only r;
  tki_stack_clean := raw_stack_clean r;
  tki_keyhash_match := raw_keyhash_match r;
  tki_crypto_accept := make_crypto_accept r;
  tki_sighash := raw_sighash r
|}.

(** * Acceptance monotonicity (test/fuzz safety property)

    NEW_ACCEPT=true && LEGACY_ACCEPT=false is a security violation.
    Diagnostic / legacy predicate is not a consensus authority. *)

Definition acceptance_grew (new_accept legacy_accept : bool) : bool :=
  andb new_accept (negb legacy_accept).

Theorem acceptance_grew_means_new_only :
  forall n l, acceptance_grew n l = true -> n = true /\ l = false.
Proof.
  intros n l H.
  unfold acceptance_grew in H.
  destruct n; destruct l; simpl in H; try discriminate H.
  split; reflexivity.
Qed.

(** * Decode / flag gates must not be bypassed into Ed25519. *)

Theorem mint_no_crypto_unless_push_only :
  forall r,
    raw_is_mint r = true ->
    raw_scriptSig_push_only r = false ->
    make_crypto_accept r = false.
Proof.
  intros r Hmint Hpush.
  unfold make_crypto_accept.
  rewrite Hmint.
  rewrite Hpush.
  destruct (sighash_path_consistent r); destruct (pushonly_gate_ok r); reflexivity.
Qed.

Theorem witness_no_crypto_without_flag :
  forall r,
    raw_is_mint r = false ->
    raw_flag_witness (raw_flags r) = false ->
    make_crypto_accept r = false.
Proof.
  intros r Hmint Hflag.
  unfold make_crypto_accept, witness_gate_ok.
  rewrite Hmint.
  rewrite Hflag.
  destruct (sighash_path_consistent r);
  destruct (pushonly_gate_ok r); destruct (witness_version_ok r);
    destruct (raw_scriptSig_empty r); reflexivity.
Qed.

Theorem witness_no_crypto_if_scriptSig :
  forall r,
    raw_is_mint r = false ->
    raw_scriptSig_empty r = false ->
    make_crypto_accept r = false.
Proof.
  intros r Hmint Hsig.
  unfold make_crypto_accept, witness_gate_ok.
  rewrite Hmint.
  rewrite Hsig.
  destruct (sighash_path_consistent r);
  destruct (pushonly_gate_ok r); destruct (witness_version_ok r);
    destruct (raw_flag_witness (raw_flags r)); reflexivity.
Qed.

Theorem crypto_accept_implies_oracle :
  forall r, make_crypto_accept r = true -> raw_crypto_ok r = true.
Proof.
  intros r H.
  unfold make_crypto_accept in H.
  destruct (raw_is_mint r);
    repeat (apply andb_prop in H; destruct H as [? H]);
    assumption.
Qed.

Theorem crypto_accept_implies_path_tag :
  forall r, make_crypto_accept r = true -> sighash_path_consistent r = true.
Proof.
  intros r H.
  unfold make_crypto_accept in H.
  destruct (raw_is_mint r);
    apply andb_prop in H; destruct H as [Hc _];
    exact Hc.
Qed.

(** DEFINITIONAL: unfolds tki_crypto_accept. Not a ∀-content theorem. *)
Theorem to_tki_crypto_accept :
  forall r, tki_crypto_accept (to_tki r) = make_crypto_accept r.
Proof. intros r. reflexivity. Qed.

(** DEFINITIONAL: flag bit projection. *)
Theorem raw_flag_witness_to_tki :
  forall r, raw_flag_witness (raw_flags r) = flag_witness (to_tki r).
Proof. intros r. reflexivity. Qed.

Lemma andb_false_r : forall b : bool, andb b false = false.
Proof. intros []; reflexivity. Qed.

Lemma cxx_oru_inner_no_crypto :
  forall i, tki_crypto_accept i = false -> cxx_oru_inner i = false.
Proof.
  intros i H. unfold cxx_oru_inner. rewrite H.
  repeat rewrite andb_false_r. reflexivity.
Qed.

Lemma cxx_witness_no_crypto :
  forall i v prog,
    tki_crypto_accept i = false -> cxx_witness i v prog = false.
Proof.
  intros i v prog H.
  unfold cxx_witness.
  destruct (andb (negb (Nat.eqb v 0%nat)) (negb (Nat.eqb v 1%nat))); [reflexivity|].
  destruct (andb (Nat.eqb v 1%nat) (negb (Nat.eqb (length prog) 32%nat))); [reflexivity|].
  destruct (negb (flag_witness i)); [reflexivity|].
  destruct (negb (Nat.eqb (length (tki_scriptSig i)) 0%nat)); [reflexivity|].
  destruct (Nat.eqb v 0%nat).
  - destruct (Nat.eqb (length prog) 20%nat); [apply cxx_oru_inner_no_crypto; exact H | reflexivity].
  - match goal with
    | |- (if ?cond then _ else _) = false => destruct cond
    end.
    + unfold cxx_res_inner.
      destruct (tki_witness i); [reflexivity|].
      rewrite H. repeat rewrite andb_false_r. reflexivity.
    + reflexivity.
Qed.

Lemma cxx_mint_no_crypto :
  forall i, tki_crypto_accept i = false -> cxx_mint i = false.
Proof.
  intros i H.
  unfold cxx_mint.
  destruct (negb (tki_scriptSig_push_only i)); [reflexivity|].
  match goal with
  | |- match ?p with Some _ => _ | None => _ end = false => destruct p as [[mat pkh]|]
  end; [|reflexivity].
  destruct (mat <? 0); [reflexivity|].
  destruct (tki_spend_height i <? mat); [reflexivity|].
  destruct (negb (encoding_ok i)); [reflexivity|].
  destruct (negb (tki_keyhash_match i)); [reflexivity|].
  rewrite H. reflexivity.
Qed.

Theorem crypto_accept_false_blocks_spend :
  forall i, tki_crypto_accept i = false -> spend_allowed i = false.
Proof.
  intros i H.
  unfold spend_allowed, Coq_spend_allowed.
  destruct (negb (spend_gate_sigpush i)); [reflexivity|].
  match goal with
  | |- match ?o with _ => _ end = false => destruct o
  end;
    try reflexivity;
    try (apply cxx_witness_no_crypto; exact H);
    try (apply cxx_mint_no_crypto; exact H).
Qed.

Theorem make_crypto_reject_blocks_spend :
  forall r, make_crypto_accept r = false -> spend_allowed (to_tki r) = false.
Proof.
  intros r H.
  apply crypto_accept_false_blocks_spend.
  rewrite to_tki_crypto_accept. exact H.
Qed.

(** DEFINITIONAL: field projections of to_tki. *)
Theorem to_tki_projects_flags :
  forall r, tki_flags (to_tki r) = raw_flags r.
Proof. intros. reflexivity. Qed.

Theorem to_tki_projects_stack_clean :
  forall r, tki_stack_clean (to_tki r) = raw_stack_clean r.
Proof. intros. reflexivity. Qed.

Theorem to_tki_projects_spend_height :
  forall r, tki_spend_height (to_tki r) = raw_spend_height r.
Proof. intros. reflexivity. Qed.

Theorem to_tki_projects_sighash :
  forall r, tki_sighash (to_tki r) = raw_sighash r.
Proof. intros. reflexivity. Qed.

(** The message Ed25519 signs is SHA256d of the canonical preimage. *)
Definition kernel_signature_message (i : TransactionKernelInput) : list byte :=
  sighash_digest (tki_sighash i).

(** DEFINITIONAL: signed message is sighash_digest of the marshalled record. *)
Theorem spend_message_is_sighash_digest :
  forall r, kernel_signature_message (to_tki r) = sighash_digest (raw_sighash r).
Proof. intros. reflexivity. Qed.

(** DEFINITIONAL: kernel_signature_message unfolds to hash256_bytes. *)
Theorem spend_message_is_coq_sighash_digest :
  forall i, kernel_signature_message i = hash256_bytes (CoqSighash (tki_sighash i)).
Proof.
  intros i.
  unfold kernel_signature_message, CoqSighash, sighash_digest.
  reflexivity.
Qed.

Theorem kernel_signature_message_len :
  forall i, length (kernel_signature_message i) = 32%nat.
Proof.
  intros i.
  unfold kernel_signature_message, sighash_digest.
  apply hash256_bytes_length.
Qed.

Lemma app_length_inj :
  forall (A : Type) (a1 a2 b1 b2 : list A),
    length a1 = length a2 ->
    a1 ++ b1 = a2 ++ b2 ->
    a1 = a2 /\ b1 = b2.
Proof.
  intros A a1.
  induction a1 as [|x xs IH]; intros a2 b1 b2 Hlen Heq.
  - destruct a2; [|discriminate Hlen]. split; [reflexivity|exact Heq].
  - destruct a2 as [|y ys]; [discriminate Hlen|].
    simpl in Heq. injection Heq as Hy Hrest. subst y.
    simpl in Hlen. apply Nat.succ_inj in Hlen.
    destruct (IH ys b1 b2 Hlen Hrest) as [Ha Hb].
    subst. split; reflexivity.
Qed.

(** Distinct from SignatureDomainSeparation.different_tags_different_preimages,
    which uses a pre-serialized-bytes SighashFields. This theorem is over
    SighashPreimage.SighashFields (typed integers + compact_size). *)
Theorem different_tags_different_preimages :
  forall f1 f2,
    length (sh_tag f1) = 8%nat ->
    length (sh_tag f2) = 8%nat ->
    sh_tag f1 <> sh_tag f2 ->
    sighash_preimage f1 <> sighash_preimage f2.
Proof.
  intros f1 f2 L1 L2 Hne Heq.
  unfold sighash_preimage in Heq.
  apply app_length_inj in Heq; [|rewrite L1, L2; reflexivity].
  destruct Heq as [Ht _].
  contradiction.
Qed.

(** DEFINITIONAL: the three ASCII tags are 8 bytes. *)
Theorem path_tags_are_eight :
  length tag_oru = 8%nat /\ length tag_res = 8%nat /\ length tag_mint = 8%nat.
Proof. repeat split; reflexivity. Qed.

(** * F-2 bounded exhaustive domain

    Nest order (false then true), matching test_makekernelinput_field_map.cpp:
    flags_push, flags_wit, scriptSig_empty, scriptSig_push_only, is_mint,
    is_res_native, encoding_ok, keyhash_match, witness_two, crypto_ok,
    then version in [0;1;2], then program_len in [0;20;32].

    witness_items_ok / mint_parsed / mint_mature default true here; the
    C++ field-map grid covers their false cases. Byte fields are empty
    placeholders except raw_sighash, which is the well-formed path-tagged
    record (F-3). This domain is the abstract gate state, not CScript. *)

Definition domain_bools : list bool := [false; true].
Definition domain_versions : list Z := [0; 1; 2].
Definition domain_lens : list Z := [0; 20; 32].

Definition raw_from_domain
  (flags_push flags_wit empty push is_mint native enc kh two crypto : bool)
  (ver len : Z) : RawPredicateInput := {|
  raw_flags := (if flags_push then 32 else 0) + (if flags_wit then 2048 else 0);
  raw_scriptSig_empty := empty;
  raw_scriptSig_push_only := push;
  raw_is_mint := is_mint;
  raw_witness_version := ver;
  raw_witness_program_len := len;
  raw_is_res_native_v1 := native;
  raw_encoding_ok := enc;
  raw_keyhash_match := kh;
  raw_witness_two := two;
  raw_witness_items_ok := true;
  raw_mint_parsed := true;
  raw_mint_mature := true;
  raw_crypto_ok := crypto;
  raw_pubkey_eq_program := true;
  raw_scriptPubKey := nil;
  raw_scriptSig := if empty then nil else [1%nat];
  raw_witness := nil;
  raw_pubkey := nil;
  raw_signature := nil;
  raw_spend_height := 0;
  raw_stack_clean := true;
  raw_sighash := canonical_sighash is_mint ver native nil
|}.

Definition pin_v0_accept :=
  raw_from_domain false true true true false true true true true true 0 20.
Lemma pin_v0_accept_crypto : make_crypto_accept pin_v0_accept = true.
Proof. vm_compute. reflexivity. Qed.

Definition pin_v0_no_witness_flag :=
  raw_from_domain false false true true false true true true true true 0 20.
Lemma pin_v0_no_witness_flag_crypto : make_crypto_accept pin_v0_no_witness_flag = false.
Proof. vm_compute. reflexivity. Qed.

Definition pin_mint_accept :=
  raw_from_domain false false true true true false true true true true 0 20.
Lemma pin_mint_accept_crypto : make_crypto_accept pin_mint_accept = true.
Proof. vm_compute. reflexivity. Qed.

Definition pin_v1_accept :=
  raw_from_domain false true true true false true true true true true 1 32.
Lemma pin_v1_accept_crypto : make_crypto_accept pin_v1_accept = true.
Proof. vm_compute. reflexivity. Qed.

(** DEFINITIONAL: raw_from_domain writes path_sighash_tag. *)
Theorem domain_raw_uses_path_tag :
  forall flags_push flags_wit empty push is_mint native enc kh two crypto ver len,
    sh_tag (raw_sighash (raw_from_domain flags_push flags_wit empty push is_mint native
                         enc kh two crypto ver len)) =
    path_sighash_tag is_mint ver native.
Proof. intros. reflexivity. Qed.

Theorem mint_oru_preimages_differ :
  sighash_preimage (raw_sighash pin_mint_accept) <>
  sighash_preimage (raw_sighash pin_v0_accept).
Proof. vm_compute. discriminate. Qed.

Theorem oru_res_preimages_differ :
  sighash_preimage (raw_sighash pin_v0_accept) <>
  sighash_preimage (raw_sighash pin_v1_accept).
Proof. vm_compute. discriminate. Qed.

Theorem mint_res_preimages_differ :
  sighash_preimage (raw_sighash pin_mint_accept) <>
  sighash_preimage (raw_sighash pin_v1_accept).
Proof. vm_compute. discriminate. Qed.

(** DEFINITIONAL: the three pin records carry ORU / RES / MINT tags. *)
Theorem pin_tags_domain_separated :
  sh_tag (raw_sighash pin_v0_accept) = tag_oru /\
  sh_tag (raw_sighash pin_v1_accept) = tag_res /\
  sh_tag (raw_sighash pin_mint_accept) = tag_mint.
Proof. repeat split; reflexivity. Qed.

(** L-8 observable domain: assigned tag overwritten. Not in all_raw. *)
Definition set_raw_tag (r : RawPredicateInput) (tag : list byte) : RawPredicateInput :=
  let sh := raw_sighash r in
  {|
    raw_flags := raw_flags r;
    raw_scriptSig_empty := raw_scriptSig_empty r;
    raw_scriptSig_push_only := raw_scriptSig_push_only r;
    raw_is_mint := raw_is_mint r;
    raw_witness_version := raw_witness_version r;
    raw_witness_program_len := raw_witness_program_len r;
    raw_is_res_native_v1 := raw_is_res_native_v1 r;
    raw_encoding_ok := raw_encoding_ok r;
    raw_keyhash_match := raw_keyhash_match r;
    raw_witness_two := raw_witness_two r;
    raw_witness_items_ok := raw_witness_items_ok r;
    raw_mint_parsed := raw_mint_parsed r;
    raw_mint_mature := raw_mint_mature r;
    raw_crypto_ok := raw_crypto_ok r;
    raw_pubkey_eq_program := raw_pubkey_eq_program r;
    raw_scriptPubKey := raw_scriptPubKey r;
    raw_scriptSig := raw_scriptSig r;
    raw_witness := raw_witness r;
    raw_pubkey := raw_pubkey r;
    raw_signature := raw_signature r;
    raw_spend_height := raw_spend_height r;
    raw_stack_clean := raw_stack_clean r;
    raw_sighash := {|
      sh_tag := tag;
      sh_chain_id := sh_chain_id sh;
      sh_version := sh_version sh;
      sh_hash_prevouts := sh_hash_prevouts sh;
      sh_hash_sequence := sh_hash_sequence sh;
      sh_hash_outputs := sh_hash_outputs sh;
      sh_prevout_hash := sh_prevout_hash sh;
      sh_prevout_n := sh_prevout_n sh;
      sh_script := sh_script sh;
      sh_amount := sh_amount sh;
      sh_sequence := sh_sequence sh;
      sh_locktime := sh_locktime sh
    |}
  |}.

Definition pin_mint_oru_tag := set_raw_tag pin_mint_accept tag_oru.
Definition pin_v0_res_tag := set_raw_tag pin_v0_accept tag_res.
Definition pin_v1_oru_tag := set_raw_tag pin_v1_accept tag_oru.

Definition tag_mismatch_pins : list RawPredicateInput :=
  [pin_mint_oru_tag; pin_v0_res_tag; pin_v1_oru_tag].

Theorem unknown_version_empty_tag :
  path_sighash_tag false 2 false = nil.
Proof. reflexivity. Qed.

Theorem non_native_v1_empty_tag :
  path_sighash_tag false 1 false = nil.
Proof. reflexivity. Qed.

Theorem mint_oru_tag_rejected :
  make_crypto_accept pin_mint_oru_tag = false.
Proof. vm_compute. reflexivity. Qed.

Theorem v0_res_tag_rejected :
  make_crypto_accept pin_v0_res_tag = false.
Proof. vm_compute. reflexivity. Qed.

Theorem v1_oru_tag_rejected :
  make_crypto_accept pin_v1_oru_tag = false.
Proof. vm_compute. reflexivity. Qed.

Theorem tag_mismatch_all_inconsistent :
  forallb (fun r => negb (sighash_path_consistent r)) tag_mismatch_pins = true.
Proof. vm_compute. reflexivity. Qed.

Theorem tag_mismatch_all_reject :
  forallb (fun r => negb (make_crypto_accept r)) tag_mismatch_pins = true.
Proof. vm_compute. reflexivity. Qed.

Theorem tag_mismatch_nonempty :
  length tag_mismatch_pins = 3%nat.
Proof. reflexivity. Qed.

Definition all_raw : list RawPredicateInput :=
  flat_map (fun flags_push =>
  flat_map (fun flags_wit =>
  flat_map (fun empty =>
  flat_map (fun push =>
  flat_map (fun is_mint =>
  flat_map (fun native =>
  flat_map (fun enc =>
  flat_map (fun kh =>
  flat_map (fun two =>
  flat_map (fun crypto =>
  flat_map (fun ver =>
    map (fun len =>
      raw_from_domain flags_push flags_wit empty push is_mint native enc kh two crypto ver len)
      domain_lens)
    domain_versions)
    domain_bools) domain_bools) domain_bools) domain_bools) domain_bools)
    domain_bools) domain_bools) domain_bools) domain_bools) domain_bools.

(** Encode as binary [N] so Coq does not emit [abstract-large-number]. *)
Definition domain_size_nat : nat := N.to_nat 9216%N.

Theorem all_raw_size : length all_raw = domain_size_nat.
Proof. vm_compute. reflexivity. Qed.

Definition domain_crypto_accept : list bool := map make_crypto_accept all_raw.

Theorem domain_reject_blocks_spend :
  forall r, In r all_raw ->
    make_crypto_accept r = false -> spend_allowed (to_tki r) = false.
Proof.
  intros r _ H. apply make_crypto_reject_blocks_spend. exact H.
Qed.


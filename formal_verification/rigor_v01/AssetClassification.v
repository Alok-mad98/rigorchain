(** RIGOR v0.1 — Property 1: Asset Classification Uniqueness.

    Unique model (matches C++ [AssetTypeFromScriptPubKey] in
    [core/src/script/rigor_classify.cpp]):

      CTxOut = (value, scriptPubKey)
           ↓
      AssetType = classify(scriptPubKey)

    There is no second argument (height, address, descriptor, witness
    version taken alone, explorer table). Unrecognized scripts are
    [AssetInvalid], never a fallback to ORU / RES / NONE.

    Option B: a native witness v1 / 32 program *is* the Ed25519 public
    key. This classifier never computes SHA256(pk) (Option A, OBSOLETE).

    Live [classify] uses the unique C++ GetResMintScript encoding
    (OP_PUSHBYTES_1 0x05 accepted; OP_5 rejected). Finite golden
    vectors lock C++ ClassifyRigorTxout against this function.
    This is not a ∀-byte VST refinement of the C++ classifier.
    No [Axiom] / [Admitted]. *)
From Coq Require Import List.
From Coq Require Import Bool.Bool.
From Coq Require Import Arith.PeanoNat.
From Coq Require Import ZArith.ZArith.
From Coq Require Import Lia.
Import ListNotations.

Local Open Scope Z_scope.

Definition byte := nat.

Fixpoint repeat_byte (n : nat) (v : byte) : list byte :=
  match n with
  | O => nil
  | S n' => v :: repeat_byte n' v
  end.

Fixpoint app_bytes (xs ys : list byte) : list byte :=
  match xs with
  | nil => ys
  | x :: xrest => x :: app_bytes xrest ys
  end.

Fixpoint split_at (n : nat) (xs : list byte) : option (list byte * list byte) :=
  match n, xs with
  | O, _ => Some (nil, xs)
  | S n', x :: xs' =>
      match split_at n' xs' with
      | Some (pre, post) => Some (x :: pre, post)
      | None => None
      end
  | S _, nil => None
  end.

Definition RIGOR_PUSH_0 : byte := 0%nat.
Definition RIGOR_PUSH_1 : byte := 81%nat.
Definition RIGOR_PUSH_16 : byte := 96%nat.
Definition RIGOR_RESERVED_LEGACY_DUP : byte := 118%nat.
Definition RIGOR_RESERVED_LEGACY_HASH160 : byte := 169%nat.
Definition RIGOR_RESERVED_LEGACY_EQUAL : byte := 135%nat.
Definition RIGOR_RESERVED_LEGACY_EQUALVERIFY : byte := 136%nat.
Definition RIGOR_RESERVED_LEGACY_CHECKSIG : byte := 172%nat.
Definition RIGOR_MARKER_RETURN : byte := 106%nat.
Definition RIGOR_PUSHBYTES_20 : byte := 20%nat.
Definition RIGOR_PUSHBYTES_32 : byte := 32%nat.
Definition RIGOR_PUSHDATA1 : byte := 76%nat.
Definition RIGOR_PUSHDATA2 : byte := 77%nat.
Definition RIGOR_PUSHDATA4 : byte := 78%nat.
Definition RIGOR_MARKER_RES_MINT : byte := 187%nat.
Definition RIGOR_MARKER_CONVERGE : byte := 189%nat.
Definition RIGOR_RESERVED_LEGACY_1NEGATE : byte := 79%nat.

Definition cvg2_tag : list byte := [67; 86; 71; 50]%nat.
Definition res2_tag : list byte := [82; 69; 83; 50]%nat.

(** * Asset type — the only classification result *)

Inductive AssetType :=
| AssetORU
| AssetRES
| AssetNone
| AssetInvalid.

Definition asset_eqb (a b : AssetType) : bool :=
  match a, b with
  | AssetORU, AssetORU => true
  | AssetRES, AssetRES => true
  | AssetNone, AssetNone => true
  | AssetInvalid, AssetInvalid => true
  | _, _ => false
  end.

(** * Witness-program container (BIP141-shaped; v1 is not BIP341 / Taproot) *)

Definition witness_version_of (b : byte) : option nat :=
  if Nat.eqb b RIGOR_PUSH_0 then Some 0%nat
  else if andb (Nat.leb RIGOR_PUSH_1 b) (Nat.leb b RIGOR_PUSH_16)
       then Some (b - 80)%nat
       else None.

Definition parse_witness_program (spk : list byte) : option (nat * list byte) :=
  let n := length spk in
  if orb (Nat.ltb n 4%nat) (Nat.ltb 42%nat n) then None
  else
    match spk with
    | ver :: plen :: rest =>
        match witness_version_of ver with
        | None => None
        | Some v =>
            if andb (Nat.ltb 0%nat plen) (Nat.eqb (length rest) plen)
            then Some (v, rest)
            else None
        end
    | _ => None
    end.

Definition prefix4_eq (prog tag : list byte) : bool :=
  match prog, tag with
  | a :: b :: c :: d :: _, x :: y :: z :: w :: nil =>
      andb (Nat.eqb a x) (andb (Nat.eqb b y) (andb (Nat.eqb c z) (Nat.eqb d w)))
  | _, _ => false
  end.

(** C++ [IsRigorResNativeWitnessProgram]: 32 bytes, neither CVG2 nor RES2.
    Option B: these 32 bytes *are* the public key. No SHA256. *)
Definition is_native_res_program (prog : list byte) : bool :=
  andb (Nat.eqb (length prog) 32%nat)
       (andb (negb (prefix4_eq prog cvg2_tag))
             (negb (prefix4_eq prog res2_tag))).

(** Option B constructor: program := pk. Never SHA256(pk). *)
Definition res_witness_program (pk : list byte) : list byte := pk.

Theorem option_b_program_is_pubkey :
  forall pk : list byte, res_witness_program pk = pk.
Proof. intros. reflexivity. Qed.

(** * RIGOR_MARKER_RES_MINT parser — unique encoding of C++ GetResMintScript.

    C++ [CScript << CScriptNum] is [<< getvch()]: locktimes 1–16 are
    [OP_PUSHBYTES_n || n], NOT OP_1..OP_16. OP_n is a second encoding and
    fails [script == GetResMintScript]. *)

Fixpoint pos_le_bytes (n : Z) (fuel : nat) : list byte :=
  match fuel with
  | O => nil
  | S f =>
      if n =? 0 then nil
      else Z.to_nat (n mod 256) :: pos_le_bytes (n / 256) f
  end.

Definition scriptnum_payload (n : Z) : list byte :=
  let raw := pos_le_bytes n 5%nat in
  match rev raw with
  | nil => nil
  | last :: _ =>
      if Nat.leb 128 last then app_bytes raw [0%nat] else raw
  end.

Definition encode_scriptnum (n : Z) : list byte :=
  if n <? 0 then [RIGOR_RESERVED_LEGACY_1NEGATE]
  else
    let p := scriptnum_payload n in
    match p with
    | nil => [RIGOR_PUSH_0]
    | _ => length p :: p
    end.

Fixpoint le_to_Z (bs : list byte) : Z :=
  match bs with
  | nil => 0
  | b :: rest => Z.of_nat b + 256 * le_to_Z rest
  end.

Definition scriptnum_to_Z (payload : list byte) : Z :=
  match rev payload with
  | O :: rest => le_to_Z (rev rest)
  | _ => le_to_Z payload
  end.

Definition decode_scriptnum_prefix (spk : list byte) : option (Z * list byte) :=
  match spk with
  | nil => None
  | b :: r =>
      if Nat.eqb b RIGOR_PUSH_0 then Some (0, r)
      else if andb (Nat.leb 1 b) (Nat.leb b 4) then
        match split_at b r with
        | Some (payload, rest) =>
            let v := scriptnum_to_Z payload in
            if list_eq_dec Nat.eq_dec (encode_scriptnum v) (b :: payload)
            then Some (v, rest)
            else None
        | None => None
        end
      else None
  end.

Definition parse_canonical_mint (spk : list byte)
  : option (Z * list byte) :=
  match decode_scriptnum_prefix spk with
  | None => None
  | Some (maturity, rest) =>
      match rest with
      | plen :: body =>
          if Nat.eqb plen RIGOR_PUSHBYTES_20 then
            match split_at 20%nat body with
            | Some (pkh, marker_tl) =>
                match marker_tl with
                | op :: nil =>
                    if Nat.eqb op RIGOR_MARKER_RES_MINT then
                      Some (maturity, pkh)
                    else None
                | _ => None
                end
            | None => None
            end
          else None
      | nil => None
      end
  end.

Definition encode_canonical_mint (maturity : Z) (pkh : list byte) : list byte :=
  app_bytes (encode_scriptnum maturity)
            (RIGOR_PUSHBYTES_20 :: app_bytes pkh [RIGOR_MARKER_RES_MINT]).

Definition is_res_mint_script (spk : list byte) : bool :=
  match parse_canonical_mint spk with
  | Some _ => true
  | None => false
  end.

Definition parse_res_mint_pubkey_hash (spk : list byte) : option (list byte) :=
  match parse_canonical_mint spk with
  | Some (_, pkh) => Some pkh
  | None => None
  end.

(** * RIGOR_MARKER_CONVERGE: <20-byte HASH160> RIGOR_MARKER_CONVERGE *)

Definition is_converge_script (spk : list byte) : bool :=
  match spk with
  | plen :: rest =>
      andb (Nat.eqb plen RIGOR_PUSHBYTES_20)
           (andb (Nat.eqb (length rest) 21%nat)
                 (match rev rest with
                  | op :: hash_rev =>
                      andb (Nat.eqb op RIGOR_MARKER_CONVERGE)
                           (Nat.eqb (length hash_rev) 20%nat)
                  | nil => false
                  end))
  | nil => false
  end.

(** * RIGOR_MARKER_RETURN / NULL_DATA (push-only remainder) *)

Fixpoint is_push_only_n (fuel : nat) (rest : list byte) : bool :=
  match fuel with
  | O => match rest with nil => true | _ => false end
  | S fuel' =>
      match rest with
      | nil => true
      | op :: tail =>
          if Nat.leb op 75%nat then
            match split_at op tail with
            | Some (_, rest') => is_push_only_n fuel' rest'
            | None => false
            end
          else if Nat.eqb op RIGOR_PUSHDATA1 then
            match tail with
            | ln :: data =>
                match split_at ln data with
                | Some (_, rest') => is_push_only_n fuel' rest'
                | None => false
                end
            | nil => false
            end
          else if Nat.eqb op RIGOR_PUSHDATA2 then
            match tail with
            | lo :: hi :: data =>
                match split_at (lo + hi * 256)%nat data with
                | Some (_, rest') => is_push_only_n fuel' rest'
                | None => false
                end
            | _ => false
            end
          else if Nat.eqb op RIGOR_PUSHDATA4 then
            match tail with
            | a :: b :: c :: d :: data =>
                match split_at
                        (a + b * 256 + c * 256 * 256 + d * 256 * 256 * 256)%nat
                        data with
                | Some (_, rest') => is_push_only_n fuel' rest'
                | None => false
                end
            | _ => false
            end
          else if Nat.leb op RIGOR_PUSH_16 then
            is_push_only_n fuel' tail
          else false
      end
  end.

Definition is_push_only (rest : list byte) : bool :=
  is_push_only_n (length rest) rest.

Definition is_op_return (spk : list byte) : bool :=
  match spk with
  | x :: rest => andb (Nat.eqb x RIGOR_MARKER_RETURN) (is_push_only rest)
  | nil => false
  end.

(** * THE unique classifier — Property 1 *)

Definition classify (spk : list byte) : AssetType :=
  match parse_witness_program spk with
  | Some (0%nat, prog) =>
      if Nat.eqb (length prog) 20%nat then AssetORU else AssetInvalid
  | Some (1%nat, prog) =>
      if is_native_res_program prog then AssetRES else AssetInvalid
  | Some _ => AssetInvalid
  | None =>
      if is_res_mint_script spk then AssetRES
      else if is_converge_script spk then AssetORU
      else if is_op_return spk then AssetNone
      else AssetInvalid
  end.

Definition asset_type_from_scriptpubkey (spk : list byte) : AssetType :=
  classify spk.

Theorem unique_classifier_is_classify :
  forall spk : list byte,
    asset_type_from_scriptpubkey spk = classify spk.
Proof. intros. reflexivity. Qed.

Theorem classify_is_a_function_of_script_only :
  forall spk1 spk2 : list byte,
    spk1 = spk2 -> classify spk1 = classify spk2.
Proof. intros spk1 spk2 H. rewrite H. reflexivity. Qed.

(** Forbidden second sources: they are not functions that return an asset.
    Witness version, address, descriptor, and explorer tables do not
    assign [AssetType] in this model. *)

Definition asset_from_witness_version (_ver : nat) : option AssetType := None.
Definition asset_from_address (_addr : list byte) : option AssetType := None.
Definition asset_from_descriptor (_desc : list byte) : option AssetType := None.
Definition asset_from_explorer_table (_spk : list byte) : option AssetType := None.

Theorem no_asset_from_witness_version :
  forall v : nat, asset_from_witness_version v = None.
Proof. intros. reflexivity. Qed.

Theorem no_asset_from_address :
  forall a : list byte, asset_from_address a = None.
Proof. intros. reflexivity. Qed.

Theorem no_asset_from_descriptor :
  forall d : list byte, asset_from_descriptor d = None.
Proof. intros. reflexivity. Qed.

Theorem no_asset_from_explorer :
  forall s : list byte, asset_from_explorer_table s = None.
Proof. intros. reflexivity. Qed.

(** * Canonical fixtures *)

Definition rfc8032_pk1 : list byte :=
  [215; 90; 152; 1; 130; 177; 10; 183;
   213; 75; 254; 211; 201; 100; 7; 58;
   14; 225; 114; 243; 218; 166; 35; 37;
   175; 2; 26; 104; 247; 7; 81; 26]%nat.

Definition canonical_p2wpkh : list byte :=
  RIGOR_PUSH_0 :: RIGOR_PUSHBYTES_20 :: repeat_byte 20 0%nat.

Definition canonical_native_res : list byte :=
  RIGOR_PUSH_1 :: RIGOR_PUSHBYTES_32 :: rfc8032_pk1.

Definition canonical_p2wsh : list byte :=
  RIGOR_PUSH_0 :: RIGOR_PUSHBYTES_32 :: repeat_byte 32 0%nat.

Definition canonical_res2 : list byte :=
  RIGOR_PUSH_1 :: RIGOR_PUSHBYTES_32 :: app_bytes res2_tag (repeat_byte 28 0%nat).

Definition canonical_cvg2 : list byte :=
  RIGOR_PUSH_1 :: RIGOR_PUSHBYTES_32 :: app_bytes cvg2_tag (repeat_byte 28 0%nat).

Definition canonical_p2pkh : list byte :=
  RIGOR_RESERVED_LEGACY_DUP :: RIGOR_RESERVED_LEGACY_HASH160 :: RIGOR_PUSHBYTES_20
        :: app_bytes (repeat_byte 20 0%nat) [RIGOR_RESERVED_LEGACY_EQUALVERIFY; RIGOR_RESERVED_LEGACY_CHECKSIG].

Definition canonical_converge : list byte :=
  RIGOR_PUSHBYTES_20 :: app_bytes (repeat_byte 20 0%nat) [RIGOR_MARKER_CONVERGE].

Definition canonical_res_mint : list byte :=
  RIGOR_PUSH_0 :: RIGOR_PUSHBYTES_20 :: app_bytes (repeat_byte 20 1%nat) [RIGOR_MARKER_RES_MINT].

Definition canonical_op_return : list byte := [RIGOR_MARKER_RETURN].

Lemma p2wpkh_is_oru :
  classify canonical_p2wpkh = AssetORU.
Proof. vm_compute. reflexivity. Qed.

Lemma native_res_is_res :
  classify canonical_native_res = AssetRES.
Proof. vm_compute. reflexivity. Qed.

Lemma native_res_program_eq_pk :
  match parse_witness_program canonical_native_res with
  | Some (1%nat, prog) => prog = rfc8032_pk1
  | _ => False
  end.
Proof. vm_compute. reflexivity. Qed.

Lemma mint_is_res :
  classify canonical_res_mint = AssetRES.
Proof. vm_compute. reflexivity. Qed.

(** Frozen encoding lock (P0-039): OP_PUSHBYTES_1 0x05 is canonical; OP_5 is not. *)
Theorem pushbytes_1_05_mint_is_res :
  classify (encode_canonical_mint 5 (repeat_byte 20 17%nat)) = AssetRES.
Proof. vm_compute. reflexivity. Qed.

Theorem op5_mint_is_invalid :
  classify (85%nat :: 20%nat :: app_bytes (repeat_byte 20 17%nat) [RIGOR_MARKER_RES_MINT])
    = AssetInvalid.
Proof. vm_compute. reflexivity. Qed.

(** * P0-040 reusable parser lemmas (live [classify] parser, not extra vectors) *)

Lemma encode_scriptnum_5_is_pushbytes_1_05 :
  encode_scriptnum 5 = [1%nat; 5%nat].
Proof. vm_compute. reflexivity. Qed.

Lemma encode_scriptnum_5_is_not_op5 :
  encode_scriptnum 5 <> [85%nat].
Proof. vm_compute. discriminate. Qed.

(** C++ OP_1..OP_16 = 81..96. Those opcodes are not a CScriptNum prefix, so they
    cannot be the unique [GetResMintScript] encoding. OP_5 (85) is the mint case. *)
Lemma op1_to_op16_not_scriptnum_prefix :
  forall (op : nat) (rest : list byte),
    Nat.leb 81%nat op = true ->
    Nat.leb op 96%nat = true ->
    decode_scriptnum_prefix (op :: rest) = None.
Proof.
  intros op rest H81 H96.
  unfold decode_scriptnum_prefix.
  destruct (Nat.eqb op RIGOR_PUSH_0) eqn:Ez.
  - apply Nat.eqb_eq in Ez. unfold RIGOR_PUSH_0 in Ez. subst op.
    discriminate H81.
  - destruct (andb (Nat.leb 1 op) (Nat.leb op 4)) eqn:Epush; [|reflexivity].
    apply andb_true_iff in Epush. destruct Epush as [_ H4].
    apply Nat.leb_le in H81. apply Nat.leb_le in H4.
    lia.
Qed.

Lemma op1_to_op16_not_canonical_mint :
  forall (op : nat) (rest : list byte),
    Nat.leb 81%nat op = true ->
    Nat.leb op 96%nat = true ->
    parse_canonical_mint (op :: rest) = None.
Proof.
  intros op rest H81 H96.
  unfold parse_canonical_mint.
  rewrite (op1_to_op16_not_scriptnum_prefix op rest H81 H96).
  reflexivity.
Qed.

Lemma mint_split_at_app :
  forall n xs pre post,
    split_at n xs = Some (pre, post) ->
    xs = app_bytes pre post /\ length pre = n.
Proof.
  induction n as [|n IH]; intros xs pre post Hs.
  - simpl in Hs. inversion Hs. subst. split; reflexivity.
  - destruct xs as [|x xs']; [discriminate|].
    simpl in Hs.
    destruct (split_at n xs') as [[pre' post']|] eqn:Hs2; [|discriminate].
    inversion Hs. subst pre post.
    pose proof (IH xs' pre' post' Hs2) as [Happ Hlen].
    subst xs'. split; [reflexivity|]. simpl. rewrite Hlen. reflexivity.
Qed.

Lemma decode_prefix_is_encode :
  forall spk v rest,
    decode_scriptnum_prefix spk = Some (v, rest) ->
    exists pref, pref = encode_scriptnum v /\ spk = app_bytes pref rest.
Proof.
  intros spk v rest H.
  unfold decode_scriptnum_prefix in H.
  destruct spk as [|b r]; [discriminate|].
  destruct (Nat.eqb b RIGOR_PUSH_0) eqn:Hb0.
  - apply Nat.eqb_eq in Hb0. subst b.
    inversion H. subst v rest.
    exists [RIGOR_PUSH_0]. split; [reflexivity|]. reflexivity.
  - destruct (andb (Nat.leb 1 b) (Nat.leb b 4)) eqn:Hlen; [|discriminate].
    destruct (split_at b r) as [[payload rest']|] eqn:Hs; [|discriminate].
    destruct (list_eq_dec Nat.eq_dec (encode_scriptnum (scriptnum_to_Z payload))
                          (b :: payload)) as [Heq|]; [|discriminate].
    inversion H. subst v rest.
    exists (b :: payload). split; [symmetry; exact Heq|].
    assert (Hr : r = app_bytes payload rest').
    { clear H Heq Hlen Hb0.
      revert r payload rest' Hs.
      generalize b. intro n.
      induction n as [|n IH]; intros r payload rest' Hs.
      - simpl in Hs. inversion Hs. subst. reflexivity.
      - destruct r as [|x xs]; [discriminate|].
        simpl in Hs.
        destruct (split_at n xs) as [[pre post]|] eqn:Hs2; [|discriminate].
        inversion Hs. subst payload rest'.
        simpl. f_equal. eapply IH. exact Hs2. }
    subst r. reflexivity.
Qed.

(** Successful parse means the bytes *are* the encoder output — reusable
    uniqueness, not another golden row. *)
Theorem parse_canonical_mint_means_encoder :
  forall spk maturity pkh,
    parse_canonical_mint spk = Some (maturity, pkh) ->
    spk = encode_canonical_mint maturity pkh.
Proof.
  intros spk maturity pkh H.
  unfold parse_canonical_mint in H.
  destruct (decode_scriptnum_prefix spk) as [[m rest]|] eqn:Hd; [|discriminate].
  destruct rest as [|plen body]; [discriminate|].
  destruct (Nat.eqb plen RIGOR_PUSHBYTES_20) eqn:Hpl; [|discriminate].
  apply Nat.eqb_eq in Hpl. subst plen.
  destruct (split_at 20%nat body) as [[pkh' marker_tl]|] eqn:Hs; [|discriminate].
  destruct marker_tl as [|op tl]; [discriminate|].
  destruct tl; [|discriminate].
  destruct (Nat.eqb op RIGOR_MARKER_RES_MINT) eqn:Hop; [|discriminate].
  apply Nat.eqb_eq in Hop. subst op.
  inversion H. subst m pkh.
  pose proof (decode_prefix_is_encode spk maturity (20%nat :: body) Hd)
    as [pref [Hpref Hspk]].
  pose proof (mint_split_at_app 20%nat body pkh' [RIGOR_MARKER_RES_MINT] Hs) as [Hbody _].
  rewrite Hspk.
  unfold encode_canonical_mint.
  rewrite Hpref.
  rewrite Hbody.
  reflexivity.
Qed.

Theorem two_scripts_same_mint_parse_are_byte_equal :
  forall s1 s2 m pkh,
    parse_canonical_mint s1 = Some (m, pkh) ->
    parse_canonical_mint s2 = Some (m, pkh) ->
    s1 = s2.
Proof.
  intros s1 s2 m pkh H1 H2.
  apply parse_canonical_mint_means_encoder in H1.
  apply parse_canonical_mint_means_encoder in H2.
  subst. reflexivity.
Qed.

Theorem classify_res_from_canonical_mint :
  forall spk m pkh,
    parse_witness_program spk = None ->
    parse_canonical_mint spk = Some (m, pkh) ->
    classify spk = AssetRES.
Proof.
  intros spk m pkh Hw Hp.
  unfold classify, is_res_mint_script.
  rewrite Hw. rewrite Hp. reflexivity.
Qed.

Theorem classify_not_res_unless_witness_or_mint :
  forall spk,
    parse_witness_program spk = None ->
    parse_canonical_mint spk = None ->
    classify spk <> AssetRES.
Proof.
  intros spk Hw Hp.
  unfold classify, is_res_mint_script.
  rewrite Hw. rewrite Hp.
  destruct (is_converge_script spk); [discriminate|].
  destruct (is_op_return spk); discriminate.
Qed.

Theorem canonical_mint_5_is_not_a_witness_program :
  parse_witness_program (encode_canonical_mint 5 (repeat_byte 20 17%nat)) = None.
Proof. vm_compute. reflexivity. Qed.

Theorem classify_pushbytes_1_05_via_parser :
  classify (encode_canonical_mint 5 (repeat_byte 20 17%nat)) = AssetRES.
Proof.
  apply (classify_res_from_canonical_mint _
           5 (repeat_byte 20 17%nat)).
  - apply canonical_mint_5_is_not_a_witness_program.
  - vm_compute. reflexivity.
Qed.

Lemma converge_is_oru :
  classify canonical_converge = AssetORU.
Proof. vm_compute. reflexivity. Qed.

Lemma opret_is_none :
  classify canonical_op_return = AssetNone.
Proof. vm_compute. reflexivity. Qed.

Lemma empty_is_invalid :
  classify nil = AssetInvalid.
Proof. vm_compute. reflexivity. Qed.

Lemma p2wsh_is_invalid :
  classify canonical_p2wsh = AssetInvalid.
Proof. vm_compute. reflexivity. Qed.

Lemma res2_is_invalid :
  classify canonical_res2 = AssetInvalid.
Proof. vm_compute. reflexivity. Qed.

Lemma cvg2_is_invalid :
  classify canonical_cvg2 = AssetInvalid.
Proof. vm_compute. reflexivity. Qed.

Lemma p2pkh_is_invalid :
  classify canonical_p2pkh = AssetInvalid.
Proof. vm_compute. reflexivity. Qed.

Lemma invalid_is_not_oru_or_none :
  classify nil <> AssetORU /\ classify nil <> AssetNone /\ classify nil <> AssetRES.
Proof. vm_compute. repeat split; discriminate. Qed.

(** Same script ⇒ same asset (Property 2, classification half). *)
Theorem same_script_same_asset :
  forall s : list byte, classify s = classify s.
Proof. intros. reflexivity. Qed.

(** * P0-023 Proof 1 — INVALID cannot enter the asset domain.

    C++ [CheckTransaction]: [ASSET_INVALID] → [bad-rigor-script-type].
    [CheckTxInputs]: spend of INVALID/NONE → [bad-txns-spend-non-asset-input].
    This is the create-side gate. Spend-side is [is_spendable_own]. *)
Definition created_output_admitted (spk : list byte) (nValue : Z) : bool :=
  match classify spk with
  | AssetInvalid => false
  | AssetNone => Z.eqb nValue 0
  | AssetORU | AssetRES => true
  end.

Theorem invalid_cannot_enter_asset_domain :
  forall (spk : list byte) (v : Z),
    classify spk = AssetInvalid ->
    created_output_admitted spk v = false.
Proof.
  intros spk v H.
  unfold created_output_admitted. rewrite H. reflexivity.
Qed.

Theorem p2wsh_cannot_enter_asset_domain :
  forall v : Z, created_output_admitted canonical_p2wsh v = false.
Proof. intros. vm_compute. reflexivity. Qed.

Theorem p2pkh_cannot_enter_asset_domain :
  forall v : Z, created_output_admitted canonical_p2pkh v = false.
Proof. intros. vm_compute. reflexivity. Qed.

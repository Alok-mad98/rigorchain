(** Abstract script classification and PCORB mint-witness *shape*.

    C++ (P0-020): [IsAllowedScript] / [IsResScript] / [AssetTypeFromScriptPubKey] /
         [ClassifyRigorTxout] live in [script/rigor_classify.cpp] (single
         authority). Witness v0 is P2WPKH only (20-byte program). Native
         witness v1 RES program **is the 32-byte Ed25519 pubkey** (Option B),
         neither CVG2 nor RES2. Spend: [pk == program] then
         [Ed25519_Verify]. Unrecognized scripts are [ASSET_INVALID], not ORU
         and not RIGOR_MARKER_RETURN [ASSET_NONE].

    This Coq module is still a *shape* model: it may encode RES as a
    RES2-tagged 32-byte token and Convergence as CVG2 for historical
    lemmas. That encoding is **not** C++ wire. It is **not** a script
    interpreter, **not** P2WSH execution, and **not** a refinement of
    CheckTransaction.

    Classifier alignment with C++ [AssetTypeFromScriptPubKey]:
    - [SmwORU]: witness v0/20 or [RIGOR_MARKER_CONVERGE].
    - [SmwRES]: native v1/32 (not CVG2/RES2) or [RIGOR_MARKER_RES_MINT].
    - [SmwNone]: leading [RIGOR_MARKER_RETURN] with a push-only remainder (C++ [NULL_DATA]).
    - [SmwInvalid]: everything else, including CVG2, RES2, v0/32, P2PKH, empty,
      and [OP_CVG_CLAIM] (C++ has no such template; the W3 recognizer still
      exists in [CvgCovenant.v] as a retired design study).
      Distinct from [SmwNone]; replay pooling maps this to [AssetInvalid].
    - [is_allowed_script] rejects CVG2 (C++ [IsAllowedScript] / NONSTANDARD)
      and [OP_CVG_CLAIM].
    There is no [SmwCVG] constructor: [OP_CVG_CLAIM] is [SmwInvalid], matching C++
    (C-4 / R-9). The W3 recognizer in [CvgCovenant.v] is a retired
    design study and does not feed the classifier.
    Shape predicates ([is_allowed_script] or zero-value [RIGOR_MARKER_RETURN] on created
    scripts, spend-CVG2, optional PCORB sizes) are a conjunct of replay
    [apply_tx] ([ScriptShape.v]). Empty created scripts are [SmwInvalid] and
    fail the shape gate. Replay pooling is [asset_kind_of_script out_script]
    ([ReplayReorgCore/AssetFromScript.v] / [StateTransition.output_asset]).

    Used by negative vectors
    *)
From Coq Require Import List.
From Coq Require Import Bool.Bool.
Require Import Coq.Arith.PeanoNat.
Import ListNotations.
From Rigor Require Import RigorBytes.
Include RigorBytes.
Require Import InvalidCvg.
Require Rigor.ReplayReorgCore.CvgCovenant.

(** * Witness program container (BIP141-shaped; v1 is not BIP341) *)

Definition RIGOR_PUSH_0 : byte := 0%nat.
Definition RIGOR_PUSH_1 : byte := 81%nat.
Definition RIGOR_PUSH_2 : byte := 82%nat.
Definition RIGOR_PUSH_16 : byte := 96%nat.
Definition RIGOR_PUSHBYTES_20 : byte := 20%nat.
Definition RIGOR_PUSHBYTES_32 : byte := 32%nat.
Definition RIGOR_RESERVED_LEGACY_DUP : byte := 118%nat.
Definition RIGOR_RESERVED_LEGACY_EQUAL : byte := 135%nat.
Definition RIGOR_RESERVED_LEGACY_EQUALVERIFY : byte := 136%nat.
Definition RIGOR_RESERVED_LEGACY_HASH160 : byte := 169%nat.
Definition RIGOR_RESERVED_LEGACY_CHECKSIG : byte := 172%nat.
Definition RIGOR_MARKER_RETURN : byte := 106%nat.
Definition RIGOR_PUSHDATA1 : byte := 76%nat.
Definition RIGOR_PUSHDATA2 : byte := 77%nat.
Definition RIGOR_PUSHDATA4 : byte := 78%nat.
(** C++ [RIGOR_MARKER_CONVERGE] = 0xbd. Template: [<20-byte HASH160> RIGOR_MARKER_CONVERGE]. *)
Definition RIGOR_MARKER_CONVERGE : byte := 189%nat.

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

(** C++ [RIGOR_MARKER_RES_MINT] = 0xbb. Template:
    [<maturity_height> <20-byte HASH160(pubkey)> RIGOR_MARKER_RES_MINT].
    Locktime is RIGOR_PUSH_0 / RIGOR_PUSH_1..RIGOR_PUSH_16 or a direct 1..5-byte push (CScriptNum
    width). Not a script interpreter. *)
Definition RIGOR_MARKER_RES_MINT : byte := 187%nat.

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

Definition is_op_n_locktime (op : byte) : bool :=
  orb (Nat.eqb op RIGOR_PUSH_0)
      (andb (Nat.leb RIGOR_PUSH_1 op) (Nat.leb op RIGOR_PUSH_16)).

Definition parse_mint_locktime_rest (spk : list byte) : option (list byte) :=
  match spk with
  | op :: rest =>
      if is_op_n_locktime op then Some rest
      else if andb (Nat.leb 1%nat op) (Nat.leb op 5%nat) then
        match split_at op rest with
        | Some (_, rest') => Some rest'
        | None => None
        end
      else None
  | nil => None
  end.

Definition is_res_mint_script (spk : list byte) : bool :=
  match parse_mint_locktime_rest spk with
  | None => false
  | Some rest =>
      match rest with
      | plen :: body =>
          andb (Nat.eqb plen 20%nat)
               (match rev body with
                | op :: hash_rev =>
                    andb (Nat.eqb op RIGOR_MARKER_RES_MINT)
                         (Nat.eqb (length hash_rev) 20%nat)
                | nil => false
                end)
      | nil => false
      end
  end.

Definition parse_res_mint_pubkey_hash (spk : list byte) : option (list byte) :=
  match parse_mint_locktime_rest spk with
  | None => None
  | Some rest =>
      match rest with
      | plen :: body =>
          if Nat.eqb plen 20%nat then
            match rev body with
            | op :: hash_rev =>
                if andb (Nat.eqb op RIGOR_MARKER_RES_MINT)
                        (Nat.eqb (length hash_rev) 20%nat)
                then Some (rev hash_rev)
                else None
            | nil => None
            end
          else None
      | nil => None
      end
  end.

Lemma is_res_mint_script_iff_parse_hash :
  forall spk : list byte,
    is_res_mint_script spk = true <->
    exists h : list byte, parse_res_mint_pubkey_hash spk = Some h.
Proof.
  intros spk. unfold is_res_mint_script, parse_res_mint_pubkey_hash.
  destruct (parse_mint_locktime_rest spk) as [rest|].
  2: { split; intros H; [discriminate H | destruct H as [h Hh]; discriminate Hh]. }
  destruct rest as [|plen body].
  1: { split; intros H; [discriminate H | destruct H as [h Hh]; discriminate Hh]. }
  destruct (Nat.eqb plen 20%nat) eqn:Ep.
  - destruct (rev body) as [|op hash_rev].
    + split; intros H; [discriminate H | destruct H as [h Hh]; discriminate Hh].
    + destruct (andb (Nat.eqb op RIGOR_MARKER_RES_MINT) (Nat.eqb (length hash_rev) 20%nat))
        eqn:Ea.
      * split; intros H.
        -- exists (rev hash_rev). reflexivity.
        -- reflexivity.
      * split; intros H; [discriminate H | destruct H as [h Hh]; discriminate Hh].
  - split; intros H.
    + discriminate H.
    + destruct H as [h Hh]. discriminate Hh.
Qed.

(** Retired witness-v1 RES2 prefix. C++ rejects this; Coq native RES
    token is [canonical_native_res] (RFC8032 pk1, not 32 zeros). *)
Definition res2_tag : list byte := [82; 69; 83; 50]%nat.

Definition is_res2_script (spk : list byte) : bool :=
  match parse_witness_program spk with
  | Some (1%nat, prog) =>
      match prog with
      | a :: b :: c :: d :: _ =>
          andb (Nat.eqb (length prog) 32%nat)
               (andb (Nat.eqb a 82%nat)
                     (andb (Nat.eqb b 69%nat)
                           (andb (Nat.eqb c 83%nat) (Nat.eqb d 50%nat))))
      | _ => false
      end
  | _ => false
  end.

(** C++ [ParseConvergeScript]: OP_PUSHBYTES_20 || 20-byte hash || RIGOR_MARKER_CONVERGE. *)
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

Inductive SmwAsset :=
| SmwORU
| SmwRES
| SmwNone
| SmwInvalid.

Definition smw_asset_eqb (a b : SmwAsset) : bool :=
  match a, b with
  | SmwORU, SmwORU => true
  | SmwRES, SmwRES => true
  | SmwNone, SmwNone => true
  | SmwInvalid, SmwInvalid => true
  | _, _ => false
  end.

(** C++ [CScript::IsPushOnly] from [pc]: [GetOp] succeeds and every opcode
    is [<= RIGOR_PUSH_16]. Direct pushes 0..75, [RIGOR_PUSHDATA1/2/4] (length-prefixed),
    and [RIGOR_RESERVED_LEGACY_1NEGATE] / [RIGOR_RESERVED_INERT_50] / [RIGOR_PUSH_1]..[RIGOR_PUSH_16] are pushes. *)
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

(** C++ [NULL_DATA]: leading [RIGOR_MARKER_RETURN] and the remainder is push-only. *)
Definition is_op_return (spk : list byte) : bool :=
  match spk with
  | x :: rest => andb (Nat.eqb x RIGOR_MARKER_RETURN) (is_push_only rest)
  | nil => false
  end.

(** Coq-model classifier aligned with C++ [AssetTypeFromScriptPubKey].
    [OP_CVG_CLAIM] → [SmwInvalid]: C++ has no such type (C-4).
    Witness v0/20 → ORU; native v1/32 (not CVG2/RES2) → RES;
    [RIGOR_MARKER_RES_MINT] → RES; [RIGOR_MARKER_CONVERGE] → ORU;
    leading [RIGOR_MARKER_RETURN] with push-only remainder → [SmwNone];
    CVG2, RES2, v0/32, P2PKH, empty, [RIGOR_MARKER_RETURN] plus a non-push opcode,
    and any other unknown script → [SmwInvalid].
    [SmwInvalid] is not [SmwNone]. *)
Definition asset_from_script (spk : list byte) : SmwAsset :=
  if CvgCovenant.is_cvg_claim_script spk then SmwInvalid
  else
    match parse_witness_program spk with
    | Some (0%nat, prog) =>
        if Nat.eqb (length prog) 20%nat then SmwORU else SmwInvalid
    | Some (1%nat, prog) =>
        if andb (Nat.eqb (length prog) 32%nat)
                (andb (negb (is_cvg2_script spk)) (negb (is_res2_script spk)))
        then SmwRES
        else SmwInvalid
    | Some _ => SmwInvalid
    | None =>
        if is_res_mint_script spk then SmwRES
        else if is_converge_script spk then SmwORU
        else if is_op_return spk then SmwNone
        else SmwInvalid
    end.

(** C++ [IsAllowedScript] / [IsResScript] are thin wrappers over
    [ClassifyRigorTxout]. Deriving them from [asset_from_script] makes
    the admit-set a function of the classifier, so the two cannot drift. *)
Definition smw_asset_is_admitted (a : SmwAsset) : bool :=
  match a with
  | SmwORU | SmwRES => true
  | SmwNone | SmwInvalid => false
  end.

Definition is_allowed_script (spk : list byte) : bool :=
  smw_asset_is_admitted (asset_from_script spk).

Definition is_res_script (spk : list byte) : bool :=
  smw_asset_eqb (asset_from_script spk) SmwRES.

Lemma is_allowed_script_iff_admitted_asset :
  forall spk : list byte,
    is_allowed_script spk = true <->
    (asset_from_script spk = SmwORU \/
     asset_from_script spk = SmwRES).
Proof.
  intros spk. unfold is_allowed_script, smw_asset_is_admitted.
  destruct (asset_from_script spk); simpl; split; intros H;
    try discriminate H; try (destruct H as [H|H]; discriminate H);
    try reflexivity; intuition congruence.
Qed.

Lemma is_res_script_iff_smw_res :
  forall spk : list byte,
    is_res_script spk = true <-> asset_from_script spk = SmwRES.
Proof.
  intros spk. unfold is_res_script, smw_asset_eqb.
  destruct (asset_from_script spk); simpl; split; intros H;
    try discriminate H; reflexivity.
Qed.

Lemma is_cvg_claim_op_return :
  forall rest : list byte,
    CvgCovenant.is_cvg_claim_script (RIGOR_MARKER_RETURN :: rest) = false.
Proof.
  intros rest.
  unfold CvgCovenant.is_cvg_claim_script, CvgCovenant.parse_cvg_claim_script.
  reflexivity.
Qed.

Lemma parse_witness_op_return :
  forall rest : list byte,
    parse_witness_program (RIGOR_MARKER_RETURN :: rest) = None.
Proof.
  intros rest.
  unfold parse_witness_program.
  destruct (orb (Nat.ltb (length (RIGOR_MARKER_RETURN :: rest)) 4%nat)
                (Nat.ltb 42%nat (length (RIGOR_MARKER_RETURN :: rest)))); [reflexivity|].
  destruct rest as [|plen rest']; [reflexivity|].
  unfold witness_version_of. reflexivity.
Qed.

Lemma is_res_mint_op_return :
  forall rest : list byte,
    is_res_mint_script (RIGOR_MARKER_RETURN :: rest) = false.
Proof.
  intros rest.
  unfold is_res_mint_script, parse_mint_locktime_rest, is_op_n_locktime.
  reflexivity.
Qed.

Lemma is_converge_op_return :
  forall rest : list byte,
    is_converge_script (RIGOR_MARKER_RETURN :: rest) = false.
Proof.
  intros rest. unfold is_converge_script. reflexivity.
Qed.

Lemma is_op_return_classifies_none :
  forall spk : list byte,
    is_op_return spk = true -> asset_from_script spk = SmwNone.
Proof.
  intros [|x rest] Hop.
  - unfold is_op_return in Hop. discriminate Hop.
  - assert (x = RIGOR_MARKER_RETURN) as Hx.
    { unfold is_op_return in Hop. apply andb_true_iff in Hop as [Heq _].
      apply Nat.eqb_eq in Heq. exact Heq. }
    subst x.
    unfold asset_from_script.
    rewrite is_cvg_claim_op_return, parse_witness_op_return.
    rewrite is_res_mint_op_return, is_converge_op_return.
    rewrite Hop. reflexivity.
Qed.

Inductive SmwReject :=
| SmwBadScriptType
| SmwNotResScript
| SmwNotOruScript
| SmwAssetTagMismatch
| SmwSpendCvg2
| SmwPcorbTooFew
| SmwPcorbFieldSize.

Definition smw_reject_eqb (a b : SmwReject) : bool :=
  match a, b with
  | SmwBadScriptType, SmwBadScriptType => true
  | SmwNotResScript, SmwNotResScript => true
  | SmwNotOruScript, SmwNotOruScript => true
  | SmwAssetTagMismatch, SmwAssetTagMismatch => true
  | SmwSpendCvg2, SmwSpendCvg2 => true
  | SmwPcorbTooFew, SmwPcorbTooFew => true
  | SmwPcorbFieldSize, SmwPcorbFieldSize => true
  | _, _ => false
  end.

(** Claimed asset vs script-derived class. [None] means "form only"
    (allowed / not). Claimed [SmwInvalid] means "this script is not an
    admitted class": it succeeds iff [asset_from_script] is [SmwInvalid],
    and is checked before the allowed-script gate so the branch is reachable.
    A v0 script claimed as RES is [SmwNotResScript];
    a v1 RES script claimed as ORU is [SmwNotOruScript]. *)
Definition classify_script_reject (spk : list byte) (claimed : option SmwAsset)
  : option SmwReject :=
  match claimed with
  | Some SmwInvalid =>
      if smw_asset_eqb (asset_from_script spk) SmwInvalid
      then None else Some SmwAssetTagMismatch
  | _ =>
      if negb (is_allowed_script spk) then Some SmwBadScriptType
      else
        match claimed with
        | None => None
        | Some SmwRES =>
            if negb (is_res_script spk) then Some SmwNotResScript else None
        | Some SmwORU =>
            if is_res_script spk then Some SmwNotOruScript
            else if negb (smw_asset_eqb (asset_from_script spk) SmwORU)
                 then Some SmwAssetTagMismatch
                 else None
        | Some SmwNone =>
            if negb (smw_asset_eqb (asset_from_script spk) SmwNone)
            then Some SmwAssetTagMismatch else None
        | Some SmwInvalid => None
        end
  end.

Definition classify_spend_cvg2 (spk : list byte) : option SmwReject :=
  if is_cvg2_script spk then Some SmwSpendCvg2 else None.

(** PCORB body-mint witness: [ParseConvergeProof] requires ≥6 items of
    sizes 32 / 8 / 32 / 8 / 4 / 32. Extra trailing items (P2WSH redeem)
    are ignored at this layer. Field *semantics* (txid vs chain, σ bind)
    stay C++. *)
Definition pcorb_required_sizes : list nat := [32; 8; 32; 8; 4; 32]%nat.

Fixpoint prefix_sizes_ok (stack : list (list byte)) (sizes : list nat) : bool :=
  match sizes, stack with
  | nil, _ => true
  | _ :: _, nil => false
  | s :: ss, item :: rest =>
      andb (Nat.eqb (length item) s) (prefix_sizes_ok rest ss)
  end.

Definition parse_pcorb_shape (stack : list (list byte)) : bool :=
  prefix_sizes_ok stack pcorb_required_sizes.

Definition classify_pcorb_reject (stack : list (list byte)) : option SmwReject :=
  if Nat.ltb (length stack) 6%nat then Some SmwPcorbTooFew
  else if negb (parse_pcorb_shape stack) then Some SmwPcorbFieldSize
  else None.

(** Canonical constructors used by golden vectors. *)
Definition canonical_p2wpkh : list byte :=
  RIGOR_PUSH_0 :: RIGOR_PUSHBYTES_20 :: repeat_byte 20 0%nat.

Definition canonical_p2wsh : list byte :=
  RIGOR_PUSH_0 :: RIGOR_PUSHBYTES_32 :: repeat_byte 32 0%nat.

Definition canonical_res2_script : list byte :=
  RIGOR_PUSH_1 :: RIGOR_PUSHBYTES_32 :: app_bytes res2_tag (repeat_byte 28 0%nat).

(** Native witness v1 RES: 32-byte RFC 8032 test-vector-1 pubkey
    (C++ [TestNativeResScript] / [TestEd25519PubKey]). Classification is
    byte-shape only: untagged v1/32 is RES whether or not the program is a
    curve point. C++ [IsCanonicalOnCurvePubkey] gates key_io / policy / wallet,
 not [AssetTypeFromScriptPubKey]. *)
Definition rfc8032_pk1 : list byte :=
  [215; 90; 152; 1; 130; 177; 10; 183;
   213; 75; 254; 211; 201; 100; 7; 58;
   14; 225; 114; 243; 218; 166; 35; 37;
   175; 2; 26; 104; 247; 7; 81; 26]%nat.

Definition canonical_native_res : list byte :=
  RIGOR_PUSH_1 :: RIGOR_PUSHBYTES_32 :: rfc8032_pk1.

(** Canonical [RIGOR_MARKER_CONVERGE] lock (C++ 22-byte template). *)
Definition canonical_converge_script : list byte :=
  RIGOR_PUSHBYTES_20 :: app_bytes (repeat_byte 20 0%nat) [RIGOR_MARKER_CONVERGE].

Definition canonical_p2pkh : list byte :=
  RIGOR_RESERVED_LEGACY_DUP :: RIGOR_RESERVED_LEGACY_HASH160 :: RIGOR_PUSHBYTES_20
        :: app_bytes (repeat_byte 20 0%nat) [RIGOR_RESERVED_LEGACY_EQUALVERIFY; RIGOR_RESERVED_LEGACY_CHECKSIG].

Definition canonical_p2sh : list byte :=
  RIGOR_RESERVED_LEGACY_HASH160 :: RIGOR_PUSHBYTES_20 :: app_bytes (repeat_byte 20 0%nat) [RIGOR_RESERVED_LEGACY_EQUAL].

Definition canonical_v1_len20 : list byte :=
  RIGOR_PUSH_1 :: RIGOR_PUSHBYTES_20 :: repeat_byte 20 0%nat.

Definition canonical_v2_32 : list byte :=
  RIGOR_PUSH_2 :: RIGOR_PUSHBYTES_32 :: repeat_byte 32 0%nat.

Definition pcorb_zeros (n : nat) : list byte := repeat_byte n 0%nat.

Definition canonical_pcorb : list (list byte) :=
  [pcorb_zeros 32; pcorb_zeros 8; pcorb_zeros 32;
   pcorb_zeros 8; pcorb_zeros 4; pcorb_zeros 32].

Lemma canonical_p2wpkh_allowed_not_res :
  andb (is_allowed_script canonical_p2wpkh)
       (negb (is_res_script canonical_p2wpkh)) = true.
Proof. vm_compute. reflexivity. Qed.

Lemma canonical_native_res_is_res :
  andb (is_allowed_script canonical_native_res)
       (is_res_script canonical_native_res) = true.
Proof. vm_compute. reflexivity. Qed.

Lemma canonical_res2_not_allowed :
  is_allowed_script canonical_res2_script = false.
Proof. vm_compute. reflexivity. Qed.

Lemma canonical_cvg2_not_allowed_invalid :
  andb (negb (is_allowed_script canonical_cvg2_script))
       (andb (negb (is_res_script canonical_cvg2_script))
             (smw_asset_eqb (asset_from_script canonical_cvg2_script) SmwInvalid))
  = true.
Proof. vm_compute. reflexivity. Qed.

Lemma canonical_p2pkh_not_allowed :
  is_allowed_script canonical_p2pkh = false.
Proof. vm_compute. reflexivity. Qed.

Lemma unknown_scripts_are_invalid :
  andb (smw_asset_eqb (asset_from_script canonical_p2pkh) SmwInvalid)
       (andb (smw_asset_eqb (asset_from_script canonical_p2wsh) SmwInvalid)
             (andb (smw_asset_eqb (asset_from_script canonical_res2_script) SmwInvalid)
                   (smw_asset_eqb (asset_from_script nil) SmwInvalid)))
  = true.
Proof. vm_compute. reflexivity. Qed.

Lemma op_return_is_none_not_invalid :
  andb (smw_asset_eqb (asset_from_script [RIGOR_MARKER_RETURN]) SmwNone)
       (negb (smw_asset_eqb (asset_from_script [RIGOR_MARKER_RETURN]) SmwInvalid))
  = true.
Proof. vm_compute. reflexivity. Qed.

Lemma op_return_direct_push_is_none :
  smw_asset_eqb (asset_from_script [RIGOR_MARKER_RETURN; 1%nat; 120%nat]) SmwNone = true.
Proof. vm_compute. reflexivity. Qed.

Lemma op_return_non_push_is_invalid :
  smw_asset_eqb (asset_from_script [RIGOR_MARKER_RETURN; RIGOR_RESERVED_LEGACY_DUP]) SmwInvalid = true.
Proof. vm_compute. reflexivity. Qed.

Lemma canonical_converge_allowed_oru :
  andb (is_allowed_script canonical_converge_script)
       (smw_asset_eqb (asset_from_script canonical_converge_script) SmwORU)
  = true.
Proof. vm_compute. reflexivity. Qed.

Lemma canonical_pcorb_parses :
  parse_pcorb_shape canonical_pcorb = true.
Proof. vm_compute. reflexivity. Qed.

(** Maturity 100 as a 1-byte CScriptNum push, then 20-byte hash, then RIGOR_MARKER_RES_MINT. *)
Definition canonical_res_mint_script : list byte :=
  1%nat :: 100%nat :: RIGOR_PUSHBYTES_20
        :: app_bytes (repeat_byte 20 0%nat) [RIGOR_MARKER_RES_MINT].

Lemma canonical_res_mint_hash :
  parse_res_mint_pubkey_hash canonical_res_mint_script = Some (repeat_byte 20 0%nat).
Proof. vm_compute. reflexivity. Qed.

Lemma canonical_res_mint_allowed_res :
  andb (is_allowed_script canonical_res_mint_script)
       (andb (is_res_script canonical_res_mint_script)
             (smw_asset_eqb (asset_from_script canonical_res_mint_script) SmwRES))
  = true.
Proof. vm_compute. reflexivity. Qed.

Lemma canonical_cvg_claim_is_invalid :
  andb (negb (is_allowed_script CvgCovenant.canonical_cvg_claim_script))
       (smw_asset_eqb (asset_from_script CvgCovenant.canonical_cvg_claim_script)
                      SmwInvalid)
  = true.
Proof. vm_compute. reflexivity. Qed.

Lemma claimed_invalid_accepts_p2pkh :
  match classify_script_reject canonical_p2pkh (Some SmwInvalid) with
  | None => true
  | Some _ => false
  end = true.
Proof. vm_compute. reflexivity. Qed.

Lemma claimed_invalid_rejects_p2wpkh :
  match classify_script_reject canonical_p2wpkh (Some SmwInvalid) with
  | Some r => smw_reject_eqb r SmwAssetTagMismatch
  | None => false
  end = true.
Proof. vm_compute. reflexivity. Qed.

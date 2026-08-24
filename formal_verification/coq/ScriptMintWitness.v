(** Abstract script classification and PCORB mint-witness *shape*.

    C++: [IsAllowedScript] / [IsResScript] / [AssetTypeFromScriptPubKey] /
         [ClassifyRigorTxout] / [ParseConvergeProof]
         (core/src/consensus/tx_check.cpp, script/standard.cpp,
          primitives/transaction.cpp, consensus/res_proof.cpp).
    Python L1: [is_allowed_script] / [is_res_script] / [asset_from_script] /
               [parse_pcorb_shape] (rigor_consensus_spec.py).

    This is the *abstract form* of:
      - Witness v0 (ORU, program 20 or 32) vs Witness v1 (RES unless CVG2)
      - [IsAllowedScript] (v0 any BIP141 program, or v1 / 32)
      - [IsResScript] (v1 / 32 / not CVG2)
      - PCORB mint carrier: tagged coinbase OP_RETURN (`PCOR` ‖ 116 field bytes)
      - spend-CVG2 as a script-form reject

    It is **not** a script interpreter, **not** P2WSH execution, **not**
    [ConnectEvaluateCoinbaseResMint], and **not** a refinement of CheckTransaction.
    Shape predicates ([is_allowed_script] on *non-empty* created scripts,
    spend-CVG2, optional PCORB sizes) are a conjunct of replay [apply_tx]
    ([ScriptShape.v]). Replay pooling is [asset_kind_of_script out_script]
    ([ReplayReorgCore/AssetFromScript.v] / [StateTransition.output_asset]).

    Used by TASK-FV-VECTOR-SCRIPT-AND-MINT-WITNESS-NEG-001 negative vectors
    and TASK-FV-MODEL-SCRIPT-SHAPE-LIFT-001. *)
From Coq Require Import List.
From Coq Require Import Bool.Bool.
Require Import Coq.Arith.PeanoNat.
Import ListNotations.
From Rigor Require Import RigorBytes.
Include RigorBytes.
Require Import InvalidCvg.
Require Rigor.ReplayReorgCore.CvgCovenant.

(** * Witness program container (BIP141-shaped; v1 is not BIP341) *)

Definition OP_2 : byte := 82%nat.
Definition OP_16 : byte := 96%nat.
Definition OP_DUP : byte := 118%nat.
Definition OP_EQUAL : byte := 135%nat.
Definition OP_EQUALVERIFY : byte := 136%nat.
Definition OP_HASH160 : byte := 169%nat.
Definition OP_CHECKSIG : byte := 172%nat.
Definition OP_RETURN : byte := 106%nat.

Definition witness_version_of (b : byte) : option nat :=
  if Nat.eqb b OP_0 then Some 0%nat
  else if andb (Nat.leb OP_1 b) (Nat.leb b OP_16)
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

(** C++ [OP_RES_MINT] = 0xbb. Template:
    [<maturity_height> <20-byte HASH160(pubkey)> OP_RES_MINT].
    Locktime is OP_0 / OP_1..OP_16 or a direct 1..5-byte push (CScriptNum
    width). Not a script interpreter. *)
Definition OP_RES_MINT : byte := 187%nat.

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
  orb (Nat.eqb op OP_0)
      (andb (Nat.leb OP_1 op) (Nat.leb op OP_16)).

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
                    andb (Nat.eqb op OP_RES_MINT)
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
                if andb (Nat.eqb op OP_RES_MINT)
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
    + destruct (andb (Nat.eqb op OP_RES_MINT) (Nat.eqb (length hash_rev) 20%nat))
        eqn:Ea.
      * split; intros H.
        -- exists (rev hash_rev). reflexivity.
        -- reflexivity.
      * split; intros H; [discriminate H | destruct H as [h Hh]; discriminate Hh].
  - split; intros H.
    + discriminate H.
    + destruct H as [h Hh]. discriminate Hh.
Qed.

(** C++ [IsAllowedScript]: [OP_CVG_CLAIM] / [OP_RES_MINT] templates,
    witness v0, or v1 / 32. *)
Definition is_allowed_script (spk : list byte) : bool :=
  if CvgCovenant.is_cvg_claim_script spk then true
  else if is_res_mint_script spk then true
  else
    match parse_witness_program spk with
    | Some (0%nat, _) => true
    | Some (1%nat, prog) => Nat.eqb (length prog) 32%nat
    | _ => false
    end.

(** C++ [IsResScript]: [OP_RES_MINT], or v1 / 32 / not CVG2.
    [OP_CVG_CLAIM] is not RES. *)
Definition is_res_script (spk : list byte) : bool :=
  if CvgCovenant.is_cvg_claim_script spk then false
  else if is_res_mint_script spk then true
  else
    match parse_witness_program spk with
    | Some (1%nat, prog) =>
        andb (Nat.eqb (length prog) 32%nat) (negb (is_cvg2_script spk))
    | _ => false
    end.

Inductive SmwAsset :=
| SmwORU
| SmwRES
| SmwNone
| SmwCVG.

Definition smw_asset_eqb (a b : SmwAsset) : bool :=
  match a, b with
  | SmwORU, SmwORU => true
  | SmwRES, SmwRES => true
  | SmwNone, SmwNone => true
  | SmwCVG, SmwCVG => true
  | _, _ => false
  end.

Definition is_op_return (spk : list byte) : bool :=
  match spk with
  | x :: _ => Nat.eqb x OP_RETURN
  | _ => false
  end.

(** Coq-model classifier. C++ [AssetTypeFromScriptPubKey] has no [OP_CVG_CLAIM]
    yet: this constructor is the W3 model extension.
    [OP_CVG_CLAIM] → CVG; [OP_RES_MINT] → RES; witness v0 → ORU;
    v1/32 CVG2 → ORU; other v1/32 → RES; OP_RETURN → None;
    remaining non-witness defaults to ORU (legacy parse only,
    **not** [IsAllowedScript]). *)
Definition asset_from_script (spk : list byte) : SmwAsset :=
  if CvgCovenant.is_cvg_claim_script spk then SmwCVG
  else if is_res_mint_script spk then SmwRES
  else
    match parse_witness_program spk with
    | Some (0%nat, _) => SmwORU
    | Some (1%nat, prog) =>
        if Nat.eqb (length prog) 32%nat then
          if is_cvg2_script spk then SmwORU else SmwRES
        else SmwORU
    | _ => if is_op_return spk then SmwNone else SmwORU
    end.

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
    (allowed / not). A v0 script claimed as RES is [SmwNotResScript];
    a v1 RES script claimed as ORU is [SmwNotOruScript]. *)
Definition classify_script_reject (spk : list byte) (claimed : option SmwAsset)
  : option SmwReject :=
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
    | Some SmwCVG =>
        if negb (CvgCovenant.is_cvg_claim_script spk) then Some SmwAssetTagMismatch else None
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
  OP_0 :: OP_PUSH20 :: repeat_byte 20 0%nat.

Definition canonical_p2wsh : list byte :=
  OP_0 :: OP_PUSH32 :: repeat_byte 32 0%nat.

Definition res2_tag : list byte := [82; 69; 83; 50]%nat.

Definition canonical_res2_script : list byte :=
  OP_1 :: OP_PUSH32 :: app_bytes res2_tag (repeat_byte 28 0%nat).

Definition canonical_p2pkh : list byte :=
  OP_DUP :: OP_HASH160 :: OP_PUSH20
        :: app_bytes (repeat_byte 20 0%nat) [OP_EQUALVERIFY; OP_CHECKSIG].

Definition canonical_p2sh : list byte :=
  OP_HASH160 :: OP_PUSH20 :: app_bytes (repeat_byte 20 0%nat) [OP_EQUAL].

Definition canonical_v1_len20 : list byte :=
  OP_1 :: OP_PUSH20 :: repeat_byte 20 0%nat.

Definition canonical_v2_32 : list byte :=
  OP_2 :: OP_PUSH32 :: repeat_byte 32 0%nat.

Definition pcorb_zeros (n : nat) : list byte := repeat_byte n 0%nat.

Definition canonical_pcorb : list (list byte) :=
  [pcorb_zeros 32; pcorb_zeros 8; pcorb_zeros 32;
   pcorb_zeros 8; pcorb_zeros 4; pcorb_zeros 32].

Lemma canonical_p2wpkh_allowed_not_res :
  andb (is_allowed_script canonical_p2wpkh)
       (negb (is_res_script canonical_p2wpkh)) = true.
Proof. vm_compute. reflexivity. Qed.

Lemma canonical_res2_is_res :
  andb (is_allowed_script canonical_res2_script)
       (is_res_script canonical_res2_script) = true.
Proof. vm_compute. reflexivity. Qed.

Lemma canonical_cvg2_is_not_res :
  andb (is_allowed_script canonical_cvg2_script)
       (negb (is_res_script canonical_cvg2_script)) = true.
Proof. vm_compute. reflexivity. Qed.

Lemma canonical_p2pkh_not_allowed :
  is_allowed_script canonical_p2pkh = false.
Proof. vm_compute. reflexivity. Qed.

Lemma canonical_pcorb_parses :
  parse_pcorb_shape canonical_pcorb = true.
Proof. vm_compute. reflexivity. Qed.

(** Maturity 100 as a 1-byte CScriptNum push, then 20-byte hash, then OP_RES_MINT. *)
Definition canonical_res_mint_script : list byte :=
  1%nat :: 100%nat :: OP_PUSH20
        :: app_bytes (repeat_byte 20 0%nat) [OP_RES_MINT].

Lemma canonical_res_mint_hash :
  parse_res_mint_pubkey_hash canonical_res_mint_script = Some (repeat_byte 20 0%nat).
Proof. vm_compute. reflexivity. Qed.

Lemma canonical_res_mint_allowed_res :
  andb (is_allowed_script canonical_res_mint_script)
       (andb (is_res_script canonical_res_mint_script)
             (smw_asset_eqb (asset_from_script canonical_res_mint_script) SmwRES))
  = true.
Proof. vm_compute. reflexivity. Qed.

Lemma canonical_cvg_claim_is_cvg :
  andb (is_allowed_script CvgCovenant.canonical_cvg_claim_script)
       (andb (negb (is_res_script CvgCovenant.canonical_cvg_claim_script))
             (smw_asset_eqb (asset_from_script CvgCovenant.canonical_cvg_claim_script)
                            SmwCVG))
  = true.
Proof. vm_compute. reflexivity. Qed.

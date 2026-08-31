(** ReplayReorgCore/AssetFromScript.v
    TASK-FV-W10-ASSET-FROM-SCRIPT-001 + TASK-4 derived-asset lift.

    [AssetKind] is a pure function of script bytes. Pooling
    ([txout_asset_value] / [output_asset]) *is* that function:
    there is no independent [out_asset] tag on [TxOut].

    This matches C++ [AssetTypeFromScriptPubKey] at the *shape* layer
    (witness v0/20 P2WPKH, native v1 RES, [OP_RES_MINT], [OP_CONVERGE],
    OP_RETURN). Unrecognized scripts, including CVG2, RES2, and [OP_CVG_CLAIM],
    map to [AssetInvalid] (not ORU, not [AssetNone] / OP_RETURN — TASK-010 C-4 / R-9).
    It is **not** a script interpreter, **not** P2WSH execution, **not** [ConnectEvaluateResMintTx],
    and **not** a C++ refinement.

    [apply_tx] requires [apply_tx_derived_assets_obligation], which holds
    for every transaction because pooling is definitionally [f(script)].

    Honesty flags stay false. No [Axiom] / [Admitted]. *)
From Coq Require Import ZArith.ZArith.
From Coq Require Import List.
From Coq Require Import Arith.
From Coq Require Import Bool.
Import ListNotations.
Require Import Rigor.ReplayReorgCore.ReplayState.
Require Import Rigor.ReplayReorgCore.StateTransition.
Require ScriptMintWitness.
Require InvalidCvg.
Require Import Rigor.ReplayReorgCore.CvgCovenant.

(** * 1. Classifier (re-export of [StateTransition.asset_kind_of_script]) *)

Definition smw_to_asset_kind (a : ScriptMintWitness.SmwAsset) : AssetKind :=
  match a with
  | ScriptMintWitness.SmwORU => AssetORU
  | ScriptMintWitness.SmwRES => AssetRES
  | ScriptMintWitness.SmwNone => AssetNone
  | ScriptMintWitness.SmwInvalid => AssetInvalid
  end.

Lemma asset_kind_of_script_smw :
  forall spk : list nat,
    asset_kind_of_script spk =
    smw_to_asset_kind (ScriptMintWitness.asset_from_script spk).
Proof. intros spk. unfold asset_kind_of_script, smw_to_asset_kind. reflexivity. Qed.

(** * 2. Derivation is definitional (no tag to drift) *)

Definition txout_asset_derived (o : TxOut) : bool :=
  asset_eqb (output_asset o) (asset_kind_of_script (out_script o)).

Fixpoint utxo_assets_derived (u : UTXOSet) : bool :=
  match u with
  | nil => true
  | (_, o) :: rest => andb (txout_asset_derived o) (utxo_assets_derived rest)
  end.

Definition tx_creates_assets_derived (tx : TxAbs) : bool :=
  forallb txout_asset_derived (tx_creates tx).

Definition mk_derived_txout (v : Z) (spk : list nat) : TxOut :=
  {| out_value := v; out_script := spk |}.

(** * 3. Core lemmas *)

Lemma asset_eqb_true :
  forall a b : AssetKind, asset_eqb a b = true -> a = b.
Proof.
  destruct a, b; simpl; intros H; try reflexivity; discriminate.
Qed.

Lemma mk_derived_txout_well_formed :
  forall (v : Z) (spk : list nat),
    txout_asset_derived (mk_derived_txout v spk) = true.
Proof.
  intros v spk.
  unfold txout_asset_derived, mk_derived_txout, output_asset.
  cbn [out_script].
  destruct (asset_kind_of_script spk); reflexivity.
Qed.

Theorem txout_asset_derived_always :
  forall o : TxOut, txout_asset_derived o = true.
Proof.
  intros o. unfold txout_asset_derived, output_asset.
  destruct (asset_kind_of_script (out_script o)); reflexivity.
Qed.

Theorem valid_output_satisfies_derivation :
  forall o : TxOut,
    output_asset o = asset_kind_of_script (out_script o).
Proof. intros o. reflexivity. Qed.

Theorem well_formed_output_asset_is_script_function :
  forall o1 o2 : TxOut,
    out_script o1 = out_script o2 ->
    output_asset o1 = output_asset o2.
Proof.
  intros o1 o2 Hs.
  unfold output_asset. rewrite Hs. reflexivity.
Qed.

Theorem derived_txout_pools_as_script :
  forall (o : TxOut) (k : AssetKind),
    txout_asset_value k o =
    (if asset_eqb (asset_kind_of_script (out_script o)) k
     then out_value o else 0%Z).
Proof.
  intros o k. apply txout_asset_value_from_script.
Qed.

Theorem apply_tx_enforces_derived_assets :
  forall (s : ReplayState) (tx : TxAbs) (idx : nat) (s' : ReplayState),
    apply_tx s tx idx = Some s' ->
    apply_tx_derived_assets_obligation tx = true.
Proof. intros. apply apply_tx_derived_assets_obligation_true. Qed.

Lemma utxo_assets_derived_nil : utxo_assets_derived nil = true.
Proof. reflexivity. Qed.

Lemma utxo_assets_derived_cons :
  forall (op : OutPoint) (o : TxOut) (rest : UTXOSet),
    utxo_assets_derived ((op, o) :: rest) = true ->
    txout_asset_derived o = true /\ utxo_assets_derived rest = true.
Proof.
  intros op o rest H.
  unfold utxo_assets_derived in H; fold utxo_assets_derived in H.
  apply andb_true_iff in H. exact H.
Qed.

Lemma utxo_assets_derived_always :
  forall u : UTXOSet, utxo_assets_derived u = true.
Proof.
  induction u as [|p rest IH]; [reflexivity|].
  destruct p as [op o]. simpl.
  rewrite txout_asset_derived_always. exact IH.
Qed.

(** * 4. Classifier examples (shape-level, [vm_compute]) *)

Lemma p2wpkh_kind_oru :
  asset_kind_of_script ScriptMintWitness.canonical_p2wpkh = AssetORU.
Proof. vm_compute. reflexivity. Qed.

Lemma native_res_kind_res :
  asset_kind_of_script ScriptMintWitness.canonical_native_res = AssetRES.
Proof. vm_compute. reflexivity. Qed.

Lemma cvg2_kind_invalid :
  asset_kind_of_script InvalidCvg.canonical_cvg2_script = AssetInvalid.
Proof. vm_compute. reflexivity. Qed.

Lemma mint_kind_res :
  asset_kind_of_script ScriptMintWitness.canonical_res_mint_script = AssetRES.
Proof. vm_compute. reflexivity. Qed.

Lemma opret_kind_none :
  asset_kind_of_script [ScriptMintWitness.RIGOR_MARKER_RETURN] = AssetNone.
Proof. vm_compute. reflexivity. Qed.

Lemma converge_kind_oru :
  asset_kind_of_script ScriptMintWitness.canonical_converge_script = AssetORU.
Proof. vm_compute. reflexivity. Qed.

(** Empty script is [AssetInvalid], not ORU and not OP_RETURN [AssetNone].
    DualPool fixtures therefore use [spk_oru] / [spk_res], not [nil]. *)
Lemma empty_script_kind_invalid :
  asset_kind_of_script nil = AssetInvalid.
Proof. vm_compute. reflexivity. Qed.

Lemma p2wsh_kind_invalid :
  asset_kind_of_script ScriptMintWitness.canonical_p2wsh = AssetInvalid.
Proof. vm_compute. reflexivity. Qed.

Lemma res2_kind_invalid :
  asset_kind_of_script ScriptMintWitness.canonical_res2_script = AssetInvalid.
Proof. vm_compute. reflexivity. Qed.

Lemma p2pkh_kind_invalid :
  asset_kind_of_script ScriptMintWitness.canonical_p2pkh = AssetInvalid.
Proof. vm_compute. reflexivity. Qed.

Lemma invalid_is_not_none :
  asset_kind_of_script nil <> AssetNone.
Proof. vm_compute. discriminate. Qed.

Lemma mint_script_is_res_bytes :
  ScriptMintWitness.is_res_script ScriptMintWitness.canonical_res_mint_script = true.
Proof. vm_compute. reflexivity. Qed.

Lemma cvg_claim_kind_invalid :
  asset_kind_of_script canonical_cvg_claim_script = AssetInvalid.
Proof. vm_compute. reflexivity. Qed.

Lemma is_cvg_claim_script_kind :
  forall spk : list nat,
    is_cvg_claim_script spk = true ->
    asset_kind_of_script spk = AssetInvalid.
Proof.
  intros spk H.
  unfold asset_kind_of_script, ScriptMintWitness.asset_from_script.
  rewrite H. reflexivity.
Qed.

Lemma cvg_claim_script_is_invalid_bytes :
  ScriptMintWitness.asset_from_script canonical_cvg_claim_script =
    ScriptMintWitness.SmwInvalid.
Proof. vm_compute. reflexivity. Qed.

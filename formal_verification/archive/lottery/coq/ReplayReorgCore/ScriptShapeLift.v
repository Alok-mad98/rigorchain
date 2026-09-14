(** ReplayReorgCore/ScriptShapeLift.v
    TASK-FV-MODEL-SCRIPT-SHAPE-LIFT-001.

    Lemmas: the finite SMW *shape* gates are conjuncts of [apply_tx].
    Disallowed created scripts (P2PKH / P2SH / v2 / v1-len20), spend-CVG2,
    and ill-sized PCORB stacks make [apply_tx] return [None]. Canonical
    ORU/RES scripts plus [tx_pcorb = None] succeed on mint-shape.

    We do **not** claim P2WSH execution or PCORB field semantics.
    Pooling is [output_asset] = [f(script)]. Block-level: every accepted
    [apply_block] body passed the same conjunct
    ([StateTransition.apply_block_succeeds_implies_script_shape]).
    Not a C++ refinement. *)
From Coq Require Import ZArith.ZArith.
From Coq Require Import List.
From Coq Require Import Arith.
From Coq Require Import Bool.
Import ListNotations.
Require Import Rigor.ReplayReorgCore.ReplayState.
Require Import Rigor.ReplayReorgCore.StateTransition.
Require Import Rigor.ReplayReorgCore.ScriptShape.
Require Import Rigor.ReplayReorgCore.ReplayInvariants.
From Rigor Require Import ResSchedule.
Require ScriptMintWitness.

(** * Local fixtures *)

Definition sl_txid : Hash32 := 1 :: hash32_placeholder.
Definition sl_op : OutPoint := MkOutPoint hash32_placeholder 0.

Definition sl_oru (v : Z) : TxOut := mk_oru v.

Definition sl_res (v : Z) : TxOut := mk_res v.

Definition sl_state (u : UTXOSet) : ReplayState :=
  {| rs_utxo := u;
     rs_chain :=
       {| cm_height := 0;
          cm_chainwork := 0%Z;
          cm_tip_hash := hash32_placeholder |};
     rs_oru_supply := 0%Z;
     rs_res_supply := 0%Z;
     rs_history := [] |}.

Lemma sl_gather_singleton :
  forall o : TxOut, gather_inputs [(sl_op, o)] [sl_op] = Some [o].
Proof.
  intros o. unfold gather_inputs, utxo_lookup.
  rewrite outpoint_eqb_refl. reflexivity.
Qed.

Lemma sl_mint_reward :
  apply_tx_mint_reward (sl_state [(sl_op, sl_oru 1)]) = RES_REWARD_GENESIS.
Proof. vm_compute. reflexivity. Qed.

(** * Created-script rejects (SMW-003 / 004 / 008 / 002 families) *)

Definition sl_out_script (spk : list nat) : TxOut :=
  {| out_value := 1%Z; out_script := spk |}.

Definition sl_create_tx (spk : list nat) : TxAbs :=
  {| tx_id := sl_txid;
     tx_spends := [sl_op];
     tx_creates := [sl_out_script spk];
     tx_pcorb := None |}.

Lemma sl_create_shape_false :
  forall spk : list nat,
    created_script_shape_ok (sl_out_script spk) = false ->
    valid_script_shape_tx [(sl_op, sl_oru 1)] (sl_create_tx spk) = false.
Proof.
  intros spk Hf.
  unfold valid_script_shape_tx, sl_create_tx.
  rewrite sl_gather_singleton.
  unfold valid_script_shape_on_ins. simpl.
  rewrite Hf. reflexivity.
Qed.

Lemma sl_empty_created_false :
  created_script_shape_ok (sl_out_script nil) = false.
Proof. vm_compute. reflexivity. Qed.

Lemma sl_p2pkh_created_false :
  created_script_shape_ok (sl_out_script ScriptMintWitness.canonical_p2pkh) = false.
Proof. vm_compute. reflexivity. Qed.

Lemma sl_p2sh_created_false :
  created_script_shape_ok (sl_out_script ScriptMintWitness.canonical_p2sh) = false.
Proof. vm_compute. reflexivity. Qed.

Lemma sl_v2_created_false :
  created_script_shape_ok (sl_out_script ScriptMintWitness.canonical_v2_32) = false.
Proof. vm_compute. reflexivity. Qed.

Lemma sl_v1_len20_created_false :
  created_script_shape_ok (sl_out_script ScriptMintWitness.canonical_v1_len20) = false.
Proof. vm_compute. reflexivity. Qed.

Theorem apply_tx_rejects_created_empty_script :
  apply_tx (sl_state [(sl_op, sl_oru 1)]) (sl_create_tx nil) 0 = None.
Proof.
  apply apply_tx_rejects_without_script_shape.
  apply sl_create_shape_false. exact sl_empty_created_false.
Qed.

Theorem apply_tx_rejects_created_p2pkh :
  apply_tx (sl_state [(sl_op, sl_oru 1)])
           (sl_create_tx ScriptMintWitness.canonical_p2pkh) 0 = None.
Proof.
  apply apply_tx_rejects_without_script_shape.
  apply sl_create_shape_false. exact sl_p2pkh_created_false.
Qed.

Theorem apply_tx_rejects_created_p2sh :
  apply_tx (sl_state [(sl_op, sl_oru 1)])
           (sl_create_tx ScriptMintWitness.canonical_p2sh) 0 = None.
Proof.
  apply apply_tx_rejects_without_script_shape.
  apply sl_create_shape_false. exact sl_p2sh_created_false.
Qed.

Theorem apply_tx_rejects_created_v2 :
  apply_tx (sl_state [(sl_op, sl_oru 1)])
           (sl_create_tx ScriptMintWitness.canonical_v2_32) 0 = None.
Proof.
  apply apply_tx_rejects_without_script_shape.
  apply sl_create_shape_false. exact sl_v2_created_false.
Qed.

Theorem apply_tx_rejects_created_v1_len20 :
  apply_tx (sl_state [(sl_op, sl_oru 1)])
           (sl_create_tx ScriptMintWitness.canonical_v1_len20) 0 = None.
Proof.
  apply apply_tx_rejects_without_script_shape.
  apply sl_create_shape_false. exact sl_v1_len20_created_false.
Qed.

(** * Spend-CVG2 (SMW-009) *)

Definition sl_cvg2_out : TxOut :=
  {| out_value := 1%Z;
     out_script := InvalidCvg.canonical_cvg2_script |}.

Definition sl_spend_cvg2_tx : TxAbs :=
  {| tx_id := sl_txid;
     tx_spends := [sl_op];
     tx_creates := [sl_oru 1];
     tx_pcorb := None |}.

Lemma sl_spend_cvg2_shape_false :
  valid_script_shape_tx [(sl_op, sl_cvg2_out)] sl_spend_cvg2_tx = false.
Proof.
  unfold valid_script_shape_tx, sl_spend_cvg2_tx.
  rewrite sl_gather_singleton.
  unfold valid_script_shape_on_ins.
  rewrite (no_spend_cvg2_ins_rejects_cvg2 sl_cvg2_out nil).
  - apply Bool.andb_false_iff. right.
    apply Bool.andb_false_iff. left. reflexivity.
  - unfold sl_cvg2_out, smw_is_cvg2_script. cbn [out_script].
    exact InvalidCvg.canonical_cvg2_script_ok.
Qed.

Theorem apply_tx_rejects_spend_cvg2 :
  apply_tx (sl_state [(sl_op, sl_cvg2_out)]) sl_spend_cvg2_tx 0 = None.
Proof.
  apply apply_tx_rejects_without_script_shape.
  exact sl_spend_cvg2_shape_false.
Qed.

(** * PCORB stack shape (SMW-006 / 007 / 011 families) *)

Definition sl_mint_tx_pcorb (stack : option (list (list nat))) : TxAbs :=
  {| tx_id := sl_txid;
     tx_spends := [sl_op];
     tx_creates := [sl_res RES_REWARD_GENESIS];
     tx_pcorb := stack |}.

Definition sl_pcorb_five : list (list nat) :=
  [ScriptMintWitness.pcorb_zeros 32;
   ScriptMintWitness.pcorb_zeros 8;
   ScriptMintWitness.pcorb_zeros 32;
   ScriptMintWitness.pcorb_zeros 8;
   ScriptMintWitness.pcorb_zeros 4].

Definition sl_pcorb_bad_amount : list (list nat) :=
  [ScriptMintWitness.pcorb_zeros 32;
   ScriptMintWitness.pcorb_zeros 7;
   ScriptMintWitness.pcorb_zeros 32;
   ScriptMintWitness.pcorb_zeros 8;
   ScriptMintWitness.pcorb_zeros 4;
   ScriptMintWitness.pcorb_zeros 32].

Lemma sl_pcorb_five_parse_false :
  smw_parse_pcorb_shape sl_pcorb_five = false.
Proof. vm_compute. reflexivity. Qed.

Lemma sl_pcorb_bad_amount_parse_false :
  smw_parse_pcorb_shape sl_pcorb_bad_amount = false.
Proof. vm_compute. reflexivity. Qed.

Lemma sl_mint_pcorb_shape_false :
  forall stack : list (list nat),
    smw_parse_pcorb_shape stack = false ->
    valid_script_shape_tx [(sl_op, sl_oru 1)]
      (sl_mint_tx_pcorb (Some stack)) = false.
Proof.
  intros stack Hf.
  unfold valid_script_shape_tx, sl_mint_tx_pcorb.
  rewrite sl_gather_singleton.
  unfold valid_script_shape_on_ins, pcorb_shape_ok. cbn [tx_pcorb].
  rewrite Hf.
  apply Bool.andb_false_iff. right.
  apply Bool.andb_false_iff. right.
  reflexivity.
Qed.

Theorem apply_tx_rejects_pcorb_too_few :
  apply_tx (sl_state [(sl_op, sl_oru 1)])
           (sl_mint_tx_pcorb (Some sl_pcorb_five)) 0 = None.
Proof.
  apply apply_tx_rejects_without_script_shape.
  apply sl_mint_pcorb_shape_false. exact sl_pcorb_five_parse_false.
Qed.

Theorem apply_tx_rejects_pcorb_field_size :
  apply_tx (sl_state [(sl_op, sl_oru 1)])
           (sl_mint_tx_pcorb (Some sl_pcorb_bad_amount)) 0 = None.
Proof.
  apply apply_tx_rejects_without_script_shape.
  apply sl_mint_pcorb_shape_false. exact sl_pcorb_bad_amount_parse_false.
Qed.

(** * Fixture admission: canonical ORU/RES scripts + [None] PCORB succeed *)

Definition sl_mint_tx : TxAbs := sl_mint_tx_pcorb None.

Lemma sl_mint_dual_pool :
  valid_dual_pool_tx [(sl_op, sl_oru 1)] sl_mint_tx RES_REWARD_GENESIS = true.
Proof.
  unfold valid_dual_pool_tx, sl_mint_tx, sl_mint_tx_pcorb.
  rewrite sl_gather_singleton.
  unfold res_mint_shape, sum_asset, txout_asset_value, count_asset, sl_oru, sl_res.
  vm_compute. reflexivity.
Qed.

Lemma sl_mint_script_shape :
  valid_script_shape_tx [(sl_op, sl_oru 1)] sl_mint_tx = true.
Proof.
  unfold valid_script_shape_tx, sl_mint_tx, sl_mint_tx_pcorb.
  rewrite sl_gather_singleton.
  vm_compute. reflexivity.
Qed.

Lemma sl_mint_cvg_spend :
  valid_cvg_spend_tx [(sl_op, sl_oru 1)] sl_mint_tx = true.
Proof.
  unfold valid_cvg_spend_tx, sl_mint_tx, sl_mint_tx_pcorb.
  rewrite sl_gather_singleton.
  vm_compute. reflexivity.
Qed.

Lemma sl_mint_apply_tx_utxo :
  apply_tx_utxo [(sl_op, sl_oru 1)] sl_mint_tx 0 =
    Some (fold_create_outputs (tx_creates sl_mint_tx)
            (utxo_remove [(sl_op, sl_oru 1)] sl_op) sl_txid 0).
Proof.
  unfold apply_tx_utxo, sl_mint_tx, sl_mint_tx_pcorb. cbv zeta.
  cbn [tx_id tx_spends tx_creates forallb fold_left].
  unfold utxo_lookup. rewrite outpoint_eqb_refl.
  cbn [andb]. reflexivity.
Qed.

Theorem apply_tx_accepts_canonical_mint_scripts :
  match apply_tx (sl_state [(sl_op, sl_oru 1)]) sl_mint_tx 0 with
  | Some _ => true
  | None => false
  end = true.
Proof.
  unfold apply_tx.
  change (rs_utxo (sl_state [(sl_op, sl_oru 1)])) with [(sl_op, sl_oru 1)].
  rewrite sl_mint_apply_tx_utxo.
  rewrite sl_mint_reward.
  rewrite sl_mint_dual_pool.
  rewrite apply_tx_derived_assets_obligation_true.
  rewrite sl_mint_cvg_spend.
  rewrite sl_mint_script_shape.
  rewrite (valid_cvg_create_tx_no_create _ sl_mint_tx eq_refl).
  reflexivity.
Qed.

(** Allowed v0 still passes *shape* and pools as ORU. *)
Theorem allowed_v0_shape_ok :
  created_script_shape_ok
    {| out_value := 1%Z;
       out_script := ScriptMintWitness.canonical_p2wpkh |} = true.
Proof. vm_compute. reflexivity. Qed.

Theorem created_script_shape_ok_not_invalid :
  forall o : TxOut,
    created_script_shape_ok o = true ->
    output_asset o <> AssetInvalid.
Proof.
  intros o Hok.
  unfold created_script_shape_ok in Hok.
  apply orb_true_iff in Hok.
  destruct Hok as [Hal | Hor].
  - unfold output_asset, asset_kind_of_script.
    unfold smw_is_allowed_script, ScriptMintWitness.is_allowed_script,
           ScriptMintWitness.smw_asset_is_admitted in Hal.
    destruct (ScriptMintWitness.asset_from_script (out_script o));
      simpl in Hal; try discriminate Hal; discriminate.
  - apply andb_true_iff in Hor as [Hop _].
    unfold smw_is_op_return in Hop.
    unfold output_asset, asset_kind_of_script.
    rewrite (ScriptMintWitness.is_op_return_classifies_none (out_script o) Hop).
    discriminate.
Qed.

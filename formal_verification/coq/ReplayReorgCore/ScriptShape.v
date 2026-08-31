(** ReplayReorgCore/ScriptShape.v
    TASK-FV-MODEL-SCRIPT-SHAPE-LIFT-001.

    Finite byte-shape gates used by [apply_tx]:
      - created [out_script] must be [IsAllowedScript] or zero-value [OP_RETURN]
      - spent prevouts must not be CVG2
      - if [tx_pcorb = Some stack], the stack must parse as PCORB sizes

    This is **not** a script interpreter, **not** P2WSH execution, and
    **not** [ParseConvergeProof] field semantics. Empty [out_script] is
    [AssetInvalid] and is rejected as a created script (C++
    [bad-rigor-script-type]). DualPool / NegativeCore fixtures carry
    canonical ORU/RES scripts so pooling matches [asset_kind_of_script].

    [Require] of [ScriptMintWitness] is without [Import] so [Types.Tx.tx_id]
    does not clash with [TxAbs.tx_id]. *)
From Coq Require Import List.
From Coq Require Import Bool.Bool.
From Coq Require Import ZArith.ZArith.
Require Import Coq.Arith.PeanoNat.
Import ListNotations.
Require Import Rigor.ReplayReorgCore.ReplayState.
Require ScriptMintWitness.

(** * Aliases (qualified; no [Import ScriptMintWitness]) *)

Definition smw_is_allowed_script : list nat -> bool :=
  ScriptMintWitness.is_allowed_script.

Definition smw_is_op_return : list nat -> bool :=
  ScriptMintWitness.is_op_return.

Definition smw_parse_pcorb_shape : list (list nat) -> bool :=
  ScriptMintWitness.parse_pcorb_shape.

Definition smw_is_cvg2_script : list nat -> bool :=
  InvalidCvg.is_cvg2_script.

Lemma smw_is_cvg2_script_nil : smw_is_cvg2_script nil = false.
Proof. reflexivity. Qed.

(** Created scripts must be [IsAllowedScript] or zero-value [OP_RETURN].
    Empty scripts classify as [AssetInvalid] and are rejected. Pooling
    class is [StateTransition.output_asset] = [f(script)]. *)
Definition created_script_shape_ok (o : TxOut) : bool :=
  orb (smw_is_allowed_script (out_script o))
      (andb (smw_is_op_return (out_script o)) (Z.eqb (out_value o) 0)).

Definition pcorb_shape_ok (tx : TxAbs) : bool :=
  match tx_pcorb tx with
  | None => true
  | Some stack => smw_parse_pcorb_shape stack
  end.

Definition no_spend_cvg2_ins (ins : list TxOut) : bool :=
  forallb (fun o => negb (smw_is_cvg2_script (out_script o))) ins.

Definition valid_script_shape_on_ins (ins : list TxOut) (tx : TxAbs) : bool :=
  andb (forallb created_script_shape_ok (tx_creates tx))
       (andb (no_spend_cvg2_ins ins) (pcorb_shape_ok tx)).

Lemma created_script_shape_ok_nil :
  forall v : Z,
    created_script_shape_ok
      {| out_value := v; out_script := nil |} = false.
Proof. intros. vm_compute. reflexivity. Qed.

Lemma created_script_shape_ok_nil_script :
  forall o : TxOut,
    out_script o = nil -> created_script_shape_ok o = false.
Proof.
  intros o H. destruct o as [v s]. simpl in H. subst s.
  apply created_script_shape_ok_nil.
Qed.

Lemma pcorb_shape_ok_none :
  forall tx : TxAbs, tx_pcorb tx = None -> pcorb_shape_ok tx = true.
Proof.
  intros tx H. unfold pcorb_shape_ok. rewrite H. reflexivity.
Qed.

Lemma forallb_nil_creates_ok :
  forallb created_script_shape_ok nil = true.
Proof. reflexivity. Qed.

Lemma no_spend_cvg2_ins_nil_scripts :
  forall ins : list TxOut,
    forallb (fun o => match out_script o with nil => true | _ => false end)
            ins = true ->
    no_spend_cvg2_ins ins = true.
Proof.
  induction ins as [|o rest IH]; intros H; simpl in *; [reflexivity|].
  apply andb_true_iff in H as [Ho Hrest].
  unfold no_spend_cvg2_ins. simpl.
  apply andb_true_iff. split.
  - destruct (out_script o); [|discriminate Ho].
    rewrite smw_is_cvg2_script_nil. reflexivity.
  - apply IH. exact Hrest.
Qed.

Lemma valid_script_shape_on_ins_placeholder :
  forall (ins : list TxOut) (tx : TxAbs),
    tx_pcorb tx = None ->
    tx_creates tx = nil ->
    forallb (fun o => match out_script o with nil => true | _ => false end)
            ins = true ->
    valid_script_shape_on_ins ins tx = true.
Proof.
  intros ins tx Hp Hc Hi.
  unfold valid_script_shape_on_ins. rewrite Hc. simpl.
  apply andb_true_iff. split.
  - apply no_spend_cvg2_ins_nil_scripts. exact Hi.
  - apply pcorb_shape_ok_none. exact Hp.
Qed.

Lemma created_script_shape_ok_disallowed :
  forall (v : Z) (spk : list nat),
    smw_is_allowed_script spk = false ->
    smw_is_op_return spk = false ->
    created_script_shape_ok
      {| out_value := v; out_script := spk |} = false.
Proof.
  intros v spk Hal Hor.
  unfold created_script_shape_ok; cbn [out_script out_value].
  rewrite Hal, Hor. reflexivity.
Qed.

Lemma no_spend_cvg2_ins_rejects_cvg2 :
  forall (o : TxOut) (rest : list TxOut),
    smw_is_cvg2_script (out_script o) = true ->
    no_spend_cvg2_ins (o :: rest) = false.
Proof.
  intros o rest H.
  unfold no_spend_cvg2_ins. simpl. rewrite H. reflexivity.
Qed.

Lemma pcorb_shape_ok_some_rejects :
  forall (tx : TxAbs) (stack : list (list nat)),
    tx_pcorb tx = Some stack ->
    smw_parse_pcorb_shape stack = false ->
    pcorb_shape_ok tx = false.
Proof.
  intros tx stack Hp Hf.
  unfold pcorb_shape_ok. rewrite Hp. exact Hf.
Qed.

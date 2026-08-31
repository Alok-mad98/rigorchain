(** Abstract CVG2 container and Convergence-tx shape.

    C++: [MatchCvg2WitnessScript] / [IsConvergenceTx] / [MIN_CONVERGENCE_AMOUNT]
         (core/src/consensus/convergenceres_verify.cpp, rigor_consensus_lock.h).
    Python L1: [is_cvg2_script] / [classify_cvg_tx_reject] (rigor_consensus_spec.py).

    This is the *abstract form* of a Convergence output — Witness v1, 32-byte
    program prefixed ``CVG2``, value > 0, asset ORU, amount >= 1 ORU, exactly
    one such output. It is **not** a script interpreter, **not** [IsResScript],
    and **not** a refinement of ConnectBlock. Dual-pool pooling is
    [asset_kind_of_script] of [out_script] (CVG2 scripts classify as
    [AssetInvalid], not ORU).
    Spend-CVG2 *shape* is a conjunct of [apply_tx] ([ScriptShape.v]);
    this module's container predicate is what that conjunct consults.

    Used by TASK-FV-VECTOR-INVALID-CONVERGENCE-ROOT-001 negative vectors. *)
From Coq Require Import List.
From Coq Require Import ZArith.ZArith.
From Coq Require Import Bool.Bool.
Require Import Coq.Arith.PeanoNat.
Import ListNotations.
From Rigor Require Import RigorBytes.
Include RigorBytes.
Require Import Rigor.legacy_account_model.Types.

Open Scope Z_scope.

(** Same value as [Selection.MIN_CONVERGENCE_AMOUNT] / C++ [COIN]. Distinct
    identifier so GoldenHarness can import both this module and [Selection]. *)
Definition icvg_min_convergence_amount : Z := 100000000.

Definition OP_0 : byte := 0%nat.
Definition OP_1 : byte := 81%nat.
Definition OP_PUSH20 : byte := 20%nat.
Definition OP_PUSH32 : byte := 32%nat.

(** ASCII ``CVG2``. *)
Definition cvg2_tag : list byte := [67; 86; 71; 50]%nat.

Definition is_cvg2_witness_program (prog : list byte) : bool :=
  andb (Nat.eqb (length prog) 32%nat)
       (bytes_eqb (firstn 4 prog) cvg2_tag).

(** Witness v1 container: ``OP_1 <32-byte CVG2 program>``. *)
Definition is_cvg2_script (spk : list byte) : bool :=
  match spk with
  | ver :: plen :: rest =>
      andb (Nat.eqb ver OP_1)
           (andb (Nat.eqb plen OP_PUSH32)
                 (is_cvg2_witness_program rest))
  | _ => false
  end.

Definition canonical_cvg2_program : list byte :=
  app_bytes cvg2_tag (repeat_byte 28 0%nat).

Definition canonical_cvg2_script : list byte :=
  OP_1 :: OP_PUSH32 :: canonical_cvg2_program.

Lemma canonical_cvg2_script_ok :
  is_cvg2_script canonical_cvg2_script = true.
Proof. vm_compute. reflexivity. Qed.

Record CvgOutAbs := {
  cvg_value : Z;
  cvg_asset : Asset;
  cvg_script : list byte
}.

Definition cvg_out_is_cvg2_positive (o : CvgOutAbs) : bool :=
  andb (Z.gtb o.(cvg_value) 0) (is_cvg2_script o.(cvg_script)).

Fixpoint count_cvg2_positive (outs : list CvgOutAbs) : nat :=
  match outs with
  | nil => 0%nat
  | o :: rest =>
      if cvg_out_is_cvg2_positive o
      then S (count_cvg2_positive rest)
      else count_cvg2_positive rest
  end.

Definition is_convergence_tx_shape (outs : list CvgOutAbs) : bool :=
  Nat.eqb (count_cvg2_positive outs) 1%nat.

Definition cvg_asset_oru (o : CvgOutAbs) : bool :=
  match o.(cvg_asset) with
  | ORU => true
  | RES => false
  end.

Definition cvg_meets_min (o : CvgOutAbs) : bool :=
  Z.geb o.(cvg_value) icvg_min_convergence_amount.

Fixpoint find_cvg2_positive (outs : list CvgOutAbs) : option CvgOutAbs :=
  match outs with
  | nil => None
  | o :: rest =>
      if cvg_out_is_cvg2_positive o then Some o else find_cvg2_positive rest
  end.

Inductive CvgTxReject :=
| CvgTxNotScript
| CvgTxAmountZero
| CvgTxCount
| CvgTxWrongAsset
| CvgTxAmountBelowMin.

Definition cvg_tx_reject_eqb (a b : CvgTxReject) : bool :=
  match a, b with
  | CvgTxNotScript, CvgTxNotScript => true
  | CvgTxAmountZero, CvgTxAmountZero => true
  | CvgTxCount, CvgTxCount => true
  | CvgTxWrongAsset, CvgTxWrongAsset => true
  | CvgTxAmountBelowMin, CvgTxAmountBelowMin => true
  | _, _ => false
  end.

Definition classify_cvg_tx_reject (outs : list CvgOutAbs) : option CvgTxReject :=
  if andb (negb (Nat.eqb (length outs) 0%nat))
          (forallb (fun o => negb (is_cvg2_script o.(cvg_script))) outs)
  then Some CvgTxNotScript
  else
    let n := count_cvg2_positive outs in
    if negb (Nat.eqb n 1%nat) then
      if andb (Nat.eqb n 0%nat)
              (forallb (fun o => Z.eqb o.(cvg_value) 0) outs)
      then Some CvgTxAmountZero
      else Some CvgTxCount
    else
      match find_cvg2_positive outs with
      | None => Some CvgTxCount
      | Some o =>
          if negb (cvg_asset_oru o) then Some CvgTxWrongAsset
          else if negb (cvg_meets_min o) then Some CvgTxAmountBelowMin
          else None
      end.

Definition valid_cvg_candidate (outs : list CvgOutAbs) : bool :=
  match classify_cvg_tx_reject outs with
  | None => true
  | Some _ => false
  end.

Lemma canonical_single_min_is_candidate :
  valid_cvg_candidate
    [{| cvg_value := icvg_min_convergence_amount;
        cvg_asset := ORU;
        cvg_script := canonical_cvg2_script |}] = true.
Proof. vm_compute. reflexivity. Qed.

(** RIGOR v0.1 — Predicate registry (P0-027).

    Live IDs match C++ [RigorPredicateID]. Lookup is [predicate_of_spk].
    Spend is ScriptPubKey → PredicateID → verifier. Unknown is not spendable.

    Model of the registry, not a C++ refinement.
    No [Axiom] / [Admitted]. *)
From Coq Require Import List.
From Coq Require Import Bool.Bool.
From Coq Require Import Arith.PeanoNat.
Import ListNotations.
Require Import RigorV01.AssetClassification.
Require Import RigorV01.RigorUpgradeNamespace.

Inductive RigorPredicateID :=
| ORU_V0_KEYHASH
| RES_V1_NATIVE_KEY
| RES_MINT_V0
| CONVERGE_V0
| OP_RETURN_BURN.

Definition live_rigor_predicate_count : nat := 5.

Definition of_upgrade_id (p : PredicateId) : option RigorPredicateID :=
  match p with
  | PredOruP2WPKH => Some ORU_V0_KEYHASH
  | PredResV1 => Some RES_V1_NATIVE_KEY
  | PredResMint => Some RES_MINT_V0
  | PredConvergeBurn => Some CONVERGE_V0
  | PredNullData => Some OP_RETURN_BURN
  | PredFuture _ => None
  end.

Definition lookup_predicate (spk : list byte) : option RigorPredicateID :=
  match predicate_of_spk spk with
  | Some p => of_upgrade_id p
  | None => None
  end.

Definition spendable_id (id : RigorPredicateID) : bool :=
  match id with
  | ORU_V0_KEYHASH | RES_V1_NATIVE_KEY | RES_MINT_V0 => true
  | CONVERGE_V0 | OP_RETURN_BURN => false
  end.

Theorem live_count_is_five :
  live_rigor_predicate_count = 5%nat.
Proof. reflexivity. Qed.

(** Theorem 2: every classified spendable/burn template has a predicate id. *)
Theorem classified_spk_has_predicate :
  forall spk : list byte,
    classify spk <> AssetInvalid ->
    exists id : RigorPredicateID, lookup_predicate spk = Some id.
Proof.
  intros spk H.
  unfold lookup_predicate, of_upgrade_id, predicate_of_spk, classify in *.
  destruct (parse_witness_program spk) as [[v prog]|].
  - destruct v as [|v'].
    + destruct (Nat.eqb (length prog) 20%nat) eqn:E.
      * exists ORU_V0_KEYHASH. reflexivity.
      * contradiction.
    + destruct v' as [|v''].
      * destruct (is_native_res_program prog) eqn:N.
        -- exists RES_V1_NATIVE_KEY. reflexivity.
        -- contradiction.
      * contradiction.
  - destruct (is_res_mint_script spk) eqn:M.
    + exists RES_MINT_V0. reflexivity.
    + destruct (is_converge_script spk) eqn:C.
      * exists CONVERGE_V0. reflexivity.
      * destruct (is_op_return spk) eqn:R.
        -- exists OP_RETURN_BURN. reflexivity.
        -- contradiction.
Qed.

Lemma predicate_of_spk_not_future :
  forall spk n, predicate_of_spk spk <> Some (PredFuture n).
Proof. apply predicate_of_spk_never_future. Qed.

Theorem unknown_has_no_predicate :
  forall spk : list byte,
    lookup_predicate spk = None ->
    classify spk = AssetInvalid.
Proof.
  intros spk H.
  unfold lookup_predicate, of_upgrade_id in H.
  destruct (predicate_of_spk spk) as [p|] eqn:E.
  - destruct p as [| | | | | slot]; try discriminate.
    exfalso. exact (predicate_of_spk_not_future spk slot E).
  - apply unknown_template_not_in_registry; exact E.
Qed.

Theorem canonicals_lookup :
  lookup_predicate canonical_p2wpkh = Some ORU_V0_KEYHASH /\
  lookup_predicate canonical_native_res = Some RES_V1_NATIVE_KEY /\
  lookup_predicate canonical_res_mint = Some RES_MINT_V0 /\
  lookup_predicate canonical_converge = Some CONVERGE_V0 /\
  lookup_predicate canonical_op_return = Some OP_RETURN_BURN.
Proof. vm_compute; repeat split; reflexivity. Qed.

Theorem checksig_is_not_a_predicate :
  lookup_predicate [reserved_legacy_checksig_byte] = None.
Proof. vm_compute. reflexivity. Qed.

Theorem p2pkh_is_not_a_predicate :
  lookup_predicate canonical_p2pkh = None.
Proof. vm_compute. reflexivity. Qed.

(** RIGOR v0.1 — Wallet ownership (P0-027).

    Ownership is classifier + HaveKey(program). It is not OP_CHECKSIG,
    P2PKH, MULTISIG, or P2SH recursion.

    Model of the wallet rule, not a C++ refinement.
    No [Axiom] / [Admitted]. *)
From Coq Require Import Bool.Bool.
From Coq Require Import List.
Import ListNotations.
Require Import RigorV01.AssetClassification.
Require Import RigorV01.RigorUpgradeNamespace.
Require Import RigorV01.RigorPredicateRegistry.

(** The earlier form of this file defined [ownership_via_opcode := false] and
    proved [false = false]. Nothing a future change could do would make that
    fail, so it recorded an intention rather than a fact. The statements below
    are about [lookup_predicate], so changing what the wallet classifies breaks
    them. *)

Definition wallet_can_own (spk : list byte) : bool :=
  match lookup_predicate spk with
  | Some id => spendable_id id
  | None => false
  end.

(** Ownership begins at classification: a script the wallet may hold keys for
    is one the registry names and marks spendable. There is no path in which
    an unclassified script reaches a key check. *)
Theorem ownable_scripts_are_classified :
  forall spk : list byte,
    wallet_can_own spk = true ->
    exists id : RigorPredicateID,
      lookup_predicate spk = Some id /\ spendable_id id = true.
Proof.
  intros spk H. unfold wallet_can_own in H.
  destruct (lookup_predicate spk) as [id|] eqn:E; [|discriminate].
  exists id. split; [reflexivity | exact H].
Qed.

Theorem burn_outputs_are_never_ownable :
  forall spk : list byte,
    lookup_predicate spk = Some CONVERGE_V0 \/
    lookup_predicate spk = Some OP_RETURN_BURN ->
    wallet_can_own spk = false.
Proof.
  intros spk [H|H]; unfold wallet_can_own; rewrite H; reflexivity.
Qed.

Theorem bitcoin_templates_are_never_ownable :
  wallet_can_own [reserved_legacy_checksig_byte] = false /\
  wallet_can_own canonical_p2pkh = false.
Proof.
  split; unfold wallet_can_own.
  - rewrite checksig_is_not_a_predicate. reflexivity.
  - rewrite p2pkh_is_not_a_predicate. reflexivity.
Qed.

Theorem checksig_output_is_not_owned_by_registry :
  lookup_predicate [reserved_legacy_checksig_byte] = None.
Proof. apply checksig_is_not_a_predicate. Qed.

Theorem p2pkh_output_is_not_owned_by_registry :
  lookup_predicate canonical_p2pkh = None.
Proof. apply p2pkh_is_not_a_predicate. Qed.

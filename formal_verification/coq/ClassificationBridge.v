(** ClassificationBridge.v — P052 classifier mapping.

    Unique Coq authority: [AssetClassification.classify].
    Unique C++ matcher: [ClassifyRigorTxout]
      (façades: [AssetTypeFromScriptPubKey], [ClassifyRigorAsset]).

    [ClassifyRigorSpendPredicate] does not exist.

    Does not prove C++ = Coq. No [Axiom] / [Admitted] / [Parameter]. *)
From Coq Require Import List.
Import ListNotations.
Require Import RigorV01.AssetClassification.

Definition refinement_proved : bool := false.
Definition equivalence_proved : bool := false.

Theorem classify_bridge_refinement_not_proved : refinement_proved = false.
Proof. reflexivity. Qed.

Theorem classify_bridge_equivalence_not_proved : equivalence_proved = false.
Proof. reflexivity. Qed.

Definition cpp_classify_bridge := classify.

Theorem cpp_classify_bridge_is_canonical :
  forall spk, cpp_classify_bridge spk = classify spk.
Proof. intros. reflexivity. Qed.

Theorem unique_classifier_authority :
  forall spk, asset_type_from_scriptpubkey spk = classify spk.
Proof. exact unique_classifier_is_classify. Qed.

(** Production C++ has no second matcher under this name. *)
Definition ClassifyRigorSpendPredicate_exists : bool := false.

Theorem no_classify_spend_predicate :
  ClassifyRigorSpendPredicate_exists = false.
Proof. reflexivity. Qed.

Theorem empty_script_fail_closed :
  classify nil = AssetInvalid.
Proof. reflexivity. Qed.

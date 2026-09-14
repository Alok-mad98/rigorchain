(** Phase4FConsensus/FrozenCppAgreement.v
    Frozen C++ vs Coq/runtime observation comparison (PHASE-4F-001K).

    Compares each frozen C++ observation against the closest available reference
    payload (a Coq-trace or runtime observation) and tallies agree / disagree /
    missing. AUDIT ONLY: this MAY compare, but MUST NOT claim equivalence,
    replay/consensus/runtime correctness, Coq = C++, or Coq = Frozen C++. No
    consensus / runtime / RFC / SHA256 / ConvergeRoot / ConnectBlock / mining /
    wallet / RPC code is touched. *)

From Coq Require Import List ZArith.
Import ListNotations.
Require Import Rigor.Phase4FConsensus.FrozenCppOracle.

(** REQUIRED-1: result of a single frozen-vs-reference comparison. *)
Inductive FrozenAgreementResult :=
| FAR_Agree
| FAR_Disagree
| FAR_Missing.

(** Reference payload for a frozen observation. [fr_source]: 0 = none,
    1 = Coq trace, 2 = runtime. *)
Record FrozenReference := {
  fr_present : bool;
  fr_payload : Z;
  fr_source : nat;
}.

(** REQUIRED-1: compare a frozen observation to its reference.
    Missing if either side is absent; else equal payload => Agree, else Disagree.
    Agreement here is payload equality of two IMPORTED values — NOT equivalence. *)
Definition compare_frozen_observation
  (o : FrozenCppObservation) (ref : FrozenReference) : FrozenAgreementResult :=
  if negb o.(fco_present) then FAR_Missing
  else if negb ref.(fr_present) then FAR_Missing
  else if Z.eqb o.(fco_payload) ref.(fr_payload) then FAR_Agree else FAR_Disagree.

Definition far_is_agree (a : FrozenAgreementResult) : bool :=
  match a with FAR_Agree => true | _ => false end.
Definition far_is_disagree (a : FrozenAgreementResult) : bool :=
  match a with FAR_Disagree => true | _ => false end.
Definition far_is_missing (a : FrozenAgreementResult) : bool :=
  match a with FAR_Missing => true | _ => false end.

Definition frozen_results
  (ps : list (FrozenCppObservation * FrozenReference)) : list FrozenAgreementResult :=
  map (fun p => compare_frozen_observation (fst p) (snd p)) ps.

(** REQUIRED-1: counts over all imported (observation, reference) pairs. *)
Definition agreement_count
  (ps : list (FrozenCppObservation * FrozenReference)) : nat :=
  length (filter far_is_agree (frozen_results ps)).

Definition disagreement_count
  (ps : list (FrozenCppObservation * FrozenReference)) : nat :=
  length (filter far_is_disagree (frozen_results ps)).

Definition missing_count
  (ps : list (FrozenCppObservation * FrozenReference)) : nat :=
  length (filter far_is_missing (frozen_results ps)).

(** A disagreement is never an agreement (shape guard). *)
Lemma disagree_not_agree : FAR_Disagree <> FAR_Agree.
Proof. discriminate. Qed.

(** Phase4FConsensus/DrawRepresentationBridge.v
    Draw winner representation bridge (PHASE-4F-001I).

    Closes the index-vs-txid representation gap surfaced in 001H: maps a draw
    winner INDEX to the winning candidate's TXID, using the candidate txids
    already embedded in the registry (NO JSON re-read, NO kernel/runtime/RFC
    edits). This lets the Coq-recomputed winner and the imported-evidence winner
    be compared in the SAME (txid) domain. AUDIT / mapping only. *)

From Coq Require Import List ZArith.
Import ListNotations.
Require Import Rigor.Phase4FConsensus.ConsensusGoldenTrace.

(** Sentinel txid for "no winner" (index < 0). *)
Definition no_winner_txid : Z := 0.

Definition nth_txid (cands : list DrawCandidate) (idx : Z) : Z :=
  match nth_error cands (Z.to_nat idx) with
  | Some c => dc_txid c
  | None => no_winner_txid
  end.

(** REQUIRED-1: winner index -> winner txid (canonical projection). *)
Definition winner_index_to_txid (cands : list DrawCandidate) (idx : Z) : Z :=
  if Z.ltb idx 0 then no_winner_txid else nth_txid cands idx.

(** REQUIRED-1: the runtime-shaped winner txid projection. Same canonical map,
    applied to the imported (kernel/golden) winner index — i.e. the txid a node
    would report for the winning candidate. *)
Definition winner_txid_projection (cands : list DrawCandidate) (idx : Z) : Z :=
  winner_index_to_txid cands idx.

(** REQUIRED-1: agreement of the two txid projections. *)
Inductive DrawRepresentationAgreement :=
  | DRA_Match
  | DRA_Mismatch.

Definition compare_representation
  (cands : list DrawCandidate) (coq_idx imported_idx : Z)
  : DrawRepresentationAgreement :=
  if Z.eqb (winner_index_to_txid cands coq_idx)
           (winner_txid_projection cands imported_idx)
  then DRA_Match else DRA_Mismatch.

(** Equal indices project to equal txids (functionality of the projection). *)
Lemma equal_index_equal_txid :
  forall cands i j,
    i = j ->
    winner_index_to_txid cands i = winner_txid_projection cands j.
Proof. intros cands i j H. subst. reflexivity. Qed.

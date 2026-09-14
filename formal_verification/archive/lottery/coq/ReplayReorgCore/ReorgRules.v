(** ReplayReorgCore/ReorgRules.v — RFC-bound fork choice and rollback. *)
From Coq Require Import ZArith.ZArith.
From Coq Require Import List.
From Coq Require Import Arith.
From Coq Require Import Bool.
Import ListNotations.
Require Import Rigor.ReplayReorgCore.ReplayState.
Require Import Rigor.ReplayReorgCore.StateTransition.
Require Import Rigor.ReplayReorgCore.Hash32Order.

(** Candidate chain tip for fork resolution (competing branches). *)
Record ChainCandidate := {
  cc_meta : ChainMeta;
  cc_state : ReplayState
}.

(** RFC-bound fork choice: greater chainwork wins; equal work → deterministic hash tie-break.
    (See RIGOR_CONSENSUS_INVARIANTS INV-01 / REPLAY_AND_DETERMINISM §9.5.) *)
Definition better_chain (c1 c2 : ChainCandidate) : bool :=
  let w1 := cm_chainwork (cc_meta c1) in
  let w2 := cm_chainwork (cc_meta c2) in
  if Z.eqb w1 w2 then
    hash32_lt (cm_tip_hash (cc_meta c1)) (cm_tip_hash (cc_meta c2))
  else
    Z.gtb w1 w2.

(** Pick the better of two candidates. *)
Definition pick_better_pair (c1 c2 : ChainCandidate) : ChainCandidate :=
  if better_chain c1 c2 then c1 else c2.

Definition select_best_fork (candidates : list ChainCandidate)
  : option ChainCandidate :=
  match candidates with
  | nil => None
  | c :: rest =>
      Some (fold_left pick_better_pair rest c)
  end.

(** Roll back [k] blocks using pre-extension snapshots in [rs_history].
    [k = 0] is identity; [k > 0] restores the [k]-th newest snapshot. *)
Fixpoint rollback_from_history (hist : list ReplaySnapshot) (k : nat)
  : option ReplaySnapshot :=
  match k with
  | O => None  (** handled at top level *)
  | S O =>
      match hist with
      | snap :: _ => Some snap
      | [] => None
      end
  | S (S k') =>
      match hist with
      | _ :: t => rollback_from_history t (S k')
      | [] => None
      end
  end.

Lemma rollback_from_history_O_false :
  forall (hist : list ReplaySnapshot) (snap : ReplaySnapshot),
    rollback_from_history hist O = Some snap -> False.
Proof.
  intros hist snap H.
  induction hist as [|snap' hist IH]; cbn in H; discriminate H.
Qed.

Definition rollback (s : ReplayState) (k : nat) : option ReplayState :=
  match k with
  | O => Some s
  | S k' =>
      match rollback_from_history (rs_history s) k with
      | None => None
      | Some snap => Some (state_of_snapshot snap (skipn k (rs_history s)))
      end
  end.

(** Reorg: switch to candidate branch after rolling back common depth. *)
Definition reorg_to_candidate (s : ReplayState) (k : nat) (c : ChainCandidate)
  : option ReplayState :=
  match rollback s k with
  | None => None
  | Some _ =>
      Some (cc_state c)
  end.

(** Fork resolution is a pure function of the candidate list. *)
Lemma select_best_fork_deterministic :
  forall (candidates : list ChainCandidate) (r1 r2 : ChainCandidate),
    select_best_fork candidates = Some r1 ->
    select_best_fork candidates = Some r2 ->
    r1 = r2.
Proof.
  intros candidates r1 r2 H1 H2.
  congruence.
Qed.

Lemma rollback_deterministic :
  forall (s : ReplayState) (k : nat) (r1 r2 : ReplayState),
    rollback s k = Some r1 ->
    rollback s k = Some r2 ->
    r1 = r2.
Proof.
  intros s k r1 r2 H1 H2.
  congruence.
Qed.

(** Monotonic chainwork along a valid extension (structural INV-01). *)
Lemma valid_chainwork_extend_ok :
  forall (cm : ChainMeta) (hdr : BlockHeaderAbs),
    valid_chainwork_extend cm hdr = true ->
    (cm_chainwork cm < hdr_chainwork hdr)%Z.
Proof.
  intros cm hdr H.
  unfold valid_chainwork_extend in H.
  apply Z.gtb_lt in H.
  exact H.
Qed.

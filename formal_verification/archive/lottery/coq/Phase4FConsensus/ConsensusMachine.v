(** Phase4FConsensus/ConsensusMachine.v
    End-to-end consensus machine template (PHASE-4F skeleton).

    SCAFFOLDING: abstract state and transitions only. No hash kernel, block
    connect, or live-runtime imports. Refined in later TASKs. *)

From Coq Require Import List ZArith.
Import ListNotations.

(** Consensus state slot (placeholder fields; not wired to C++ chainstate yet). *)
Record ConsensusState := {
  cs_height : Z;
  cs_chain_tip_tag : nat;
  cs_supply_oru : Z;
  cs_supply_res : Z;
}.

Definition init_consensus_state : ConsensusState :=
  {| cs_height := 0%Z;
     cs_chain_tip_tag := 0;
     cs_supply_oru := 0%Z;
     cs_supply_res := 0%Z;
  |}.

(** Transition labels (scaffolding; block content abstracted to [nat] tags). *)
Inductive ConsensusTransition :=
  | TrAcceptBlock : nat -> ConsensusTransition
  | TrRollback : nat -> ConsensusTransition
  | TrNoOp : ConsensusTransition.

(** Single-step transition executor (total on [Some]; [None] = rejection). *)
Definition execute_transition
  (s : ConsensusState) (t : ConsensusTransition) : option ConsensusState :=
  match t with
  | TrNoOp => Some s
  | TrAcceptBlock bid =>
      Some {| cs_height := s.(cs_height) + 1;
              cs_chain_tip_tag := bid;
              cs_supply_oru := s.(cs_supply_oru);
              cs_supply_res := s.(cs_supply_res);
           |}
  | TrRollback k =>
      let h := s.(cs_height) in
      let kz := Z.of_nat k in
      if Z.ltb kz h then
        Some {| cs_height := h - kz;
                cs_chain_tip_tag := s.(cs_chain_tip_tag);
                cs_supply_oru := s.(cs_supply_oru);
                cs_supply_res := s.(cs_supply_res);
             |}
      else
        None
  end.

(** Trace execution: fold transitions left-to-right (deterministic order). *)
Fixpoint execute_trace
  (s : ConsensusState) (trace : list ConsensusTransition) : option ConsensusState :=
  match trace with
  | nil => Some s
  | t :: rest =>
      match execute_transition s t with
      | None => None
      | Some s' => execute_trace s' rest
      end
  end.

(** Alias for documentation cross-references. *)
Definition consensus_trace := list ConsensusTransition.

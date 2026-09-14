(** legacy_account_model/ConnectBlockScaffold.v
    F-1 quarantine (TASK-P0-016).

    HISTORICAL PHASE-3 account-model step. NOT ConnectBlock. NOT production.
    On RES cap overshoot this function KEEPS the prior state (silent skip).
    ReplayReorgCore [apply_block] and C++ ConnectBlock REJECT.

    Do not import this module from monetary proofs. Cap lemmas live in
    [ResSchedule.v] / [ResMintObligation.v]. Draw lives in [Execution.v] /
    [Selection.v]. *)
Require Import Coq.ZArith.ZArith.
Require Import Coq.Lists.List.
Import ListNotations.
Require Import Rigor.legacy_account_model.Types.
From Rigor Require Import RigorBytes.
Include RigorBytes.
Require Import Rigor.legacy_account_model.State.
Require Import ConvergeRoot.
Require Import Execution.
Require Import ResSchedule.

Definition legacy_RES_MINT_AMOUNT (height : Z) : Z := 0.
Definition legacy_MAX_RES_SUPPLY : Z := RES_MAX_SUPPLY.

Record DrawTrace := {
  convergence_root_bytes : list byte;
  winner : option Tx;
}.

Record BlockResult := {
  st : State;
  draw : DrawTrace;
}.

(** UNSAFE relative to production: overshoot → identity on [s], not reject. *)
Definition connect_block_scaffold (s : State) (b : Block) : BlockResult :=
  let draw_tx := Execute b 0%Z b.(prev_block_bytes) in
  let F := build_F b.(txs) in
  let C := ConvergeRoot F in
  let st' :=
    match draw_tx with
    | None => s
    | Some _ =>
        if Z.gtb (s.(RES_supply) + legacy_RES_MINT_AMOUNT b.(height)) legacy_MAX_RES_SUPPLY then
          s
        else
          {| ORU_supply := s.(ORU_supply);
             RES_supply := s.(RES_supply) + legacy_RES_MINT_AMOUNT b.(height);
             current_height := b.(height);
             last_hash := s.(last_hash);
          |}
    end in
  {| st := st';
     draw := {| convergence_root_bytes := C; winner := draw_tx |};
  |}.

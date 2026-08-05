(** ConnectBlock scaffold — PHASE-3 partial state machine (not full C++ parity). *)
Require Import Coq.ZArith.ZArith.
Require Import Coq.Lists.List.
Import ListNotations.
Require Import Types.
From Rigor Require Import RigorBytes.
Include RigorBytes.
Require Import State.
Require Import ConvergeRoot.
Require Import Execution.

Definition RES_MINT_AMOUNT : Z := 10000%Z.
Definition MAX_RES_SUPPLY : Z := 21000000 * 100%Z.

Record DrawTrace := {
  convergence_root_bytes : list byte;
  winner : option Tx;
}.

Record BlockResult := {
  st : State;
  draw : DrawTrace;
}.

(** Simplified block step: draw + optional RES mint increment (no UTXO graph). *)
Definition connect_block_scaffold (s : State) (b : Block) : BlockResult :=
  let draw_tx := Execute b 0%Z b.(prev_block_bytes) in
  let F := build_F b.(txs) in
  let C := ConvergeRoot F in
  let st' :=
    match draw_tx with
    | None => s
    | Some _ =>
        if Z.gtb (s.(RES_supply) + RES_MINT_AMOUNT) MAX_RES_SUPPLY then
          s
        else
          {| ORU_supply := s.(ORU_supply);
             RES_supply := s.(RES_supply) + RES_MINT_AMOUNT;
             current_height := b.(height);
             last_hash := s.(last_hash);
          |}
    end in
  {| st := st';
     draw := {| convergence_root_bytes := C; winner := draw_tx |};
  |}.

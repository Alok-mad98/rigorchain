(** ConnectBlock scaffold — PHASE-3 partial state machine (not full C++ parity). *)
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

(** RFC-017. The mint amount is no longer a constant: it is [get_res_mint_reward] evaluated at
    the block's height, which is what production [ConnectBlock] enforces
    (`core/src/validation.cpp`). The scaffold takes the height from the block rather than the
    state so that the amount a block may mint is a function of the block being connected, exactly
    as in the C++ check.

    The cap is unchanged at 21M RES; only the scale moved, from 10^2 to 10^6 atoms per RES. *)
Definition RES_MINT_AMOUNT (height : Z) : Z := get_res_mint_reward height.
Definition MAX_RES_SUPPLY : Z := RES_MAX_SUPPLY.

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
        if Z.gtb (s.(RES_supply) + RES_MINT_AMOUNT b.(height)) MAX_RES_SUPPLY then
          s
        else
          {| ORU_supply := s.(ORU_supply);
             RES_supply := s.(RES_supply) + RES_MINT_AMOUNT b.(height);
             current_height := b.(height);
             last_hash := s.(last_hash);
          |}
    end in
  {| st := st';
     draw := {| convergence_root_bytes := C; winner := draw_tx |};
  |}.

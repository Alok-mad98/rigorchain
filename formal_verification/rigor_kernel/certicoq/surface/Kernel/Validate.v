(** RigorSurface.Kernel.Validate — [validate_kernel] dispatcher.

    Must call the same six checkers. No theorems.
*)
From RigorSurface Require Import Kernel.UTXO.
From RigorSurface Require Import Kernel.ORU.
From RigorSurface Require Import Kernel.RES.
From RigorSurface Require Import Kernel.Script.
From RigorSurface Require Import Kernel.Pow.
From RigorSurface Require Import Kernel.ASERT.

Inductive KernelInput :=
| KI_UTXO : UTXOState -> KernelInput
| KI_ORU : ORUState -> KernelInput
| KI_RES : RESState -> KernelInput
| KI_Script : ScriptState -> KernelInput
| KI_Pow : PowHeader -> KernelInput
| KI_ASERT : ASERTInput -> KernelInput.

Definition validate_kernel (input : KernelInput) : bool :=
  match input with
  | KI_UTXO s => check_utxo s
  | KI_ORU s => check_oru s
  | KI_RES s => check_res s
  | KI_Script s => check_script s
  | KI_Pow h => check_pow h
  | KI_ASERT i => check_asert i
  end.

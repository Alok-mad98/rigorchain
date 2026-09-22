(** RigorSurface.Kernel.Validate — [validate_kernel] dispatcher.

    Must call the same six checkers. No theorems.
*)
From RigorSurface Require Import Kernel.UTXO.
From RigorSurface Require Import Kernel.ORU.
From RigorSurface Require Import Kernel.RES.
From RigorSurface Require Import Kernel.Script.
From RigorSurface Require Import Kernel.Pow.
From RigorSurface Require Import Kernel.ASERT.

Inductive KernelValidationRequest :=
| KI_UTXO : UTXOState -> KernelValidationRequest
| KI_ORU : ORUState -> KernelValidationRequest
| KI_RES : RESState -> KernelValidationRequest
| KI_Script : ScriptState -> KernelValidationRequest
| KI_Pow : PowHeader -> KernelValidationRequest
| KI_ASERT : ASERTInput -> KernelValidationRequest.

Definition validate_kernel (input : KernelValidationRequest) : bool :=
  match input with
  | KI_UTXO s => check_utxo s
  | KI_ORU s => check_oru s
  | KI_RES s => check_res s
  | KI_Script s => check_script s
  | KI_Pow h => check_pow h
  | KI_ASERT i => check_asert i
  end.

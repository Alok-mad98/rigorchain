(** Extraction operation trace. Same kernel operation, same sequence
    of interface labels. Not compiler correctness.

    NOT CLAIMED: OCaml evaluation order, ABI, CPU.
*)
From Coq Require Import List.
Import ListNotations.
From RigorKernel Require Import ExtractionBoundary.ExtractionInterface.

Definition abstract_kernel_operation := CoqKernelFunction.
Definition extracted_kernel_operation := ExtractedKernelInterface.

Fixpoint extract_trace (ops : list abstract_kernel_operation)
  : list extracted_kernel_operation :=
  match ops with
  | [] => []
  | o :: rest => kernel_call_boundary o :: extract_trace rest
  end.

(** Independent expected labels. Must not call [kernel_call_boundary]. *)
Fixpoint expected_trace (ops : list abstract_kernel_operation)
  : list extracted_kernel_operation :=
  match ops with
  | [] => []
  | CK_UTXO :: rest => EK_UTXO :: expected_trace rest
  | CK_ORU :: rest => EK_ORU :: expected_trace rest
  | CK_RES :: rest => EK_RES :: expected_trace rest
  | CK_Script :: rest => EK_Script :: expected_trace rest
  | CK_PoW :: rest => EK_PoW :: expected_trace rest
  | CK_ASERT :: rest => EK_ASERT :: expected_trace rest
  end.

Theorem operation_trace_preserved :
  forall ops : list abstract_kernel_operation,
    extract_trace ops = expected_trace ops.
Proof.
  induction ops as [|o rest IH]; simpl.
  - reflexivity.
  - destruct o; simpl; rewrite IH; reflexivity.
Qed.

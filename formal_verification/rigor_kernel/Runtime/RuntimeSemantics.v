(** Runtime semantics: extracted interface tag on a runtime state.

    Coq model only. Not OCaml evaluation.

    NOT CLAIMED: compiler, CPU, native binary.
*)
From Coq Require Import ZArith.ZArith.
From RigorKernel Require Import ExecutionBoundary.ConcreteExecutionState.
From RigorKernel Require Import ExtractionBoundary.ExtractionInterface.
From RigorKernel Require Import Runtime.RuntimeState.

Local Open Scope Z_scope.

Definition default_executable : ExecutableKernelState :=
  {| ek_utxo_flag := true;
     ek_fees := 0;
     ek_burn := 0;
     ek_subsidy := 0;
     ek_res_stock := 0;
     ek_res_cap := 0;
     ek_oru_stock := 0;
     ek_script_ok := true;
     ek_pow_ok := true;
     ek_asert_ok := true |}.

Definition extracted_interface_to_runtime
    (e : ExtractedKernelInterface) : RuntimeKernelState :=
  embed_executable default_executable (extracted_to_runtime_tag e).

(** ConsensusModel.KernelInputV2

    Specification of a later marshalling object. Does **not** modify
    [Record KernelInput] in Kernel/UnifiedTransition.v.

    Existing record (unchanged, ten bool/Z fields) is copied here only as
    [ExistingKernelInput] so the refinement interface can be typed.
    That copy is a *shape view*. It is not the production Coq record,
    and C++ still does not construct either object.

    KernelInputV2 sources:
      Block, UTXOState, ValidationResult
    plus the existing bools:
      ki_pow_ok, ki_asert_ok

    Future: kernel_input_of_validation : ValidationResult -> KernelInput

    CPP_KERNELINPUT_EQUIVALENCE = NOT PROVED
*)
From Coq Require Import ZArith.ZArith.
From ConsensusModel Require Import BlockModel.
From ConsensusModel Require Import UTXOState.
From ConsensusModel Require Import ValidationResult.

Local Open Scope Z_scope.

(** Shape view of UnifiedTransition.KernelInput. Do not replace that file. *)
Record ExistingKernelInput := {
  ki_utxo_ok : bool;
  ki_incoming : Z;
  ki_outgoing : Z;
  ki_fees : Z;
  ki_burn : Z;
  ki_subsidy : Z;
  ki_res_mint : Z;
  ki_script_ok : bool;
  ki_pow_ok : bool;
  ki_asert_ok : bool
}.

Record KernelInputV2 := {
  kiv2_block : Block;
  kiv2_utxo : UTXOState;
  kiv2_result : ValidationResult;
  kiv2_pow_ok : bool;
  kiv2_asert_ok : bool
}.

(* kiv2_pow_ok / kiv2_asert_ok are the v2 carriers of existing ki_pow_ok / ki_asert_ok. *)

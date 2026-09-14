(** Extraction interface: Coq executable model → extracted names.

    Maps each executable validator to an extraction entry. This is
    interface consistency, not compiler correctness.

    NOT CLAIMED: OCaml semantics, binary equivalence, CompCert.
*)

Inductive CoqKernelFunction :=
| CK_UTXO
| CK_ORU
| CK_RES
| CK_Script
| CK_PoW
| CK_ASERT.

Inductive ExtractedKernelInterface :=
| EK_UTXO
| EK_ORU
| EK_RES
| EK_Script
| EK_PoW
| EK_ASERT.

(** Named mapping. M37 sends [CK_UTXO] to [EK_ORU]. *)
Definition kernel_call_boundary (f : CoqKernelFunction) : ExtractedKernelInterface :=
  match f with
  | CK_UTXO => EK_UTXO
  | CK_ORU => EK_ORU
  | CK_RES => EK_RES
  | CK_Script => EK_Script
  | CK_PoW => EK_PoW
  | CK_ASERT => EK_ASERT
  end.

Definition boundary_aligned (f : CoqKernelFunction) : Prop :=
  match f with
  | CK_UTXO => kernel_call_boundary f = EK_UTXO
  | CK_ORU => kernel_call_boundary f = EK_ORU
  | CK_RES => kernel_call_boundary f = EK_RES
  | CK_Script => kernel_call_boundary f = EK_Script
  | CK_PoW => kernel_call_boundary f = EK_PoW
  | CK_ASERT => kernel_call_boundary f = EK_ASERT
  end.

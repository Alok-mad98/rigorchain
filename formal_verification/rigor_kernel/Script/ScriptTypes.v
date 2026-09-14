(** RigorKernel.Script.ScriptTypes — Phase A script lattice.

    Scope: consensus kernel only.

    [T_P2PKH] and [T_P2SH] exist as reserved constructors so the
    Bitcoin-legacy templates have names in the lattice. The classifier
    never returns them: 25-byte P2PKH is not a RIGOR consensus type
    (see [core/src/script/rigor_classify.cpp]).

    NOT CLAIMED:
      C++ implementation equivalence
      Full chain validity
      Agreement with [rigor_classify.cpp]
*)
From Coq Require Import List.
From RigorKernel Require Import Core.Bytes.

Definition Script := list byte.

Inductive ScriptType : Type :=
| T_P2PKH
| T_P2SH
| T_WITNESS_V0
| T_WITNESS_V1
| T_NONSTANDARD
| T_MINT
| T_CONVERGE
| T_NULL_DATA.

Definition script_type_eqb (a b : ScriptType) : bool :=
  match a, b with
  | T_P2PKH, T_P2PKH => true
  | T_P2SH, T_P2SH => true
  | T_WITNESS_V0, T_WITNESS_V0 => true
  | T_WITNESS_V1, T_WITNESS_V1 => true
  | T_NONSTANDARD, T_NONSTANDARD => true
  | T_MINT, T_MINT => true
  | T_CONVERGE, T_CONVERGE => true
  | T_NULL_DATA, T_NULL_DATA => true
  | _, _ => false
  end.

Theorem script_type_eqb_refl :
  forall t : ScriptType, script_type_eqb t t = true.
Proof. intros t. destruct t; reflexivity. Qed.

(** Consensus asset lattice induced by [ScriptType].
    Authority-free: no key, height, or chain state. *)
Inductive AssetClass : Type :=
| A_ORU
| A_RES
| A_NONE
| A_INVALID.

Definition asset_of_script_type (t : ScriptType) : AssetClass :=
  match t with
  | T_WITNESS_V0 | T_CONVERGE => A_ORU
  | T_WITNESS_V1 | T_MINT => A_RES
  | T_NULL_DATA => A_NONE
  | T_P2PKH | T_P2SH | T_NONSTANDARD => A_INVALID
  end.

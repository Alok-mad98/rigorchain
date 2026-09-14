(** RigorSurface.Script.ScriptTypes — compile-surface slice. No theorems. *)
From Coq Require Import List.
From RigorSurface Require Import Core.Bytes.

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

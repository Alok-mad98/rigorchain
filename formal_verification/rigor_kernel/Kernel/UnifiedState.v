(** Unified consensus kernel state. Coq model only.

    One [KernelState] bundles the independently verified domains.
    Not a node; not C++ ConnectBlock.

    NOT CLAIMED: full implementation, binary equivalence, CompCert.
*)
From Coq Require Import ZArith.ZArith.

Local Open Scope Z_scope.

Record UTXOState := { utxo_wf_flag : bool }.

Record EconomicState := {
  econ_fees : Z;
  econ_burn : Z;
  econ_subsidy : Z
}.

Record RESState := {
  res_stock : Z;
  res_cap : Z
}.

Record ORUState := { oru_stock : Z }.

Record ScriptState := { script_class_ok : bool }.

Record PowState := { pow_ok : bool }.

Record ASERTState := { asert_ok : bool }.

Record KernelState := {
  ks_utxo : UTXOState;
  ks_econ : EconomicState;
  ks_res : RESState;
  ks_oru : ORUState;
  ks_script : ScriptState;
  ks_pow : PowState;
  ks_asert : ASERTState
}.

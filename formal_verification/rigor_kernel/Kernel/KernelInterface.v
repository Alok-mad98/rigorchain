(** RigorKernel.Kernel.KernelInterface — executable consensus kernel.

    Scope: consensus kernel only.

    Assembles the six boolean validators. This is a kernel-model
    executable interface, not a C++ node, and not a CompCert
    refinement.

    NOT CLAIMED:
      C++ implementation equivalence
      Full node correctness
      Network / wallet / cryptographic security
*)
From RigorKernel Require Import Kernel.UTXOValidator.
From RigorKernel Require Import Kernel.ORUValidator.
From RigorKernel Require Import Kernel.RESValidator.
From RigorKernel Require Import Kernel.ScriptValidator.
From RigorKernel Require Import Kernel.PowValidator.
From RigorKernel Require Import Kernel.ASERTValidator.

Record ConsensusKernel := {
  check_utxo : UTXOState -> bool;
  check_oru : ORUState -> bool;
  check_res : RESState -> bool;
  check_script : ScriptState -> bool;
  check_pow : PowHeader -> bool;
  check_asert : ASERTInput -> bool
}.

Definition rigor_kernel : ConsensusKernel :=
  {| check_utxo := UTXOValidator.check_utxo;
     check_oru := ORUValidator.check_oru;
     check_res := RESValidator.check_res;
     check_script := ScriptValidator.check_script;
     check_pow := PowValidator.check_pow;
     check_asert := ASERTValidator.check_asert |}.

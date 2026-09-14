(** RigorKernel.extraction.ExtractKernel — LEGACY extract path.

    This leftover path is affected by P0-113 F-01: it imports
    ExtrOcamlNatInt and ExtrOcamlZInt (native integers). It is
    kept for contrast only. It is NOT evidence and MUST NOT be
    cited as the faithful extract.

    Faithful extract (inductive Z/nat, no ExtrOcamlZInt):
      extracted/Extract.v

    NOT CLAIMED:
      CompCert refinement
      Extraction semantic preservation
      Binary semantic equivalence
      Theorem-to-binary refinement
      C++ implementation equivalence
      Cryptographic security
*)
From Coq Require Import Extraction.
From Coq Require Import ExtrOcamlBasic.
From Coq Require Import ExtrOcamlNatInt.
From Coq Require Import ExtrOcamlZInt.
From RigorKernel Require Import Kernel.UTXOValidator.
From RigorKernel Require Import Kernel.ORUValidator.
From RigorKernel Require Import Kernel.RESValidator.
From RigorKernel Require Import Kernel.ScriptValidator.
From RigorKernel Require Import Kernel.PowValidator.
From RigorKernel Require Import Kernel.ASERTValidator.
From RigorKernel Require Import Kernel.KernelInterface.
From RigorKernel Require Import Kernel.KernelSoundness.

Extraction Language OCaml.

Extraction "ExtractedKernel.ml"
  UTXOValidator.check_utxo
  ORUValidator.check_oru
  RESValidator.check_res
  ScriptValidator.check_script
  PowValidator.check_pow
  ASERTValidator.check_asert
  validate_kernel
  rigor_kernel.

(** Extract the six Coq validators to extracted/ExtractedKernel.ml.

    Z and nat are left as Coq inductives (no ExtrOcamlZInt /
    ExtrOcamlNatInt). That keeps machine integers out of the
    extracted arithmetic. Compiler correctness is still not proved.

    NOT CLAIMED:
      CompCert refinement
      Full OCaml semantic preservation
      C++ implementation equivalence
*)
From Coq Require Import Extraction.
From Coq Require Import ExtrOcamlBasic.
From RigorKernel Require Import Kernel.UTXOValidator.
From RigorKernel Require Import Kernel.ORUValidator.
From RigorKernel Require Import Kernel.RESValidator.
From RigorKernel Require Import Kernel.ScriptValidator.
From RigorKernel Require Import Kernel.PowValidator.
From RigorKernel Require Import Kernel.ASERTValidator.
From RigorKernel Require Import Kernel.KernelInterface.
From RigorKernel Require Import Kernel.KernelSoundness.
From RigorKernel Require Import Kernel.ExtractionBoundary.

Extraction Language OCaml.

Extraction "ExtractedKernel.ml"
  UTXOValidator.check_utxo
  ORUValidator.check_oru
  RESValidator.check_res
  ScriptValidator.check_script
  PowValidator.check_pow
  ASERTValidator.check_asert
  validate_kernel
  extracted_check_utxo
  extracted_check_oru
  extracted_check_res
  extracted_check_script
  extracted_check_pow
  extracted_check_asert.

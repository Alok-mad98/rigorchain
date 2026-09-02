(** ExtractMakeKernelInput.v — F-4 test oracle. NOT listed in _CoqProject.

    Extracts [make_crypto_accept] / [raw_from_domain] / [all_raw] as an
    OCaml test oracle over the F-2 domain. Not production. Not linked
    into rigord. [extracted_kernel_production] stays false.

    No [Extract Constant] / [Extract Inductive] overrides.
    [ExtrOcamlNatInt]: [nat] as machine [int]. [Z] stays Coq integers.

    Re-running [coqc] rewrites the OCaml; the default production build
    does not compile this file. *)

From Coq Require Import Extraction ExtrOcamlBasic ExtrOcamlNatInt.
Require Import MakeKernelInput.

Extraction Language OCaml.
Extraction "makekernelinput_oracle.ml"
  make_crypto_accept
  raw_from_domain
  all_raw
  domain_crypto_accept
  sighash_path_consistent
  tag_mismatch_pins
  pin_v0_accept
  pin_v0_no_witness_flag
  pin_mint_accept
  extracted_kernel_production
  extracted_kernel_linked
  compcert_binary_verified_kernel
  compcert_vst_deferred
  vst_scope_makekernelinput
  vst_scope_full_kernel.

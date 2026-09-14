(** Extract classify / apply_delta / sighash_preimage.
    Not in the default _CoqProject. apply_delta is the RFC-020 conservation
    slice (authorized mint ≤ RES_MAX_SUPPLY); it is not the pole predicate. *)
From Coq Require Import Extraction ExtrOcamlBasic ExtrOcamlNatInt.
Require Import RigorV01.AssetClassification.
Require Import RigorV01.MintSupplyConservation.
Require Import RigorV01.SighashPreimage.

Extraction Language OCaml.
Extraction "classify_kernel.ml" classify parse_canonical_mint.
Extraction "supply_kernel.ml" apply_delta genesis_supply.
Extraction "sighash_kernel.ml" sighash_preimage compact_size le32 le64.

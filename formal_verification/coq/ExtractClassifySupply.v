(** Extract classify / parse_canonical_mint. Not in the default _CoqProject. *)
From Coq Require Import Extraction ExtrOcamlBasic ExtrOcamlNatInt.
Require Import RigorV01.AssetClassification.
Require Import RigorV01.MintSupplyConservation.
Require Import RigorV01.SighashPreimage.

Extraction Language OCaml.
Extraction "classify_kernel.ml" classify parse_canonical_mint.
Extraction "supply_kernel.ml" apply_delta genesis_supply.
Extraction "sighash_kernel.ml" sighash_preimage compact_size le32 le64.

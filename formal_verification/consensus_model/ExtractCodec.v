(** ExtractCodec.v — 1.3 differential test oracle. NOT in _CoqProject.

    Extracts the byte codec so it can be run against the C++
    deserializer on the same bytes. Not production. Not linked into
    rigord. Nothing here is claimed to be proved about C++.

    No [Extract Constant] and no [Extract Inductive] beyond
    [ExtrOcamlBasic], which only maps bool / option / list / prod to
    their OCaml counterparts. In particular [ExtrOcamlNatInt] is
    deliberately *not* used: mapping [nat] onto machine [int] would
    delegate arithmetic to OCaml, and the correctness of that
    delegation is part of what this differential test exists to
    probe. [nat], [N] and [Z] therefore extract as Coq datatypes and
    all arithmetic remains the extracted Coq arithmetic.

    This is only affordable because of 2.2b. While wire scalars were
    unary [nat], the extracted decoder could not run on a real amount.

    Re-running [coqc] on this file rewrites the OCaml. The default
    build does not compile it. *)

From Coq Require Import Extraction ExtrOcamlBasic.
From ConsensusModel Require Import ByteModel.
From ConsensusModel Require Import CompactSizeModel.
From ConsensusModel Require Import TransactionModel.
From ConsensusModel Require Import BlockModel.
From ConsensusModel Require Import TransactionCodec.
From ConsensusModel Require Import BlockCodec.
From ConsensusModel Require Import WireCodec.

Extraction Language OCaml.

Extraction "codec_oracle_gen.ml"
  decode_block_from
  decode_block
  decode_block_wire
  encode_block
  decode_transaction_from
  decode_transaction
  encode_transaction
  decode_compactsize
  encode_compactsize
  decode_compactsize_wire
  MAX_SIZE.

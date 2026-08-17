(** ExtractHashKernel.v — NOT listed in _CoqProject.

    Extracts [sha256_bytes] / [hash256_bytes] to OCaml. Re-running [coqc] on
    this file rewrites the extracted sources; keep it off the default build.

    This is not a refinement proof against C++. Trust includes Coq's extractor,
    OCaml, and [ExtrOcamlNatInt] ([nat] as machine [int]; golden payloads are
    far below overflow).
*)

From Coq Require Import Extraction ExtrOcamlBasic ExtrOcamlNatInt.
From Rigor Require Import RigorSha256.

Extraction Language OCaml.
(* Coq 8.15 has no Extraction Output Directory option; extract into cwd and
   let extract_hash_kernel.sh move the sources into extracted/. *)
Extraction "hash256_kernel.ml" sha256_bytes hash256_bytes.

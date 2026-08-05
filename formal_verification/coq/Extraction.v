Require Import Types.
Require Import ConvergeRoot.
Require Import Sigma.
Require Import Execution.
Require Import Selection.
From Coq Require Import Extraction.

Extraction Language OCaml.
Extraction "rigor.ml"
  Execute
  ConvergeRoot.ConvergeRoot
  Sigma.Sigma
  weight
  select_winner.

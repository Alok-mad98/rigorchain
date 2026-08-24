Require Import Rigor.legacy_account_model.Types.
Require Import ConvergeRoot.
Require Import Sigma.
Require Import Execution.
Require Import Rigor.legacy_account_model.Selection.
From Coq Require Import Extraction.

Extraction Language OCaml.
Extraction "rigor.ml"
  Execute
  ConvergeRoot.ConvergeRoot
  Sigma.Sigma
  weight
  select_winner.

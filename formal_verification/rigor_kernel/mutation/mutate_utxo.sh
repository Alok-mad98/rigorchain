#!/usr/bin/env bash
# M11, M13 — UTXO apply_block / apply_block_transition uniqueness
set -euo pipefail
# shellcheck source=lib.sh
source "$(cd "$(dirname "$0")" && pwd)/lib.sh"

# Drop the intra-block duplicate-input guard. An accepted block no
# longer implies NoDup (collect_inputs b).
run_mutation M11 apply_block_accepted_inputs_unique proofs/UTXO_Soundness/UTXOSoundness.v \
  'Definition apply_block (u : utxo_set) (b : list tx) : option utxo_set :=
  if nodupb (collect_inputs b)
  then fold_left apply_tx_opt b (Some u)
  else None.' \
  'Definition apply_block (u : utxo_set) (b : list tx) : option utxo_set :=
  fold_left apply_tx_opt b (Some u).' \
  Kernel/SafetyExtensions || true

# Drop the named uniqueness guard on the block-transition wrapper.
run_mutation M13 block_transition_utxo_soundness proofs/UTXO_Soundness/UTXOTransitionSoundness.v \
  'Definition apply_block_transition (u : utxo_set) (b : list tx) : option utxo_set :=
  if transition_inputs_unique b
  then fold_left apply_tx_opt b (Some u)
  else None.' \
  'Definition apply_block_transition (u : utxo_set) (b : list tx) : option utxo_set :=
  fold_left apply_tx_opt b (Some u).' \
  proofs/UTXO_Soundness/UTXOTransitionSoundness || true

#!/usr/bin/env bash
# M3, M4, M7, M9, M14 — ORU / fee / script / UTXO
set -euo pipefail
# shellcheck source=lib.sh
source "$(cd "$(dirname "$0")" && pwd)/lib.sh"

# Drop "- burn" from the conservation equation.
run_mutation M3 oru_conservation_extended proofs/ORU_Conservation/ORUConservation.v \
  '    old_supply tx + subsidy tx + fees tx - burn tx.' \
  '    old_supply tx + subsidy tx + fees tx.' \
  proofs/ORU_Conservation/ORUConservation || true

# fee creates ORU: fee := out - in  (sign flip).
run_mutation M4 oru_conservation_tx proofs/ORU_Conservation/ORUConservation.v \
  'Definition fee (t : kernel_tx) : Z := kt_oru_in t - kt_oru_out t.' \
  'Definition fee (t : kernel_tx) : Z := kt_oru_out t - kt_oru_in t.' \
  proofs/ORU_Conservation/ORUConservation || true

# Classifier overlap: a supported v0 witness is labelled T_MINT.
run_mutation M7 recognizer_pairwise_disjoint Script/ScriptClassifier.v \
  '      if Nat.eqb (length prog) 20%nat then T_WITNESS_V0 else T_NONSTANDARD' \
  '      if Nat.eqb (length prog) 20%nat then T_MINT else T_NONSTANDARD' \
  Script/ScriptClassifier || true

# Allow double-spend residue: apply no longer strips inputs.
run_mutation M9 apply_body_wf proofs/UTXO_Soundness/UTXOSoundness.v \
  'Definition apply_body (u : utxo_set) (t : tx) : utxo_set :=
  let u1 := utxo_remove_all u (tx_in t) in
  let u2 := utxo_remove_all u1 (output_outpoints (tx_id t) (length (tx_out t))) in
  utxo_insert_outputs u2 (tx_id t) (tx_out t) 0%nat.' \
  'Definition apply_body (u : utxo_set) (t : tx) : utxo_set :=
  utxo_insert_outputs u (tx_id t) (tx_out t) 0%nat.' \
  proofs/UTXO_Soundness/UTXOSoundness || true

# Ambiguous opcode class: mint marker classified as converge.
run_mutation M14 recognizer_trace_disjoint Script/Semantics/ScriptTrace.v \
  '  if Nat.eqb b RIGOR_MARKER_RES_MINT then OK_MARKER_MINT' \
  '  if Nat.eqb b RIGOR_MARKER_RES_MINT then OK_MARKER_CONVERGE' \
  Script/Semantics/ScriptTrace || true

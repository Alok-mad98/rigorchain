#!/usr/bin/env bash
# M36–M37 — extraction interface / negative-vector reject
set -euo pipefail
# shellcheck source=lib.sh
source "$(cd "$(dirname "$0")" && pwd)/lib.sh"

# Force validators to accept the V1–V6 invalid cases.
run_mutation M36 golden_negative_vectors_rejected \
  ExtractionBoundary/GoldenNegatives.v \
  'Definition force_accept_invalid : bool := false.' \
  'Definition force_accept_invalid : bool := true.' \
  ExtractionBoundary/GoldenNegatives || true

# Change the UTXO extraction mapping to the ORU entry.
run_mutation M37 "extraction_interface_injective extraction_interface_surjective operation_trace_preserved" \
  ExtractionBoundary/ExtractionInterface.v \
  '  | CK_UTXO => EK_UTXO' \
  '  | CK_UTXO => EK_ORU' \
  ExtractionBoundary/ExtractionModel || true

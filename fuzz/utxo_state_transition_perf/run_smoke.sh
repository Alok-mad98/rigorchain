#!/usr/bin/env bash
# P0-066 UTXO state-transition performance fuzz smoke. Crash = FAIL.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SRC="$ROOT/core/src"
OUT="${TMPDIR:-/tmp}/utxo_state_transition_perf_smoke"
CXX="${CXX:-$(command -v g++ || command -v c++)}"
mkdir -p "$(dirname "$OUT")"
"$CXX" -O2 -std=c++17 -ffunction-sections -fdata-sections \
  -DUTXO_STATE_TRANSITION_PERF_MAIN \
  -I"$SRC" \
  -o "$OUT" \
  "$ROOT/fuzz/utxo_state_transition_perf/utxo_state_transition_perf.cpp" \
  "$SRC/consensus/ledger/ledger_transition.cpp" \
  "$SRC/consensus/rigor_issuance.cpp" \
  "$ROOT/tools/consensus_kernel_refinement_log_stub.cpp" \
  "$SRC/script/rigor_classify.cpp" \
  "$SRC/script/script.cpp" \
  "$SRC/uint256.cpp" \
  "$SRC/util/strencodings.cpp" \
  "$SRC/crypto/sha256.cpp" \
  "$SRC/arith_uint256.cpp" \
  -Wl,--gc-sections
set +e
"$OUT" smoke
rc=$?
set -e
if [[ $rc -ne 0 ]]; then
  echo "CONSENSUS_KERNEL_CRASH_FOUND"
  exit 1
fi
echo "FUZZ=PASS"
echo "CRASH=0"

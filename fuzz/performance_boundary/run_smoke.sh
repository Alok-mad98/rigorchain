#!/usr/bin/env bash
# P0-077 performance-boundary fuzz smoke. Crash or state drift = FAIL.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SRC="$ROOT/core/src"
OUT="${TMPDIR:-/tmp}/performance_boundary_smoke"
CXX="${CXX:-$(command -v g++ || command -v c++)}"
mkdir -p "$(dirname "$OUT")"
"$CXX" -O2 -std=c++17 -ffunction-sections -fdata-sections \
  -DPERFORMANCE_BOUNDARY_MAIN \
  -I"$SRC" \
  -o "$OUT" \
  "$ROOT/fuzz/performance_boundary/performance_boundary.cpp" \
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
  echo "CRASH_FOUND=YES"
  exit 1
fi
echo "FUZZ=PASS"
echo "CRASH=0"
echo "NO_STATE_DIFFERENCE=YES"
echo "NO_CONSENSUS_DIFFERENCE=YES"

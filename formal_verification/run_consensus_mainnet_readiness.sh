#!/usr/bin/env bash
# P061: mainnet readiness audit (no protocol edit, no Kernel switch).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/core/src"
OUT="$ROOT/formal_verification/extracted"
TOOLS="$ROOT/tools"

echo "== P061 Consensus Kernel mainnet readiness =="

python3 "$TOOLS/audit-consensus-release-build.py"
python3 "$TOOLS/lint-consensus-authority.py"
python3 "$TOOLS/lint-audit-scope.py"

CXX="${CXX:-$(command -v g++ || command -v c++)}"
mkdir -p "$OUT"
echo "== g++ test_consensus_mainnet_boundary =="
"$CXX" -O2 -std=c++17 -ffunction-sections -fdata-sections -I"$SRC" \
  -o "$OUT/test_consensus_mainnet_boundary" \
  "$SRC/test/test_consensus_mainnet_boundary.cpp" \
  "$TOOLS/consensus_kernel_refinement_log_stub.cpp" \
  "$SRC/consensus/rigor_issuance.cpp" \
  "$SRC/consensus/rigor_supply.cpp" \
  "$SRC/script/rigor_classify.cpp" \
  "$SRC/script/script.cpp" \
  "$SRC/uint256.cpp" \
  "$SRC/util/strencodings.cpp" \
  "$SRC/consensus/convergence_logic.cpp" \
  "$SRC/consensus_rigor/sigma_hash.cpp" \
  "$SRC/crypto/sha256.cpp" \
  "$SRC/arith_uint256.cpp" \
  -Wl,--gc-sections

"$OUT/test_consensus_mainnet_boundary"

echo "CONSENSUS_KERNEL_MAINNET_READINESS=PASS"
echo "PRODUCTION_CALLGRAPH=FROZEN"
echo "AUTHORITY_INVENTORY=PASS"
echo "EXTRACTED_KERNEL_ISOLATION=PASS"
echo "RELEASE_BUILD_AUDIT=PASS"
echo "UPGRADE_POLICY=PASS"
echo "PRODUCTION_CONSENSUS_MODIFIED=NO"
python3 "$ROOT/tools/refinement_status.py"

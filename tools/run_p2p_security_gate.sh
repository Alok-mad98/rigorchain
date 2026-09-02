#!/usr/bin/env bash
# P0-076: P2P security boundary gate.
# Does not rewrite consensus formulas. Does not modify Genesis / Kernel.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/core/src"
OUT="$ROOT/formal_verification/extracted"
TOOLS="$ROOT/tools"

echo "== P076 P2P security boundary gate =="

python3 "$TOOLS/lint-p2p-security-boundary.py"
python3 "$TOOLS/lint-consensus-authority.py"
python3 "$TOOLS/lint-consensus-authority-security.py"
python3 "$TOOLS/lint-consensus-kernel-freeze.py"

CXX="${CXX:-$(command -v g++ || command -v c++)}"
mkdir -p "$OUT"

echo "== g++ test_p2p_security_boundary =="
"$CXX" -O2 -std=c++17 -ffunction-sections -fdata-sections \
  -DP2P_SECURITY_STANDALONE \
  -I"$SRC" \
  -o "$OUT/test_p2p_security_boundary" \
  "$SRC/test/test_p2p_security_boundary.cpp" \
  "$SRC/net/security/p2p_message_boundary.cpp" \
  -Wl,--gc-sections

"$OUT/test_p2p_security_boundary"

echo "== fuzz/p2p_security smoke =="
bash "$ROOT/fuzz/p2p_security/run_smoke.sh"

echo "P2P_SECURITY_BOUNDARY = CLOSED"
echo "P2P_CONSENSUS_BYPASS = NONE"
echo "CRASH_FOUND = NO"
echo "CONSENSUS_MODIFIED = NO"
echo "P2P_SECURITY_BOUNDARY_CLOSED"

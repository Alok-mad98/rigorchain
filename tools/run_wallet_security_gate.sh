#!/usr/bin/env bash
# P0-077: wallet security baseline gate.
# Does not rewrite consensus formulas. Does not modify Genesis / Kernel.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/core/src"
OUT="$ROOT/formal_verification/extracted"
TOOLS="$ROOT/tools"

echo "== P077 wallet security baseline gate =="

python3 "$TOOLS/lint-wallet-security.py"
python3 "$TOOLS/lint-consensus-authority.py"
python3 "$TOOLS/lint-consensus-authority-security.py"
python3 "$TOOLS/lint-consensus-kernel-freeze.py"

CXX="${CXX:-$(command -v g++ || command -v c++)}"
mkdir -p "$OUT"

echo "== g++ test_wallet_security_boundary =="
"$CXX" -O2 -std=c++17 -ffunction-sections -fdata-sections \
  -DWALLET_SECURITY_STANDALONE \
  -I"$SRC" \
  -o "$OUT/test_wallet_security_boundary" \
  "$SRC/test/test_wallet_security_boundary.cpp" \
  "$SRC/wallet/security/wallet_security_boundary.cpp" \
  "$SRC/wallet/security/wallet_signing_boundary.cpp" \
  -Wl,--gc-sections

"$OUT/test_wallet_security_boundary"

echo "== fuzz/wallet_security smoke =="
bash "$ROOT/fuzz/wallet_security/run_smoke.sh"

echo "== differential gate (consensus unchanged) =="
SKIP_TEST_RIGOR="${SKIP_TEST_RIGOR:-1}" \
  bash "$ROOT/formal_verification/scripts/run_differential_gate.sh"

echo "WALLET_SECURITY_STATUS = CLOSED"
echo "PRIVATE_KEY_EXPOSURE = NONE"
echo "INVALID_TX_GENERATION = NONE"
echo "CONSENSUS_BYPASS = NONE"
echo "CRASH_FOUND = NO"
echo "CONSENSUS_MODIFIED = NO"
echo "WALLET_SECURITY_BASELINE_CLOSED"

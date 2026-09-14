#!/usr/bin/env bash
# P075: genesis consensus release audit. Does not rewrite formulas.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/core/src"
OUT="$ROOT/formal_verification/extracted"
TOOLS="$ROOT/tools"

echo "== P075 genesis consensus release audit =="

python3 "$TOOLS/lint-genesis-consensus-release-audit.py"
python3 "$TOOLS/lint-genesis-consensus-authority.py"
python3 "$TOOLS/lint-consensus-authority.py"
python3 "$TOOLS/lint-consensus-authority-security.py"

CXX="${CXX:-$(command -v g++ || command -v c++)}"
mkdir -p "$OUT"
echo "== g++ test_genesis_consensus_release_audit =="
"$CXX" -O2 -std=c++17 -ffunction-sections -fdata-sections -I"$SRC" \
  -o "$OUT/test_genesis_consensus_release_audit" \
  "$SRC/test/test_genesis_consensus_release_audit.cpp" \
  "$SRC/consensus/ledger/ledger_transition.cpp" \
  "$SRC/consensus/rigor_issuance.cpp" \
  "$SRC/consensus/rigor_supply.cpp" \
  "$TOOLS/consensus_kernel_refinement_log_stub.cpp" \
  "$SRC/script/rigor_classify.cpp" \
  "$SRC/script/script.cpp" \
  "$SRC/uint256.cpp" \
  "$SRC/util/strencodings.cpp" \
  "$SRC/crypto/sha256.cpp" \
  "$SRC/arith_uint256.cpp" \
  -Wl,--gc-sections

"$OUT/test_genesis_consensus_release_audit"

echo "GENESIS_CONSENSUS_READY=TRUE"
echo "REFINEMENT_PROVED=TRUE"
echo "EQUIVALENCE_PROVED=FALSE"
echo "GENESIS_CONSENSUS_RELEASE_AUDIT=CLOSED"

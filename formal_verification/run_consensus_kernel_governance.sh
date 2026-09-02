#!/usr/bin/env bash
# P060: Consensus Kernel governance lock. Does not switch production.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/core/src"
OUT="$ROOT/formal_verification/extracted"
TOOLS="$ROOT/tools"

echo "== P060 Consensus Kernel governance =="

python3 "$TOOLS/lint-consensus-authority.py"

if [[ "${SKIP_COQ:-0}" != "1" ]]; then
  python3 "$TOOLS/check-coq-consensus-assumptions.py"
else
  echo "SKIP_COQ=1 (assumption gate already run or Coq job is separate)"
fi

CXX="${CXX:-$(command -v g++ || command -v c++)}"
mkdir -p "$OUT"
echo "== g++ test_consensus_kernel_governance =="
"$CXX" -O2 -std=c++17 -ffunction-sections -fdata-sections -I"$SRC" \
  -o "$OUT/test_consensus_kernel_governance" \
  "$SRC/test/test_consensus_kernel_governance.cpp" \
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

RIGOR_ROOT="$ROOT" "$OUT/test_consensus_kernel_governance" "$ROOT"

echo "CONSENSUS_KERNEL_GOVERNANCE=CLOSED"
echo "AUTHORITY_LOCK=PASS"
echo "GOLDEN_VECTOR_FREEZE=PASS"
echo "DUPLICATE_RULE_DETECTION=PASS"
echo "PRODUCTION_CONSENSUS_MODIFIED=NO"
python3 "$ROOT/tools/refinement_status.py"

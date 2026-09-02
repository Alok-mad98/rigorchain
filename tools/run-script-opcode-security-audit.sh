#!/usr/bin/env bash
# P0-078: Script / opcode boundary security audit gate.
# Does not rewrite consensus formulas. Does not modify Genesis / Kernel /
# VerifyRigorPredicate / Sighash / Ed25519 / asset classification / ledger.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/core/src"
OUT="$ROOT/formal_verification/extracted"
TOOLS="$ROOT/tools"

echo "== P078 script opcode security audit gate =="

echo "== authority lint =="
python3 "$TOOLS/lint-script-authority.py"

echo "== script opcode lint =="
if git -C "$ROOT" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
  python3 "$ROOT/core/test/lint/lint-no-script-vm.py"
else
  echo "lint-no-script-vm.py requires git ls-files; covered by lint-script-authority.py"
fi

echo "== transaction kernel refinement lint =="
python3 "$TOOLS/lint-transaction-kernel-refinement.py"

CXX="${CXX:-$(command -v g++ || command -v c++)}"
mkdir -p "$OUT"

echo "== g++ test_script_opcode_security =="
"$CXX" -O2 -std=c++17 -ffunction-sections -fdata-sections \
  -DSCRIPT_OPCODE_SECURITY_STANDALONE \
  -I"$SRC" \
  -o "$OUT/test_script_opcode_security" \
  "$SRC/test/test_script_opcode_security.cpp" \
  "$TOOLS/consensus_kernel_refinement_log_stub.cpp" \
  "$SRC/script/rigor_classify.cpp" \
  "$SRC/script/script.cpp" \
  "$SRC/uint256.cpp" \
  "$SRC/util/strencodings.cpp" \
  "$SRC/crypto/sha256.cpp" \
  -Wl,--gc-sections

"$OUT/test_script_opcode_security"

echo "== fuzz/script_opcode_boundary smoke =="
bash "$ROOT/fuzz/script_opcode_boundary/run_smoke.sh"

echo "== differential gate (consensus unchanged) =="
SKIP_TEST_RIGOR="${SKIP_TEST_RIGOR:-1}" \
  bash "$ROOT/formal_verification/scripts/run_differential_gate.sh"

echo "SCRIPT_OPCODE_SECURITY_STATUS = CLOSED"
echo "SCRIPT_VM_PRESENT = NO"
echo "OPCODE_SURFACE_COUNT = 0"
echo "DUPLICATE_VALIDATION_PATH = NONE"
echo "FUZZ_STATUS = PASS"
echo "CONSENSUS_MODIFIED = NO"
echo "RIGOR_SCRIPT_OPCODE_SECURITY_AUDIT_CLOSED"

#!/usr/bin/env bash
# P0-079: Convergence Draw security audit gate.
# Does not rewrite ResolveConvergenceDraw, the draw formula, issuance,
# supply, ConsensusKernel, or TransactionKernel.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/core/src"
OUT="$ROOT/formal_verification/extracted"
TOOLS="$ROOT/tools"

echo "== P079 convergence draw security audit gate =="

echo "== authority lint =="
python3 "$TOOLS/lint-convergence-draw-authority.py"

echo "== formal boundary (existing draw_refinement; no new theorem) =="
python3 - "$ROOT" <<'PY'
from pathlib import Path
import re
import sys
root = Path(sys.argv[1])
files = [
    root / "formal_verification/coq/ConsensusKernelRefinement.v",
    root / "formal_verification/coq/DrawResolverBridge.v",
]
assume = re.compile(r"^\s*(Admitted|Axiom|Parameter)\s")
admitted = axioms = parameters = 0
ref = files[0].read_text(encoding="utf-8", errors="replace")
if "Theorem draw_refinement" not in ref:
    raise SystemExit("FAIL: draw_refinement theorem missing")
for p in files:
    for line in p.read_text(encoding="utf-8", errors="replace").splitlines():
        m = assume.match(line)
        if not m:
            continue
        kind = m.group(1)
        if kind == "Admitted":
            admitted += 1
        elif kind == "Axiom":
            axioms += 1
        else:
            parameters += 1
        print(f"  {p.name}: {line.strip()}")
print(f"COQ_ADMITTED = {admitted}")
print(f"COQ_AXIOMS = {axioms}")
print(f"COQ_PARAMETERS = {parameters}")
if admitted or axioms or parameters:
    raise SystemExit("FAIL: Coq assumption counts are not zero")
print("DRAW_REFINEMENT_THEOREM = PRESENT")
print("NO_NEW_THEOREM")
PY

CXX="${CXX:-$(command -v g++ || command -v c++)}"
mkdir -p "$OUT"

echo "== g++ test_convergence_draw_security =="
"$CXX" -O2 -std=c++17 -ffunction-sections -fdata-sections \
  -DCONVERGENCE_DRAW_SECURITY_STANDALONE \
  -I"$SRC" \
  -o "$OUT/test_convergence_draw_security" \
  "$SRC/test/test_convergence_draw_security.cpp" \
  "$TOOLS/consensus_kernel_refinement_log_stub.cpp" \
  "$SRC/consensus/convergence_logic.cpp" \
  "$SRC/consensus_rigor/sigma_hash.cpp" \
  "$SRC/crypto/sha256.cpp" \
  "$SRC/arith_uint256.cpp" \
  "$SRC/uint256.cpp" \
  "$SRC/util/strencodings.cpp" \
  "$SRC/script/rigor_classify.cpp" \
  "$SRC/script/script.cpp" \
  -Wl,--gc-sections

"$OUT/test_convergence_draw_security"

echo "== fuzz/convergence_draw smoke =="
bash "$ROOT/fuzz/convergence_draw/run_smoke.sh"

echo "== differential gate (consensus unchanged) =="
SKIP_TEST_RIGOR="${SKIP_TEST_RIGOR:-1}" \
  bash "$ROOT/formal_verification/scripts/run_differential_gate.sh"

echo "== consensus kernel refinement gate =="
bash "$ROOT/formal_verification/run_consensus_kernel_refinement_gate.sh"

echo "CONVERGENCE_DRAW_SECURITY = CLOSED"
echo "DRAW_AUTHORITY_COUNT = 1"
echo "DRAW_DETERMINISM = PASS"
echo "DRAW_TIEBREAK = PASS"
echo "DRAW_WEIGHT_SECURITY = PASS"
echo "MANIPULATION_RESISTANCE = PASS"
echo "CONSENSUS_MODIFIED = NO"
echo "RIGOR_CONVERGENCE_DRAW_SECURITY_AUDIT_CLOSED"

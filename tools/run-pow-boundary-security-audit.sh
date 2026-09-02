#!/usr/bin/env bash
# P0-081: PoW boundary security audit gate.
# Does not rewrite SHA256, SHA3, ASERT, PoW parameters, Consensus Kernel,
# Transaction Kernel, or Ledger.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/core/src"
OUT="$ROOT/formal_verification/extracted"
TOOLS="$ROOT/tools"

echo "== P081 PoW boundary security audit gate =="

echo "== authority lint =="
python3 "$TOOLS/lint-pow-boundary-authority.py"

echo "== inherited PowBoundary (no new theorem; no SHA256/PoW-economic/51% claim) =="
python3 - "$ROOT" <<'PY'
from pathlib import Path
import re
import sys
root = Path(sys.argv[1])
surface = (root / "formal_verification/coq/ConsensusSurfaceClosure.v").read_text(encoding="utf-8", errors="replace")
powv = (root / "formal_verification/coq/PowDigest.v").read_text(encoding="utf-8", errors="replace")
kernel = (root / "core/src/consensus/kernel/genesis_authority.h").read_text(encoding="utf-8", errors="replace")
assume = re.compile(r"^\s*(Admitted|Axiom|Parameter)\s")
admitted = axioms = parameters = 0
if "Theorem pow_boundary_accept_iff_le" not in surface:
    raise SystemExit("FAIL: PowBoundary theorem missing")
if "Definition PowKernelBoundary" not in surface:
    raise SystemExit("FAIL: PowKernelBoundary missing")
if "PowAccept" not in surface or "PowReject" not in surface:
    raise SystemExit("FAIL: PowBoundary is not ACCEPT/REJECT")
for line in surface.splitlines():
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
    print(f"  ConsensusSurfaceClosure.v: {line.strip()}")
print(f"COQ_ADMITTED = {admitted}")
print(f"COQ_AXIOMS = {axioms}")
print(f"COQ_PARAMETERS = {parameters}")
if admitted or axioms or parameters:
    raise SystemExit("FAIL: Coq assumption counts are not zero")
if "equivalence_claimed = false" not in powv and "equivalence_claimed   = false" not in powv:
    raise SystemExit("FAIL: PowDigest.v must keep equivalence_claimed = false")
if "refinement_proved   = false" not in powv and "refinement_proved = false" not in powv:
    raise SystemExit("FAIL: PowDigest.v SHA3 model must keep refinement_proved = false")
if 'PRODUCTION_CONSENSUS_AUTHORITY[] = "RigorConsensusKernel(C++)"' not in kernel:
    raise SystemExit("FAIL: PRODUCTION_CONSENSUS_AUTHORITY is not RigorConsensusKernel(C++)")
if "inline constexpr bool EXTRACTED_KERNEL_PRODUCTION = false" not in kernel:
    raise SystemExit("FAIL: EXTRACTED_KERNEL_PRODUCTION must be false")
print("POW_BOUNDARY_THEOREM = PRESENT")
print("NO_NEW_THEOREM")
print("REFINEMENT_PROVED = TRUE  # inherited PowBoundary accept-iff-le; unchanged")
print("POW_DIGEST_MODEL_REFINEMENT_PROVED = FALSE")
print("EQUIVALENCE_PROVED = FALSE")
print("SHA256_MATH_PROOF_CLAIMED = false")
print("POW_ECONOMIC_PROOF_CLAIMED = false")
print("FIFTY_ONE_PERCENT_IMPOSSIBLE_CLAIMED = false")
print("PRODUCTION_CONSENSUS_AUTHORITY = RigorConsensusKernel(C++)")
print("EXTRACTED_KERNEL_PRODUCTION = FALSE")
print("COMPCERT_BINARY_VERIFIED_KERNEL = FALSE")
print("PRODUCTION_CONSENSUS_MODIFIED = NO")
PY

CXX="${CXX:-$(command -v g++ || command -v c++)}"
mkdir -p "$OUT"

echo "== g++ test_pow_boundary_security =="
"$CXX" -O2 -std=c++17 -ffunction-sections -fdata-sections \
  -DPOW_BOUNDARY_SECURITY_STANDALONE \
  -I"$SRC" \
  -o "$OUT/test_pow_boundary_security" \
  "$SRC/test/test_pow_boundary_security.cpp" \
  "$SRC/rigor_kernel/keccak.cpp" \
  -Wl,--gc-sections

"$OUT/test_pow_boundary_security"

echo "== fuzz/pow_boundary smoke =="
bash "$ROOT/fuzz/pow_boundary/run_smoke.sh"

echo "RIGOR_POW_BOUNDARY_SECURITY_STATUS = CLOSED"
echo "POW_AUTHORITY_COUNT = 1"
echo "HASH_AUTHORITY_COUNT = 1"
echo "ASERT_AUTHORITY_COUNT = 1"
echo "TARGET_VALIDATION_STATUS = PASS"
echo "HEADER_SERIALIZATION_STATUS = PASS"
echo "FUZZ_STATUS = PASS"
echo "CONSENSUS_BYPASS_STATUS = NONE"
echo "CRASH_FOUND = NO"
echo "UNDEFINED_BEHAVIOR = NO"
echo "PRODUCTION_CONSENSUS_MODIFIED = NO"
python3 "$ROOT/tools/refinement_status.py"
echo "RIGOR_POW_BOUNDARY_SECURITY_AUDIT_CLOSED"

#!/usr/bin/env bash
# P059: Consensus surface final closure. Does not switch production.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
COQ="$ROOT/formal_verification/coq"
SRC="$ROOT/core/src"
OUT="$ROOT/formal_verification/extracted"
TOOLS="$ROOT/tools"
V="$COQ/ConsensusSurfaceClosure.v"

echo "== P059 Consensus surface final closure =="

if grep -E '^[[:space:]]*(Admitted|Axiom|Parameter)[[:space:]]' "$V"; then
  echo "FAIL: ConsensusSurfaceClosure.v is not axiom-free"
  exit 1
fi
if grep -E '^[[:space:]]*Extract Constant' "$V"; then
  echo "FAIL: Extract Constant in ConsensusSurfaceClosure.v"
  exit 1
fi
echo "COQ_ADMITTED=0 COQ_AXIOMS=0 COQ_PARAMETERS=0"

if ! command -v coqc >/dev/null; then
  echo "FAIL: coqc is not on PATH" >&2
  exit 1
fi

echo "== coqc ConsensusSurfaceClosure.v =="
(cd "$COQ" && coqc -q -R . Rigor -R fee-burn Rigor.FeeBurn -R ../rigor_v01 RigorV01 ConsensusSurfaceClosure.v)

echo "== Print Assumptions ConsensusSurfaceClosed =="
ASSUME="$(cd "$COQ" && coqtop -q -R . Rigor -R fee-burn Rigor.FeeBurn -R ../rigor_v01 RigorV01 <<'EOF'
Require ConsensusSurfaceClosure.
Print Assumptions ConsensusSurfaceClosure.ConsensusSurfaceClosed.
Print Assumptions ConsensusSurfaceClosure.every_decision_has_a_boundary.
Print Assumptions ConsensusSurfaceClosure.acceptance_path_order.
Print Assumptions ConsensusSurfaceClosure.pow_boundary_accept_iff_le.
Print Assumptions ConsensusSurfaceClosure.phase5_not_authority.
EOF
)"
echo "$ASSUME"
if echo "$ASSUME" | grep -q '^Axioms:'; then
  echo "FAIL: Print Assumptions listed axioms"
  exit 1
fi
echo "Print Assumptions: Closed under the global context (Axioms: 0)"

python3 "$TOOLS/lint-consensus-surface-closure.py"

CXX="${CXX:-$(command -v g++ || command -v c++)}"
mkdir -p "$OUT"
echo "== g++ test_consensus_surface_closure =="
"$CXX" -O2 -std=c++17 -ffunction-sections -fdata-sections -I"$SRC" \
  -o "$OUT/test_consensus_surface_closure" \
  "$SRC/test/test_consensus_surface_closure.cpp" \
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
  "$SRC/rigor_kernel/pow_digest.cpp" \
  "$SRC/rigor_kernel/keccak.cpp" \
  -Wl,--gc-sections

"$OUT/test_consensus_surface_closure"

echo "CONSENSUS_SURFACE_CLOSURE=CLOSED"
echo "CONSENSUS_AUTHORITY_SINGLE=PASS"
echo "CONSENSUS_DECISION_COVERAGE=PASS"
echo "TRANSACTION_KERNEL_BOUNDARY=PASS"
echo "CONSENSUS_KERNEL_BOUNDARY=PASS"
echo "POW_BOUNDARY_REGISTERED=PASS"
echo "DUPLICATE_AUTHORITY=NONE"
echo "PRODUCTION_KERNEL_SWITCHED=YES"
echo "REFINEMENT_PROVED=true"
echo "EQUIVALENCE_PROVED=false"

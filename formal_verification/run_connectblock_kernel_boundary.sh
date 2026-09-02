#!/usr/bin/env bash
# P057: ConnectBlock ↔ Kernel boundary. Does not switch production.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
COQ="$ROOT/formal_verification/coq"
SRC="$ROOT/core/src"
OUT="$ROOT/formal_verification/extracted"
TOOLS="$ROOT/tools"

echo "== P057 ConnectBlock Kernel boundary =="

for v in ConnectBlockInputPreparation.v KernelResultApplication.v ConnectBlockKernelBoundary.v; do
  if grep -E '^[[:space:]]*(Admitted|Axiom|Parameter)[[:space:]]' "$COQ/$v"; then
    echo "FAIL: $v is not axiom-free"
    exit 1
  fi
  if grep -E '^[[:space:]]*Extract Constant' "$COQ/$v"; then
    echo "FAIL: Extract Constant in $v"
    exit 1
  fi
done
echo "COQ_ADMITTED=0 COQ_AXIOMS=0 COQ_PARAMETERS=0"

if ! command -v coqc >/dev/null; then
  echo "FAIL: coqc is not on PATH" >&2
  exit 1
fi

echo "== coqc boundary modules =="
(cd "$COQ" && \
  coqc -q -R . Rigor -R fee-burn Rigor.FeeBurn -R ../rigor_v01 RigorV01 ConnectBlockInputPreparation.v && \
  coqc -q -R . Rigor -R fee-burn Rigor.FeeBurn -R ../rigor_v01 RigorV01 KernelResultApplication.v && \
  coqc -q -R . Rigor -R fee-burn Rigor.FeeBurn -R ../rigor_v01 RigorV01 ConnectBlockKernelBoundary.v)

echo "== Print Assumptions =="
ASSUME="$(cd "$COQ" && coqtop -q -R . Rigor -R fee-burn Rigor.FeeBurn -R ../rigor_v01 RigorV01 <<'EOF'
Require ConnectBlockKernelBoundary.
Print Assumptions ConnectBlockKernelBoundary.connectblock_kernel_boundary.
Print Assumptions ConnectBlockKernelBoundary.input_preparation_alignment.
Print Assumptions ConnectBlockKernelBoundary.result_consumption_is_apply.
Print Assumptions ConnectBlockKernelBoundary.connectblock_can_reject.
Print Assumptions ConnectBlockKernelBoundary.connectblock_rejects_bad_input.
Print Assumptions ConnectBlockKernelBoundary.connectblock_rejects_winner_mismatch.
EOF
)"
echo "$ASSUME"
if echo "$ASSUME" | grep -q '^Axioms:'; then
  echo "FAIL: Print Assumptions listed axioms"
  exit 1
fi
echo "Print Assumptions: Closed under the global context (Axioms: 0)"

python3 "$TOOLS/lint-connectblock-kernel-boundary.py"

CXX="${CXX:-$(command -v g++ || command -v c++)}"
mkdir -p "$OUT"
echo "== g++ test_connectblock_kernel_boundary =="
"$CXX" -O2 -std=c++17 -ffunction-sections -fdata-sections -I"$SRC" \
  -o "$OUT/test_connectblock_kernel_boundary" \
  "$SRC/test/test_connectblock_kernel_boundary.cpp" \
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

"$OUT/test_connectblock_kernel_boundary"

echo "CONNECTBLOCK_BOUNDARY_REFINEMENT=PASS"
echo "INPUT_PREPARATION_ALIGNMENT=PASS"
echo "RESULT_CONSUMPTION_ALIGNMENT=PASS"
python3 "$TOOLS/refinement_status.py"

#!/usr/bin/env bash
# P058: Transaction Kernel spend-predicate refinement. Does not switch production.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
COQ="$ROOT/formal_verification/coq"
SRC="$ROOT/core/src"
OUT="$ROOT/formal_verification/extracted"
TOOLS="$ROOT/tools"

echo "== P058 Transaction Kernel spend-predicate refinement =="

for v in SignatureBoundary.v TransactionKernelRefinement.v; do
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

echo "== coqc transaction kernel modules =="
(cd "$COQ" && \
  coqc -q -R . Rigor -R fee-burn Rigor.FeeBurn -R ../rigor_v01 RigorV01 SignatureBoundary.v && \
  coqc -q -R . Rigor -R fee-burn Rigor.FeeBurn -R ../rigor_v01 RigorV01 TransactionKernelRefinement.v)

echo "== Print Assumptions TransactionKernelRefinement =="
ASSUME="$(cd "$COQ" && coqtop -q -R . Rigor -R fee-burn Rigor.FeeBurn -R ../rigor_v01 RigorV01 <<'EOF'
Require TransactionKernelRefinement.
Print Assumptions TransactionKernelRefinement.predicate_theorem.
Print Assumptions TransactionKernelRefinement.spend_predicate_refinement.
Print Assumptions TransactionKernelRefinement.sighash_alignment.
Print Assumptions TransactionKernelRefinement.signature_boundary_alignment.
Print Assumptions TransactionKernelRefinement.fail_closed_flag.
EOF
)"
echo "$ASSUME"
if echo "$ASSUME" | grep -q '^Axioms:'; then
  echo "FAIL: Print Assumptions listed axioms"
  exit 1
fi
echo "Print Assumptions: Closed under the global context (Axioms: 0)"

python3 "$TOOLS/lint-transaction-kernel-refinement.py"

CXX="${CXX:-$(command -v g++ || command -v c++)}"
mkdir -p "$OUT"
echo "== g++ test_transaction_kernel_refinement =="
"$CXX" -O2 -std=c++17 -ffunction-sections -fdata-sections -I"$SRC" \
  -o "$OUT/test_transaction_kernel_refinement" \
  "$SRC/test/test_transaction_kernel_refinement.cpp" \
  "$TOOLS/consensus_kernel_refinement_log_stub.cpp" \
  "$SRC/script/rigor_classify.cpp" \
  "$SRC/script/script.cpp" \
  "$SRC/uint256.cpp" \
  "$SRC/util/strencodings.cpp" \
  "$SRC/crypto/sha256.cpp" \
  -Wl,--gc-sections

"$OUT/test_transaction_kernel_refinement"

echo "TRANSACTION_KERNEL_REFINEMENT=CLOSED"
echo "SPEND_PREDICATE_ALIGNMENT=PASS"
echo "SIGHASH_ALIGNMENT=PASS"
echo "SIGNATURE_BOUNDARY_ALIGNMENT=PASS"
echo "FAIL_CLOSED=PASS"
echo "SCRIPT_VM_ABSENT=PASS"
echo "MULTISIG_ABSENT=PASS"
echo "PRODUCTION_KERNEL_SWITCHED=NO"
echo "REFINEMENT_PROVED=true"
echo "EQUIVALENCE_PROVED=false"

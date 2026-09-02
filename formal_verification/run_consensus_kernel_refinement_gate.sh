#!/usr/bin/env bash
# P053: scoped execution refinement gate on finite domain V.
# FAIL => STOP. Does not auto-correct vectors. Not a ∀-proof.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
FV="$ROOT/formal_verification"
SRC="$ROOT/core/src"
OUT="$FV/extracted"
TOOLS="$ROOT/tools"

echo "== P053 consensus kernel refinement execution gate =="
echo "REFINEMENT_PROVED=false EQUIVALENCE_PROVED=false"

for banned in coq_formula_copy.cpp extracted_formula_copy.cpp adapter_formula.cpp; do
  if find "$ROOT" -name "$banned" -not -path '*/.git/*' | grep -q .; then
    echo "FAIL: $banned exists"
    exit 1
  fi
done
echo "DUPLICATE_KERNEL=PASS"

python3 "$TOOLS/lint-kernel-switch-single-source.py"
python3 "$TOOLS/lint-frozen-kernel-params.py"
python3 "$TOOLS/lint-kernel-reject-coverage.py"
python3 "$TOOLS/lint-coq-claim-strength.py"

echo "== 0. ExtractConsensusKernel.v introduces no Admitted / Axiom / Parameter =="
if grep -E '^[[:space:]]*(Admitted|Axiom|Parameter)[[:space:]]' \
     "$FV/coq/ExtractConsensusKernel.v"; then
  echo "FAIL: ExtractConsensusKernel.v is not axiom-free"
  exit 1
fi

echo "== 1. coq / extract build =="
bash "$FV/extract_consensus_kernel.sh"

echo "== 2. C++ production kernel refinement CLI =="
CXX="${CXX:-$(command -v g++ || command -v c++)}"
"$CXX" -O2 -std=c++17 -ffunction-sections -fdata-sections -I"$SRC" \
  -o "$OUT/consensus_kernel_refinement_cli" \
  "$TOOLS/consensus_kernel_refinement_cli.cpp" \
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

echo "== 3. vector comparison (no auto-fix) =="
python3 "$TOOLS/run_consensus_kernel_refinement.py"

echo "== 3b. mutation test (constants / LedgerApply early-return) =="
python3 "$TOOLS/run_kernel_mutation_test.py"

echo "== 4. honesty (P053 finite vectors stay execution-only) =="
python3 -c "
import json
from pathlib import Path
p = Path(r'$FV/consensus_kernel_refinement_vectors.json')
v = json.loads(p.read_text())
assert v.get('refinement_proved') is False
assert v.get('equivalence_proved') is False
print('P053 vector honesty flags remain false')
"

echo "== 5. P056 kernel-boundary Coq refinement =="
bash "$FV/run_consensus_kernel_formal_refinement.sh"

echo "== 6. P057 ConnectBlock Kernel boundary =="
bash "$FV/run_connectblock_kernel_boundary.sh"

echo "== 7. P058 Transaction Kernel spend-predicate refinement =="
bash "$FV/run_transaction_kernel_refinement.sh"

echo "== 8. P059 Consensus surface final closure =="
bash "$FV/run_consensus_surface_closure.sh"

echo "== 9. P060 Consensus Kernel governance =="
SKIP_COQ=1 bash "$FV/run_consensus_kernel_governance.sh"

echo "== 10. P061 Consensus Kernel mainnet readiness =="
bash "$FV/run_consensus_mainnet_readiness.sh"

echo "== 11. P062 audit-scope lock =="
python3 "$TOOLS/lint-audit-scope.py"

echo "== 12. P063 UTXO ledger transition refinement =="
bash "$FV/run_ledger_transition_refinement.sh"

echo "== 13. P065 UTXO state-transition refinement =="
bash "$FV/run_utxo_state_transition_refinement.sh"

echo "CONSENSUS_KERNEL_REFINEMENT_EXECUTION_CLOSED"
echo "P053_VECTOR_REFINEMENT_PROVED=false"
echo "P056_KERNEL_BOUNDARY_REFINEMENT_PROVED=true"
python3 "$ROOT/tools/refinement_status.py"

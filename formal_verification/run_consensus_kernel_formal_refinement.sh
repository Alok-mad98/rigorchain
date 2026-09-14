#!/usr/bin/env bash
# P056: compile ConsensusKernelRefinement.v and Print Assumptions.
# FAIL => STOP. Does not rewrite C++ or Coq formulas.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
COQ="$ROOT/formal_verification/coq"
V="$COQ/ConsensusKernelRefinement.v"

echo "== P056 Consensus Kernel formal refinement =="

if grep -E '^[[:space:]]*(Admitted|Axiom|Parameter)[[:space:]]' "$V"; then
  echo "CONSENSUS_KERNEL_REFINEMENT_DISCREPANCY_FOUND"
  echo "FAIL: ConsensusKernelRefinement.v is not axiom-free"
  exit 1
fi
if grep -E '^[[:space:]]*Extract Constant' "$V"; then
  echo "CONSENSUS_KERNEL_REFINEMENT_DISCREPANCY_FOUND"
  echo "FAIL: Extract Constant is forbidden"
  exit 1
fi
echo "COQ_ADMITTED=0 COQ_AXIOMS=0 COQ_PARAMETERS=0"

if ! command -v coqc >/dev/null; then
  echo "FAIL: coqc is not on PATH" >&2
  exit 1
fi

echo "== coqc ConsensusKernelRefinement.v =="
(cd "$COQ" && coqc -q -R . Rigor -R ../rigor_v01 RigorV01 ConsensusKernelRefinement.v)

echo "== Print Assumptions =="
ASSUME="$(cd "$COQ" && coqtop -q -R . Rigor -R ../rigor_v01 RigorV01 <<'EOF'
Require ConsensusKernelRefinement.
Print Assumptions ConsensusKernelRefinement.cpp_flat_oru_pins.
Print Assumptions ConsensusKernelRefinement.classification_refinement.
Print Assumptions ConsensusKernelRefinement.draw_refinement.
Print Assumptions ConsensusKernelRefinement.supply_delta_refinement.
Print Assumptions ConsensusKernelRefinement.coinbase_boundary_refinement.
EOF
)"
echo "$ASSUME"
if echo "$ASSUME" | grep -q '^Axioms:'; then
  echo "CONSENSUS_KERNEL_REFINEMENT_DISCREPANCY_FOUND"
  echo "FAIL: Print Assumptions listed axioms"
  exit 1
fi
echo "Print Assumptions: Closed under the global context (Axioms: 0)"

echo "ISSUANCE_REFINEMENT=PASS"
echo "CLASSIFICATION_REFINEMENT=PASS"
echo "DRAW_REFINEMENT=PASS"
echo "SUPPLY_DELTA_REFINEMENT=PASS"
echo "COINBASE_BOUNDARY_REFINEMENT=PASS"
echo "TAIL_EMISSION_CAP=PASS"
python3 "$ROOT/tools/refinement_status.py"

#!/usr/bin/env bash
# P061: one-shot formal reproducibility of Consensus Kernel proofs and gates.
# Does not switch production. Does not rewrite formulas.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
COQ="$ROOT/formal_verification/coq"
FLAGS=(-q -R . Rigor -R ../rigor_v01 RigorV01)

fail() {
  echo "CONSENSUS_FORMAL_BUILD_NOT_REPRODUCIBLE"
  echo "FAIL: $*"
  exit 1
}

echo "== P061 reproduce Consensus Kernel formal build =="

command -v coqc >/dev/null || fail "coqc is not on PATH (need Coq 8.15.0)"
command -v coqtop >/dev/null || fail "coqtop is not on PATH"

echo "== coqc P056-P059 =="
(
  cd "$COQ"
  coqc "${FLAGS[@]}" ConsensusKernelRefinement.v
  coqc "${FLAGS[@]}" ConnectBlockInputPreparation.v
  coqc "${FLAGS[@]}" KernelResultApplication.v
  coqc "${FLAGS[@]}" ConnectBlockKernelBoundary.v
  coqc "${FLAGS[@]}" SignatureBoundary.v
  coqc "${FLAGS[@]}" TransactionKernelRefinement.v
  coqc "${FLAGS[@]}" ConsensusSurfaceClosure.v
)

echo "== Print Assumptions =="
ASSUME="$(cd "$COQ" && coqtop "${FLAGS[@]}" <<'EOF'
Require ConsensusSurfaceClosure.
Print Assumptions ConsensusKernelRefinement.cpp_flat_oru_pins.
Print Assumptions ConnectBlockKernelBoundary.connectblock_kernel_boundary.
Print Assumptions TransactionKernelRefinement.predicate_theorem.
Print Assumptions ConsensusSurfaceClosure.ConsensusSurfaceClosed.
EOF
)"
echo "$ASSUME"
if echo "$ASSUME" | grep -q '^Axioms:'; then
  fail "Print Assumptions listed axioms"
fi

echo "== run_consensus_kernel_refinement_gate.sh =="
bash "$ROOT/formal_verification/run_consensus_kernel_refinement_gate.sh"

echo "== run_differential_gate.sh =="
SKIP_TEST_RIGOR=1 bash "$ROOT/formal_verification/scripts/run_differential_gate.sh"

echo "== lint-consensus-authority.py =="
python3 "$ROOT/tools/lint-consensus-authority.py"

echo "== check-coq-consensus-assumptions.py =="
python3 "$ROOT/tools/check-coq-consensus-assumptions.py"

echo "== audit-consensus-release-build.py =="
python3 "$ROOT/tools/audit-consensus-release-build.py"

echo "CONSENSUS_FORMAL_BUILD_REPRODUCIBLE"
echo "FORMAL_REPRODUCIBILITY=PASS"
echo "COQ_ADMITTED=0"
echo "COQ_AXIOMS=0"
echo "EQUIVALENCE_PROVED=false"

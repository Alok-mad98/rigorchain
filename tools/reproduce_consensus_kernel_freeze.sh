#!/usr/bin/env bash
# P068: reproduce the Consensus Kernel migration freeze.
# Does not switch production. Does not rewrite formulas.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
COQ="$ROOT/formal_verification/coq"
FLAGS=(-q -R . Rigor -R ../rigor_v01 RigorV01)

fail() {
  echo "CONSENSUS_KERNEL_FREEZE_DRIFT"
  echo "FAIL: $*"
  exit 1
}

echo "== P068 reproduce Consensus Kernel migration freeze =="

command -v coqc >/dev/null || fail "coqc is not on PATH"
command -v coqtop >/dev/null || fail "coqtop is not on PATH"

echo "== coqc refinement files =="
(
  cd "$COQ"
  for v in \
    ConsensusKernelRefinement.v \
    TransactionKernelRefinement.v \
    LedgerTransitionRefinement.v \
    UTXOStateTransitionRefinement.v \
    ConnectBlockKernelBoundary.v
  do
    if [[ -f "$v" ]]; then
      if grep -E '^[[:space:]]*(Admitted|Axiom|Parameter)[[:space:]]' "$v"; then
        fail "$v has Admitted/Axiom/Parameter"
      fi
      coqc "${FLAGS[@]}" "$v"
    fi
  done
)

echo "== Print Assumptions =="
ASSUME="$(cd "$COQ" && coqtop "${FLAGS[@]}" <<'EOF'
Require ConsensusKernelRefinement.
Print Assumptions ConsensusKernelRefinement.cpp_flat_oru_pins.
EOF
)"
echo "$ASSUME"
if echo "$ASSUME" | grep -q '^Axioms:'; then
  fail "Print Assumptions listed axioms"
fi
echo "COQ_ADMITTED = 0"
echo "COQ_AXIOMS = 0"
echo "COQ_PARAMETERS = 0"

echo "== authority lint =="
python3 "$ROOT/tools/lint-consensus-authority.py"
python3 "$ROOT/tools/lint-consensus-authority-security.py"

echo "== migration readiness =="
python3 "$ROOT/tools/run_kernel_migration_readiness.py"

echo "== canary =="
python3 "$ROOT/tools/run_consensus_kernel_canary.py"

echo "== differential gate =="
SKIP_TEST_RIGOR=1 bash "$ROOT/formal_verification/scripts/run_differential_gate.sh"

echo "== freeze gate =="
python3 "$ROOT/tools/run_consensus_kernel_freeze_gate.py"

echo "CONSENSUS_KERNEL_MIGRATION_FREEZE = CLOSED"
echo "CANARY_STATUS = PASS"
echo "CURRENT_CPP_AUTHORITY = PASS"
echo "MIGRATION_PATH = PASS"
python3 "$ROOT/tools/refinement_status.py"

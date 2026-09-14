#!/usr/bin/env bash
# P062: third-party reproduction of the Consensus Kernel audit package.
# Does not switch production. Does not rewrite formulas.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
COQ="$ROOT/formal_verification/coq"
FLAGS=(-q -R . Rigor -R ../rigor_v01 RigorV01)

fail() {
  echo "RIGOR_CONSENSUS_AUDIT_PACKAGE_NOT_REPRODUCIBLE"
  echo "FAIL: $*"
  exit 1
}

echo "== P062 reproduce Consensus Kernel audit package =="

command -v coqc >/dev/null || fail "coqc is not on PATH (need Coq 8.15.0)"
command -v coqtop >/dev/null || fail "coqtop is not on PATH"

python3 "$ROOT/tools/lint-audit-scope.py"
python3 "$ROOT/tools/lint-consensus-authority.py"
python3 "$ROOT/tools/audit-consensus-release-build.py"

echo "== coqc formal_verification/coq/*.v (skip Extract* / Extraction / Phase5 re-export) =="
(
  cd "$COQ"
  shopt -s nullglob
  for v in *.v; do
    case "$v" in
      Extract*.v|Extraction.v|DrawBridgeProgress.v)
        echo "skip $v (extraction driver or non-authoritative re-export)"
        continue
        ;;
    esac
    echo "coqc $v"
    coqc "${FLAGS[@]}" "$v"
  done
)

echo "== Print Assumptions =="
ASSUME="$(cd "$COQ" && coqtop "${FLAGS[@]}" <<'EOF'
Require ConsensusSurfaceClosure.
Print Assumptions ConsensusKernelRefinement.cpp_flat_oru_pins.
Print Assumptions ConsensusKernelRefinement.supply_delta_refinement.
Print Assumptions ConsensusKernelRefinement.draw_refinement.
Print Assumptions ConsensusKernelRefinement.classification_refinement.
Print Assumptions TransactionKernelRefinement.predicate_theorem.
Print Assumptions ConnectBlockKernelBoundary.connectblock_kernel_boundary.
Print Assumptions ConsensusSurfaceClosure.ConsensusSurfaceClosed.
EOF
)"
echo "$ASSUME"
if echo "$ASSUME" | grep -q '^Axioms:'; then
  fail "Print Assumptions listed axioms"
fi

python3 "$ROOT/tools/check-coq-consensus-assumptions.py"
bash "$ROOT/formal_verification/run_consensus_kernel_refinement_gate.sh"
python3 "$ROOT/tools/run_consensus_kernel_shadow.py"
SKIP_TEST_RIGOR=1 bash "$ROOT/formal_verification/scripts/run_differential_gate.sh"
bash "$ROOT/formal_verification/run_consensus_mainnet_readiness.sh"

echo "RIGOR_CONSENSUS_AUDIT_PACKAGE_REPRODUCIBLE"
echo "AUDIT_PACKAGE_STATUS=PASS"
python3 "$ROOT/tools/refinement_status.py"

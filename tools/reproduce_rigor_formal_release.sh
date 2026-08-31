#!/usr/bin/env bash
# P074: reproduce the formal consensus genesis-release freeze.
# Does not rewrite formulas. Does not modify production consensus.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
MANIFEST="$ROOT/formal_verification/rigor_formal_final_manifest.json"

fail() {
  echo "FAIL: $*"
  exit 1
}

echo "== P074 reproduce Rigor formal consensus release freeze =="

[[ -f "$MANIFEST" ]] || fail "missing formal_verification/rigor_formal_final_manifest.json"

python3 - "$MANIFEST" <<'PY'
import json, sys
from pathlib import Path
p = Path(sys.argv[1])
m = json.loads(p.read_text(encoding="utf-8"))
errors = []
if m.get("COQ_ADMITTED") != 0: errors.append("COQ_ADMITTED")
if m.get("COQ_AXIOMS") != 0: errors.append("COQ_AXIOMS")
if m.get("COQ_PARAMETERS") != 0: errors.append("COQ_PARAMETERS")
if m.get("REFINEMENT_PROVED") is not True: errors.append("REFINEMENT_PROVED")
if m.get("EQUIVALENCE_PROVED") is not False: errors.append("EQUIVALENCE_PROVED")
auth = m.get("authority_map") or {}
expect = {
    "issuance": "GetBlockSubsidy",
    "res": "GetResMintReward",
    "classification": "ClassifyRigorTxout",
    "draw": "ResolveConvergenceDraw",
    "supply": "RigorSupplyDelta",
    "spend": "VerifyRigorPredicate",
    "ledger": "LedgerApply",
    "kernel": "RigorConsensusKernel",
}
for k, v in expect.items():
    if auth.get(k) != v:
        errors.append(f"authority_map.{k}")
if errors:
    print("FAIL: manifest", ", ".join(errors))
    sys.exit(1)
print("MANIFEST_FLAGS=PASS")
PY

command -v coqc >/dev/null || fail "coqc is not on PATH"
command -v coqtop >/dev/null || fail "coqtop is not on PATH"

echo "== coqc proofs + Print Assumptions =="
python3 "$ROOT/tools/check-coq-consensus-assumptions.py"

echo "== refinement gates =="
bash "$ROOT/formal_verification/run_consensus_kernel_formal_refinement.sh"
bash "$ROOT/formal_verification/run_transaction_kernel_refinement.sh"
bash "$ROOT/formal_verification/run_connectblock_kernel_boundary.sh"
bash "$ROOT/formal_verification/run_ledger_transition_refinement.sh"

echo "== authority lint =="
python3 "$ROOT/tools/lint-consensus-authority.py"
python3 "$ROOT/tools/lint-consensus-authority-security.py"
python3 "$ROOT/tools/lint-genesis-consensus-authority.py"

echo "== differential tests =="
SKIP_TEST_RIGOR=1 bash "$ROOT/formal_verification/scripts/run_differential_gate.sh"

echo "== UTXO transition tests =="
bash "$ROOT/formal_verification/run_utxo_state_transition_refinement.sh"

echo "COQ_ADMITTED=0"
echo "COQ_AXIOMS=0"
echo "COQ_PARAMETERS=0"
echo "REFINEMENT_PROVED=TRUE"
echo "EQUIVALENCE_PROVED=FALSE"
echo "AUTHORITY_LINT=PASS"
echo "REPRODUCIBILITY=PASS"
echo "FORMAL_CONSENSUS_FINAL_FREEZE=CLOSED"

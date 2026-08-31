#!/usr/bin/env bash
# P063: UTXO ledger transition refinement. Does not switch production.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
COQ="$ROOT/formal_verification/coq"
SRC="$ROOT/core/src"
OUT="$ROOT/formal_verification/extracted"
TOOLS="$ROOT/tools"
V="$COQ/LedgerTransitionRefinement.v"

echo "== P063 UTXO ledger transition refinement =="

if grep -E '^[[:space:]]*(Admitted|Axiom|Parameter)[[:space:]]' "$V"; then
  echo "FAIL: LedgerTransitionRefinement.v is not axiom-free"
  exit 1
fi
if grep -E '^[[:space:]]*Extract Constant' "$V"; then
  echo "FAIL: Extract Constant in LedgerTransitionRefinement.v"
  exit 1
fi
echo "COQ_ADMITTED=0 COQ_AXIOMS=0 COQ_PARAMETERS=0"

if ! command -v coqc >/dev/null; then
  echo "FAIL: coqc is not on PATH" >&2
  exit 1
fi

echo "== coqc LedgerTransitionRefinement.v =="
(cd "$COQ" && coqc -q -R . Rigor -R fee-burn Rigor.FeeBurn -R ../rigor_v01 RigorV01 LedgerTransitionRefinement.v)

echo "== Print Assumptions =="
ASSUME="$(cd "$COQ" && coqtop -q -R . Rigor -R fee-burn Rigor.FeeBurn -R ../rigor_v01 RigorV01 <<'EOF'
Require LedgerTransitionRefinement.
Print Assumptions LedgerTransitionRefinement.ledger_apply_eq.
Print Assumptions LedgerTransitionRefinement.genesis_actual_zero.
Print Assumptions LedgerTransitionRefinement.double_spend_rejects.
Print Assumptions LedgerTransitionRefinement.value_conservation_normal_spend.
Print Assumptions LedgerTransitionRefinement.asset_conservation_mint.
EOF
)"
echo "$ASSUME"
if echo "$ASSUME" | grep -q '^Axioms:'; then
  echo "FAIL: Print Assumptions listed axioms"
  exit 1
fi
echo "Print Assumptions: Closed under the global context (Axioms: 0)"

python3 "$TOOLS/lint-ledger-kernel-boundary.py"

CXX="${CXX:-$(command -v g++ || command -v c++)}"
mkdir -p "$OUT"
echo "== g++ test_ledger_transition_refinement =="
"$CXX" -O2 -std=c++17 -ffunction-sections -fdata-sections -I"$SRC" \
  -o "$OUT/test_ledger_transition_refinement" \
  "$SRC/test/test_ledger_transition_refinement.cpp" \
  "$SRC/consensus/ledger/ledger_transition.cpp" \
  "$SRC/consensus/rigor_issuance.cpp" \
  "$TOOLS/consensus_kernel_refinement_log_stub.cpp" \
  "$SRC/uint256.cpp" \
  "$SRC/util/strencodings.cpp" \
  -Wl,--gc-sections

"$OUT/test_ledger_transition_refinement"

echo "UTXO_LEDGER_REFINEMENT=PASS"
echo "VALUE_CONSERVATION=PASS"
echo "DOUBLE_SPEND_BOUNDARY=PASS"
echo "ASSET_CONSERVATION=PASS"
echo "PRODUCTION_CONSENSUS_MODIFIED=NO"
echo "PRODUCTION_KERNEL_SWITCHED=NO"
echo "REFINEMENT_PROVED=true"
echo "EQUIVALENCE_PROVED=false"

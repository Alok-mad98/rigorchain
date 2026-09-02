#!/usr/bin/env bash
# P065: UTXO state-transition composition refinement. Does not switch production.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
COQ="$ROOT/formal_verification/coq"
SRC="$ROOT/core/src"
OUT="$ROOT/formal_verification/extracted"
TOOLS="$ROOT/tools"
V="$COQ/UTXOStateTransitionRefinement.v"

echo "== P065 UTXO state-transition refinement =="

if grep -E '^[[:space:]]*(Admitted|Axiom|Parameter)[[:space:]]' "$V"; then
  echo "FAIL: UTXOStateTransitionRefinement.v is not axiom-free"
  exit 1
fi
if grep -E '^[[:space:]]*Extract Constant' "$V"; then
  echo "FAIL: Extract Constant in UTXOStateTransitionRefinement.v"
  exit 1
fi
echo "COQ_ADMITTED=0 COQ_AXIOMS=0 COQ_PARAMETERS=0"

if ! command -v coqc >/dev/null; then
  echo "FAIL: coqc is not on PATH" >&2
  exit 1
fi

echo "== coqc UTXOStateTransitionRefinement.v =="
(cd "$COQ" && coqc -q -R . Rigor -R fee-burn Rigor.FeeBurn -R ../rigor_v01 RigorV01 UTXOStateTransitionRefinement.v)

echo "== Print Assumptions =="
ASSUME="$(cd "$COQ" && coqtop -q -R . Rigor -R fee-burn Rigor.FeeBurn -R ../rigor_v01 RigorV01 <<'EOF'
Require UTXOStateTransitionRefinement.
Print Assumptions UTXOStateTransitionRefinement.state_transition_refinement.
Print Assumptions UTXOStateTransitionRefinement.cpp_rejects_unscheduled_res_mint.
Print Assumptions UTXOStateTransitionRefinement.transaction_gate.
Print Assumptions UTXOStateTransitionRefinement.consensus_gate.
Print Assumptions UTXOStateTransitionRefinement.ledger_transition.
EOF
)"
echo "$ASSUME"
if echo "$ASSUME" | grep -q '^Axioms:'; then
  echo "FAIL: Print Assumptions listed axioms"
  exit 1
fi
echo "Print Assumptions: Closed under the global context (Axioms: 0)"

python3 "$TOOLS/lint-utxo-state-transition-boundary.py"

CXX="${CXX:-$(command -v g++ || command -v c++)}"
mkdir -p "$OUT"
echo "== g++ test_utxo_state_transition_refinement =="
"$CXX" -O2 -std=c++17 -ffunction-sections -fdata-sections -I"$SRC" \
  -o "$OUT/test_utxo_state_transition_refinement" \
  "$SRC/test/test_utxo_state_transition_refinement.cpp" \
  "$SRC/consensus/ledger/ledger_transition.cpp" \
  "$SRC/consensus/rigor_issuance.cpp" \
  "$TOOLS/consensus_kernel_refinement_log_stub.cpp" \
  "$SRC/script/rigor_classify.cpp" \
  "$SRC/script/script.cpp" \
  "$SRC/uint256.cpp" \
  "$SRC/util/strencodings.cpp" \
  "$SRC/consensus/convergence_logic.cpp" \
  "$SRC/consensus_rigor/sigma_hash.cpp" \
  "$SRC/crypto/sha256.cpp" \
  "$SRC/arith_uint256.cpp" \
  -Wl,--gc-sections

"$OUT/test_utxo_state_transition_refinement"

echo "UTXO_STATE_TRANSITION_REFINEMENT=PASS/CLOSED"
echo "PRODUCTION_CONSENSUS_MODIFIED=NO"
python3 "$TOOLS/refinement_status.py"

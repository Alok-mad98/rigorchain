#!/usr/bin/env bash
# P0-086: RIGOR ConnectBlock / consensus-convergence security audit gate.
# Does not modify Consensus Kernel, Transaction Kernel, Ledger, genesis,
# issuance, RES, draw, PoW, ASERT, consensus parameters, or Coq proofs.
#
# Honest gate: ReorgBind / live-mint goldens, contextual non-reinvoke,
# RollforwardBlock, and inherited DB fail-closed keep the audit OPEN.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/core/src"
OUT="$ROOT/formal_verification/extracted"
TOOLS="$ROOT/tools"
CXX="${CXX:-$(command -v g++ || command -v c++)}"

echo "== P086 ConnectBlock consensus convergence audit =="

echo "== authority / ordering lint =="
python3 "$TOOLS/lint-connectblock-convergence-audit.py"

echo "== kernel-boundary lint (existing P057) =="
python3 "$TOOLS/lint-connectblock-kernel-boundary.py"

echo "== source-level gate mutation =="
python3 "$TOOLS/run_connectblock_convergence_mutation.py"

echo "== catalog / hex lock =="
python3 "$ROOT/formal_verification/tests/test_p0_016_vector_lock.py"

echo "== standalone audit harness =="
mkdir -p "$OUT"
"$CXX" -O2 -std=c++17 -ffunction-sections -fdata-sections \
  -DCONNECTBLOCK_CONVERGENCE_AUDIT_STANDALONE \
  -I"$SRC" \
  -o "$OUT/test_connectblock_convergence_audit" \
  "$SRC/test/test_connectblock_convergence_audit.cpp" \
  -Wl,--gc-sections
"$OUT/test_connectblock_convergence_audit"

echo "== fuzz smoke =="
bash "$ROOT/fuzz/connectblock_convergence/run_smoke.sh"

echo
echo "== P086 verdict (audit-first; not a formal proof) =="
python3 - "$ROOT" <<'PY'
from pathlib import Path
import json
import sys
root = Path(sys.argv[1])
man = json.loads((root / "formal_verification/connectblock_convergence_audit_manifest.json").read_text(encoding="utf-8"))
cat = json.loads((root / "formal_verification/vectors/rigor_connectblock_convergence_catalog.json").read_text(encoding="utf-8"))
assert man["connectblock_consensus_convergence_audit"] == "OPEN"
assert cat["live_mint_golden_vector"] == "NOT_COVERED"
assert cat["reorgbind_golden_vector"] == "NOT_COVERED"
print(f"CONNECTBLOCK_AUDIT_STATUS={man['connectblock_consensus_convergence_audit']}")
print(f"CONNECTBLOCK_ACCEPTANCE_AUTHORITY_COUNT={man['connectblock_acceptance_authority_count']}")
print(f"CONSENSUS_BYPASS_COUNT={man['consensus_bypass_count']}")
print(f"DUPLICATE_AUTHORITY_COUNT={man['duplicate_authority_count']}")
print(f"STATE_MUTATION_BEFORE_ACCEPTANCE={man['state_mutation_before_acceptance']}")
print(f"CONNECT_DISCONNECT_SYMMETRY={man['connect_disconnect_symmetry']}")
print(f"REORG_COVERAGE={man['reorg_coverage']}")
print(f"DATABASE_BOUNDARY_STATUS={man['database_boundary_status']}")
print(f"FORMAL_COVERAGE={man['formal_coverage']}")
print(f"FUZZ_STATUS={man['fuzz_status']}")
print(f"MUTATION_STATUS={man['mutation_status']}")
print(f"GOLDEN_VECTOR_STATUS={man['golden_vector_status']}")
print(f"RELEASE_GUARDIAN_COVERAGE={man['release_guardian_coverage']}")
print(f"CONSENSUS_CODE_CHANGED={str(man['consensus_code_changed']).upper()}")
print(f"PROTOCOL_PARAMETERS_CHANGED={str(man['protocol_parameters_changed']).upper()}")
print(f"FORMAL_THEOREMS_CHANGED={str(man['formal_theorems_changed']).upper()}")
print("LIVE_MINT_GOLDEN_VECTOR=NOT_COVERED")
print("REORGBIND_GOLDEN_VECTOR=NOT_COVERED")
print("CONNECTBLOCK_CONSENSUS_CONVERGENCE_AUDIT=OPEN")
PY

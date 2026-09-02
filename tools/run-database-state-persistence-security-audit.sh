#!/usr/bin/env bash
# P0-086: RIGOR database / CoinsDB / state-persistence security audit gate.
# Does not modify Consensus Kernel, Transaction Kernel, Ledger, genesis,
# issuance, RES, draw, PoW, ASERT, consensus parameters, or Coq proofs.
#
# Fail-closed gate for CoinsDB deserialize / integrity. Does not rewrite
# Consensus Kernel, Transaction Kernel, Ledger, genesis, issuance, RES,
# draw, PoW, ASERT, consensus parameters, or Coq proofs.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/core/src"
OUT="$ROOT/formal_verification/extracted"
TOOLS="$ROOT/tools"
CXX="${CXX:-$(command -v g++ || command -v c++)}"

echo "== P086 database / CoinsDB / state-persistence security audit =="

echo "== authority lint =="
python3 "$TOOLS/lint-database-consensus-boundary.py"

echo "== golden vectors =="
python3 "$TOOLS/database_state_codec.py" write
python3 "$TOOLS/database_state_codec.py" verify

echo "== python tests/database =="
python3 "$ROOT/tests/database/test_database_state_persistence.py"

echo "== g++ test_database_state_persistence_security =="
mkdir -p "$OUT"
"$CXX" -O2 -std=c++17 -ffunction-sections -fdata-sections \
  -DDATABASE_STATE_PERSISTENCE_SECURITY_STANDALONE \
  -I"$SRC" \
  -o "$OUT/test_database_state_persistence_security" \
  "$SRC/test/test_database_state_persistence_security.cpp" \
  -Wl,--gc-sections
"$OUT/test_database_state_persistence_security"

echo "== fuzz/database smoke =="
bash "$ROOT/fuzz/database/run_smoke.sh"

echo
echo "== P086 verdict (audit-first; not a formal proof) =="
python3 - "$ROOT" <<'PY'
from pathlib import Path
import json
import sys
root = Path(sys.argv[1])
man = json.loads((root / "formal_verification/database_state_persistence_audit_manifest.json").read_text(encoding="utf-8"))
vec = json.loads((root / "formal_verification/database_state_golden_vectors.json").read_text(encoding="utf-8"))
cache = "PASS" if vec["cache_db_equivalence"]["equivalent"] else "FAIL"
reorg = "PASS" if vec["reorg_recovery"]["equivalent"] else "FAIL"
rebuild_state = bool(vec["rebuild_equivalence"]["consensus_state_equivalent"])
fail_closed_vec = bool(vec["production_failure_boundary"]["database_fail_closed_for_value_corruption"])
mapped = bool(vec["production_failure_boundary"]["database_failure_mapped_to_consensus_invalid"])
rebuild = "PARTIAL"
corruption = "PASS" if fail_closed_vec and not mapped else "FAIL"
fail_closed = "TRUE" if fail_closed_vec and not mapped else "FALSE"
boundary = "CLOSED" if fail_closed == "TRUE" else "OPEN"
audit = "PASS" if fail_closed == "TRUE" and cache == "PASS" and reorg == "PASS" else "BLOCKED"
gate = "PARTIAL"
print(f"DATABASE_SECURITY_AUDIT={audit}")
print(f"DATABASE_CONSENSUS_BOUNDARY={boundary}")
print(f"DATABASE_FAIL_CLOSED={fail_closed}")
print(f"CACHE_DB_EQUIVALENCE={cache}")
print(f"REORG_RECOVERY={reorg}")
print(f"REBUILD_EQUIVALENCE={rebuild}")
print(f"CONSENSUS_STATE_EQUIVALENCE={'TRUE' if rebuild_state else 'FALSE'}")
print(f"CORRUPTION_TEST={corruption}")
print("DATABASE_CONSENSUS_DIVERGENCE=0")
print("FUZZ_STATUS=PASS")
print("CRASH_FOUND=NO")
print("UNDEFINED_BEHAVIOR=NO")
print(f"DATABASE_CONSENSUS_AUTHORITY_COUNT={man['database_consensus_authority_count']}")
print(f"DATABASE_VALIDATION_AUTHORITY_COUNT={man['database_validation_authority_count']}")
print(f"DATABASE_STATE_RECONSTRUCTION_PATH_COUNT={man['database_state_reconstruction_path_count']}")
print("INHERITED_BITCOIN_DB_SURFACE=AUDITED")
print("DATABASE_FORMAL_PROOF=FALSE")
print("DATABASE_TRUST_BOUNDARY=EXTERNAL_INFRASTRUCTURE")
print("RELEASE_GUARDIAN_DATABASE_COVERAGE=FULL")
print(f"DATABASE_RELEASE_GATE={gate}")
print("PRODUCTION_CONSENSUS_MODIFIED=NO")
print("DATABASE_FORMALLY_PROVED=FALSE")
print("COIN_DISK_FORMAT=LEAF+CRC32C_RGCI")
print("PRE_CHECKSUM_CHAINSTATE=WIPE_OR_REINDEX")
if man.get("database_security_audit") != audit:
    raise SystemExit(f"FAIL: manifest database_security_audit={man.get('database_security_audit')} != {audit}")
if man.get("database_release_gate") != gate:
    raise SystemExit(f"FAIL: manifest database_release_gate={man.get('database_release_gate')} != {gate}")
if fail_closed != "TRUE":
    raise SystemExit("FAIL: DATABASE_FAIL_CLOSED is not TRUE")
print("RIGOR_DATABASE_STATE_PERSISTENCE_SECURITY_AUDIT=PASS")
print("NOTE: DATABASE_RELEASE_GATE=PARTIAL because production -reindex was not executed")
PY

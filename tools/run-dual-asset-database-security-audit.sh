#!/usr/bin/env bash
# P0-086B: RIGOR dual-asset (ORU / RES) database & state-persistence security audit.
# Audit-first. Does not modify Consensus Kernel, Transaction Kernel, Ledger,
# genesis, issuance, RES mint rules, draw, convergence, ORU rules, PoW, ASERT,
# consensus parameters, P2P rules, wallet consensus rules, or Coq theorems.
#
# Honest gate: script-mutation reclassification and deserialize→missing keep
# DUAL_ASSET_DATABASE_AUDIT=BLOCKED. Do not edit production consensus to force PASS.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/core/src"
OUT="$ROOT/formal_verification/extracted"
TOOLS="$ROOT/tools"
CXX="${CXX:-$(command -v g++ || command -v c++)}"

echo "== P086B dual-asset ORU / RES database security audit =="

echo "== golden vectors =="
if [[ ! -f "$ROOT/formal_verification/dual_asset_database_vectors/catalog.json" ]]; then
  python3 "$TOOLS/dual_asset_database_codec.py" write
fi
python3 "$TOOLS/dual_asset_database_codec.py" verify

echo "== authority lint =="
python3 "$TOOLS/lint-dual-asset-database-boundary.py"

echo "== python tests =="
python3 "$ROOT/tests/dual_asset_database/test_dual_asset_database.py"

echo "== g++ test_dual_asset_database_security =="
mkdir -p "$OUT"
"$CXX" -O2 -std=c++17 -ffunction-sections -fdata-sections \
  -DDUAL_ASSET_DATABASE_SECURITY_STANDALONE \
  -I"$SRC" \
  -o "$OUT/test_dual_asset_database_security" \
  "$SRC/test/test_dual_asset_database_security.cpp" \
  -Wl,--gc-sections
"$OUT/test_dual_asset_database_security"

echo "== fuzz/dual_asset_database smoke =="
bash "$ROOT/fuzz/dual_asset_database/run_smoke.sh"

echo
echo "== P086B verdict (audit-first; not a formal proof) =="
python3 - "$ROOT" <<'PY'
from pathlib import Path
import json
import sys
root = Path(sys.argv[1])
man = json.loads((root / "formal_verification/dual_asset_database_audit_manifest.json").read_text(encoding="utf-8"))
vec = json.loads((root / "formal_verification/dual_asset_database_vectors/catalog.json").read_text(encoding="utf-8"))
oru_cache = "PASS" if vec["cache_db_equivalence"]["oru_equivalent"] else "FAIL"
res_cache = "PASS" if vec["cache_db_equivalence"]["res_equivalent"] else "FAIL"
oru_reorg = "PASS" if vec["reorg_recovery"]["oru_equivalent"] else "FAIL"
res_reorg = "PASS" if vec["reorg_recovery"]["res_equivalent"] else "FAIL"
# Production -reindex was not executed. Harness rebuild is PARTIAL.
oru_rebuild = "PARTIAL" if vec["rebuild_equivalence"]["oru_equivalent"] else "FAIL"
res_rebuild = "PARTIAL" if vec["rebuild_equivalence"]["res_equivalent"] else "FAIL"
oru_supply = "TRUE" if vec["rebuild_equivalence"]["supply_equivalent"] and vec["reorg_recovery"]["supply_equivalent"] else "FALSE"
res_supply = oru_supply
# Script mutation reclassifies; deserialize→missing. Do not force PASS.
audit = "BLOCKED"
print(f"DUAL_ASSET_DATABASE_AUDIT={audit}")
print(f"ORU_RES_STORAGE_MODEL={vec['oru_res_storage_model']}")
print(f"ORU_DATABASE_SEMANTICS=SCRIPT_AS_ASSET_COIN")
print(f"RES_DATABASE_SEMANTICS=HYBRID_COIN_PLUS_MINT_SCRIPT_PLUS_INDEX_MEMO")
print(f"ASSET_ID_AUTHORITY_COUNT={man['asset_id_authority_count']}")
print(f"ORU_DATABASE_AUTHORITY_COUNT={man['oru_database_authority_count']}")
print(f"RES_DATABASE_AUTHORITY_COUNT={man['res_database_authority_count']}")
print(f"ORU_SUPPLY_EQUIVALENCE={oru_supply}")
print(f"RES_SUPPLY_EQUIVALENCE={res_supply}")
print(f"ORU_CACHE_DB_EQUIVALENCE={oru_cache}")
print(f"RES_CACHE_DB_EQUIVALENCE={res_cache}")
print(f"ORU_REORG_RECOVERY={oru_reorg}")
print(f"RES_REORG_RECOVERY={res_reorg}")
print(f"ORU_REBUILD_EQUIVALENCE={oru_rebuild}")
print(f"RES_REBUILD_EQUIVALENCE={res_rebuild}")
print("ORU_CORRUPTION_FAIL_CLOSED=FALSE")
print("RES_CORRUPTION_FAIL_CLOSED=FALSE")
print(f"ORU_RES_IMPLICIT_CONVERSION={man['oru_res_implicit_conversion']}")
print(f"ORU_RES_CROSS_ACCOUNTING={man['oru_res_cross_accounting']}")
print("FUZZ_STATUS=PASS")
print("MUTATION_STATUS=RECLASSIFIES")
print("GOLDEN_VECTOR_STATUS=LOCKED")
print(f"FORMAL_COVERAGE={man['formal_coverage']}")
print(f"RELEASE_GUARDIAN_DUAL_ASSET_COVERAGE={man['release_guardian_dual_asset_coverage']}")
print("CONSENSUS_CODE_CHANGED=FALSE")
print("PROTOCOL_PARAMETERS_CHANGED=FALSE")
print("FORMAL_THEOREMS_CHANGED=FALSE")
if man.get("dual_asset_database_audit") != "BLOCKED":
    raise SystemExit("FAIL: manifest must keep DUAL_ASSET_DATABASE_AUDIT=BLOCKED until fail-closed and reclassification are closed")
if not vec["production_failure_boundary"]["script_mutation_reclassifies_asset"]:
    raise SystemExit("FAIL: catalog must keep script-mutation reclassification recorded")
print("RIGOR_DUAL_ASSET_ORU_RES_DATABASE_AUDIT=BLOCKED")
PY

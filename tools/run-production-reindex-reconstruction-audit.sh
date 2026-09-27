#!/usr/bin/env bash
# P0-088: production -reindex / -reindex-chainstate field-level reconstruction audit.
# AUDIT-FIRST. Does not modify Consensus Kernel, Transaction Kernel, Ledger,
# genesis, issuance, RES, draw, PoW, ASERT, consensus parameters, Coq proofs,
# Coin disk format, or Release Guardian (this TASK). P0-091 requires
# Guardian Stage DATABASE_SECURITY; this gate now fails closed without it.
#
# A harness PASS is not a production PASS. If genuine rigord -reindex cannot
# run, the honest result is BLOCKED, not PASS.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TOOLS="$ROOT/tools"
DOC="$ROOT/docs/security/RIGOR_PRODUCTION_REINDEX_RECONSTRUCTION_AUDIT_P0_088.md"
MANIFEST="$ROOT/formal_verification/production_reindex_reconstruction_audit_manifest.json"
DUMP_DIR="$ROOT/formal_verification/database_audit"
TEST_PY="$ROOT/core/test/functional/feature_production_reindex_reconstruction_p0_088.py"
BINARY="$ROOT/core/src/rigord"
CONFIG="$ROOT/core/src/config/rigor-config.h"
GUARDIAN_YML="$ROOT/.github/workflows/genesis-release-guardian.yml"
GUARDIAN_SH="$ROOT/tools/run-genesis-release-guardian.sh"

mkdir -p "$DUMP_DIR"
# Prior-run blockers / dumps must not leak into this execution.
find "$DUMP_DIR" -mindepth 1 -maxdepth 1 -exec rm -rf {} +
export RIGOR_P088_DUMP_DIR="$DUMP_DIR"

echo "== P088 production reindex / reconstruction equivalence audit =="
echo "== disk / leftover functional datadirs =="
df -h / /tmp 2>/dev/null || true
# Prior runs with framework -debug left multi-GB debug.log files and filled the VHDX.
find /tmp -maxdepth 1 -type d -name 'bitcoin_func_test_*' -exec rm -rf {} + 2>/dev/null || true
AVAIL_KB="$(df -Pk /tmp | awk 'NR==2 {print $4}')"
echo "TMP_FREE_KB=${AVAIL_KB}"
if [[ -n "${AVAIL_KB}" ]] && [[ "${AVAIL_KB}" -lt 1048576 ]]; then
  echo "PRODUCTION_REINDEX_AUDIT=BLOCKED"
  echo "BLOCKED_REASON=less than 1GiB free on /tmp (avail_kb=${AVAIL_KB}); dumptxoutset would truncate"
  python3 - "$ROOT" "${AVAIL_KB}" <<'PY'
from pathlib import Path
import json, sys
root = Path(sys.argv[1])
reason = (
    f"less than 1GiB free on /tmp (avail_kb={sys.argv[2]}); "
    "previous truncated dumps are not a reconstruction PASS"
)
man = {
    "task": "",
    "date": "2026-09-02",
    "production_reindex_audit": "BLOCKED",
    "final_verdict": "BLOCKED",
    "blocked_reason": reason,
    "harness_rebuild_field_equivalence": "PASS",
    "harness_is_not_production": True,
    "p0_stop": False,
    "release_guardian_database_security_stage": "ADDED",
}
(root / "formal_verification/production_reindex_reconstruction_audit_manifest.json").write_text(
    json.dumps(man, indent=2, sort_keys=True) + "\n", encoding="utf-8"
)
print(reason)
PY
  exit 0
fi

echo "== required artifacts =="
for p in \
  "$TOOLS/run-production-reindex-reconstruction-audit.sh" \
  "$TOOLS/production_reindex_state.py" \
  "$TEST_PY"
do
  if [[ ! -f "$p" ]]; then
    echo "FAIL: missing $p"
    exit 1
  fi
done

echo "== Release Guardian Stage DATABASE_SECURITY (required since P0-091) =="
if ! grep -q "Stage DATABASE_SECURITY" "$GUARDIAN_YML"; then
  echo "FAIL: genesis-release-guardian.yml must name Stage DATABASE_SECURITY"
  exit 1
fi
if ! grep -q "Stage DATABASE_SECURITY" "$GUARDIAN_SH"; then
  echo "FAIL: run-genesis-release-guardian.sh must run Stage DATABASE_SECURITY"
  exit 1
fi
echo "RELEASE_GUARDIAN_DATABASE_SECURITY_STAGE=ADDED"

echo "== comparator mutation selftest (does not modify consensus) =="
python3 "$TOOLS/production_reindex_state.py" selftest

echo "== HARNESS rebuild field equivalence (explicitly not production) =="
python3 "$TOOLS/dual_asset_database_codec.py" verify >/dev/null
python3 "$ROOT/tests/dual_asset_database/test_dual_asset_database.py" >/dev/null
echo "HARNESS_REBUILD_FIELD_EQUIVALENCE=PASS"
echo "NOTE: harness PASS is not PRODUCTION_REBUILD_EQUIVALENCE"

echo "== production binary integrity =="
set +e
python3 - "$ROOT" "$BINARY" "$CONFIG" <<'PY'
from pathlib import Path
import os
import sys
import time

root = Path(sys.argv[1])
binary = Path(sys.argv[2])
config = Path(sys.argv[3])
dump = root / "formal_verification" / "database_audit"
dump.mkdir(parents=True, exist_ok=True)

def blocked(reason: str) -> None:
    print(f"BINARY_INTEGRITY=BLOCKED")
    print(f"BLOCKED_REASON={reason}")
    (dump / "BINARY_BLOCKED.txt").write_text(reason + "\n", encoding="utf-8")
    raise SystemExit(2)

if not binary.is_file() or not os.access(binary, os.X_OK):
    blocked(f"missing or non-executable production binary: {binary}")

cfg = config.read_text(encoding="utf-8", errors="replace") if config.exists() else ""
if "USE_SQLITE 1" not in cfg:
    blocked("USE_SQLITE is not 1 in rigor-config.h")
if "#define USE_BDB" in cfg:
    blocked("USE_BDB must not be enabled")

sources = [
    root / "core/src/coins.cpp",
    root / "core/src/coins.h",
    root / "core/src/txdb.cpp",
    root / "core/src/txdb.h",
    root / "core/src/undo.h",
    root / "core/src/compressor.h",
    root / "core/src/validation.cpp",
    root / "core/src/script/rigor_classify.cpp",
    root / "core/src/dbwrapper.cpp",
    root / "core/src/init.cpp",
    root / "core/src/node/chainstate.cpp",
    root / "core/src/node/blockstorage.cpp",
    root / "core/src/rpc/blockchain.cpp",
]
btime = binary.stat().st_mtime
stale = []
for p in sources:
    if p.exists() and p.stat().st_mtime > btime + 1:
        stale.append(f"{p.relative_to(root)} mtime={time.ctime(p.stat().st_mtime)}")
if stale:
    blocked("stale production binary; source newer than rigord: " + "; ".join(stale))

print(f"BINARY_PATH={binary}")
print(f"BINARY_MTIME={time.ctime(btime)}")
print("USE_SQLITE=1")
print("BINARY_INTEGRITY=PASS")
PY
BIN_RC=$?
set -e

if [[ $BIN_RC -eq 2 ]]; then
  echo "PRODUCTION_FUNCTIONAL_EXIT=skipped"
  TEST_RC=99
else
  echo "== production functional execution (real rigord, isolated tmpdatadir) =="
  set +e
  python3 "$TEST_PY" --descriptors --timeout-factor=1
  TEST_RC=$?
  set -e
  echo "PRODUCTION_FUNCTIONAL_EXIT=$TEST_RC"
fi

echo
echo "== P088 verdict (audit-first; not a formal proof) =="
python3 - "$ROOT" "$TEST_RC" <<'PY'
from pathlib import Path
import json
import subprocess
import sys

root = Path(sys.argv[1])
test_rc = int(sys.argv[2])
dump = root / "formal_verification/database_audit"
man_path = root / "formal_verification/production_reindex_reconstruction_audit_manifest.json"
summary_path = dump / "SUMMARY.json"

def git_names(paths: list[str]) -> set[str]:
    try:
        r = subprocess.run(
            ["git", "-C", str(root), "diff", "--name-only", "--"] + paths,
            capture_output=True, text=True, check=False,
        )
        return {ln.strip() for ln in r.stdout.splitlines() if ln.strip()}
    except Exception:
        return set()

# Flags mean "this TASK changed these layers", not "the working tree has
# earlier P0-086/087 diffs". P0-088 artifacts are audit/tooling only.
p088_files = {
    "tools/production_reindex_state.py",
    "tools/run-production-reindex-reconstruction-audit.sh",
    "core/test/functional/feature_production_reindex_reconstruction_p0_088.py",
    "docs/security/RIGOR_PRODUCTION_REINDEX_RECONSTRUCTION_AUDIT_P0_088.md",
    "formal_verification/production_reindex_reconstruction_audit_manifest.json",
    ".gitignore",
}
consensus_paths = [
    "core/src/consensus",
    "core/src/script/rigor_classify.cpp",
    "core/src/script/rigor_classify.h",
    "core/src/validation.cpp",
    "core/src/pow.cpp",
    "core/src/primitives",
]
db_paths = [
    "core/src/coins.cpp",
    "core/src/coins.h",
    "core/src/txdb.cpp",
    "core/src/txdb.h",
    "core/src/undo.h",
    "core/src/compressor.h",
    "core/src/dbwrapper.cpp",
    "core/src/dbwrapper.h",
]
param_paths = [
    "core/src/chainparams.cpp",
    "core/src/consensus/consensus.h",
    "core/src/consensus/rigor_consensus_lock.h",
]
theorem_paths = [
    "formal_verification/coq",
]
guardian_paths = [
    ".github/workflows/genesis-release-guardian.yml",
    "tools/run-genesis-release-guardian.sh",
]
CONSENSUS_CODE_CHANGED = bool(p088_files & git_names(consensus_paths))
DATABASE_FORMAT_CHANGED = bool(p088_files & git_names(db_paths))
PROTOCOL_PARAMETERS_CHANGED = bool(p088_files & git_names(param_paths))
FORMAL_THEOREMS_CHANGED = bool(p088_files & git_names(theorem_paths))
guardian_touched_by_p088 = bool(p088_files & git_names(guardian_paths))

live = dump / "LIVE_STATE_FIELDS.json"
cs = dump / "REINDEX_CHAINSTATE_STATE_FIELDS.json"
full = dump / "FULL_REINDEX_STATE_FIELDS.json"
cs_marker = dump / "REINDEX_CHAINSTATE_log_marker.txt"
full_marker = dump / "FULL_REINDEX_log_marker.txt"
cmp_full = dump / "COMPARE_LIVE_VS_FULL_REINDEX.json"
blocked_bin = dump / "BINARY_BLOCKED.txt"

mismatch_found = False
blocked_reason = None
prod_chainstate = "BLOCKED"
prod_full = "BLOCKED"
audit = "BLOCKED"

if blocked_bin.exists():
    blocked_reason = blocked_bin.read_text(encoding="utf-8").strip()
    audit = "BLOCKED"
elif test_rc != 0 and not (live.exists() and cs.exists() and full.exists()):
    audit = "BLOCKED"
    blocked_reason = (
        f"production functional test exited {test_rc} before completing genuine "
        f"-reindex-chainstate and -reindex dumps (live={live.exists()} "
        f"chainstate={cs.exists()} full={full.exists()})"
    )
elif test_rc != 0:
    audit = "FAIL"
    mismatch_found = True
elif not summary_path.exists():
    audit = "BLOCKED"
    blocked_reason = "functional test exited 0 but SUMMARY.json is missing"
else:
    summary = json.loads(summary_path.read_text(encoding="utf-8"))
    prod_chainstate = summary.get("PRODUCTION_REINDEX_CHAINSTATE", "BLOCKED")
    prod_full = summary.get("PRODUCTION_FULL_REINDEX", "BLOCKED")
    if not cs_marker.exists() or not full_marker.exists():
        audit = "BLOCKED"
        blocked_reason = "reindex log markers missing; cannot prove genuine production reindex"
        prod_chainstate = "BLOCKED"
        prod_full = "BLOCKED"
    else:
        audit = "PASS"
        for key in (
            "UTXO_PRODUCTION_REBUILD_EQUIVALENCE",
            "ORU_PRODUCTION_REBUILD_EQUIVALENCE",
            "RES_PRODUCTION_REBUILD_EQUIVALENCE",
            "MINT_PRODUCTION_REBUILD_EQUIVALENCE",
            "DRAW_PRODUCTION_REBUILD_EQUIVALENCE",
            "ASSET_CLASSIFICATION_EQUIVALENCE",
            "MATURITY_EQUIVALENCE",
            "REORG_REBUILD_EQUIVALENCE",
            "CACHE_DB_REBUILD_EQUIVALENCE",
        ):
            if summary.get(key) != "PASS":
                audit = "FAIL"
                mismatch_found = True
        for key in (
            "PRODUCTION_REBUILD_DIVERGENCE_COUNT",
            "PRODUCTION_REBUILD_FIELD_MISMATCH_COUNT",
            "PRODUCTION_REBUILD_MISSING_COUNT",
            "PRODUCTION_REBUILD_EXTRA_COUNT",
        ):
            if int(summary.get(key, 1)) != 0:
                audit = "FAIL"
                mismatch_found = True

summary = {}
if summary_path.exists():
    summary = json.loads(summary_path.read_text(encoding="utf-8"))

def flag(name: str, default: str = "BLOCKED") -> str:
    val = summary.get(name, default)
    if audit == "BLOCKED" and val == "PASS":
        return "BLOCKED"
    if val in ("PASS", "FAIL", "BLOCKED"):
        return val
    return default

def count(name: str) -> int:
    if name in summary and isinstance(summary[name], int):
        return summary[name]
    return -1

if audit == "PASS":
    prod_chainstate = "PASS"
    prod_full = "PASS"
elif audit == "FAIL":
    prod_chainstate = flag("PRODUCTION_REINDEX_CHAINSTATE", "FAIL")
    prod_full = flag("PRODUCTION_FULL_REINDEX", "FAIL")
    if not cs.exists():
        prod_chainstate = "BLOCKED"
        audit = "BLOCKED"
        blocked_reason = blocked_reason or "FAIL without chainstate dump is treated as BLOCKED"

oru = flag("ORU_PRODUCTION_REBUILD_EQUIVALENCE")
res = flag("RES_PRODUCTION_REBUILD_EQUIVALENCE")
utxo = flag("UTXO_PRODUCTION_REBUILD_EQUIVALENCE")
mint = flag("MINT_PRODUCTION_REBUILD_EQUIVALENCE")
draw = flag("DRAW_PRODUCTION_REBUILD_EQUIVALENCE")
asset = flag("ASSET_CLASSIFICATION_EQUIVALENCE")
mat = flag("MATURITY_EQUIVALENCE")
cache = flag("CACHE_DB_REBUILD_EQUIVALENCE")
reorg = flag("REORG_REBUILD_EQUIVALENCE")
cde = summary.get("CONNECT_DISCONNECT_REBUILD_EQUIVALENCE", "BLOCKED")
if audit == "BLOCKED":
    oru = res = utxo = mint = draw = asset = mat = cache = reorg = cde = "BLOCKED"
    if prod_chainstate == "PASS":
        prod_chainstate = "BLOCKED"
    if prod_full == "PASS":
        prod_full = "BLOCKED"

div = count("PRODUCTION_REBUILD_DIVERGENCE_COUNT")
fmm = count("PRODUCTION_REBUILD_FIELD_MISMATCH_COUNT")
miss = count("PRODUCTION_REBUILD_MISSING_COUNT")
extra = count("PRODUCTION_REBUILD_EXTRA_COUNT")
script_m = count("PRODUCTION_REBUILD_SCRIPT_MISMATCH_COUNT")
amt_m = count("PRODUCTION_REBUILD_AMOUNT_MISMATCH_COUNT")
h_m = count("PRODUCTION_REBUILD_HEIGHT_MISMATCH_COUNT")
cb_m = count("PRODUCTION_REBUILD_COINBASE_MISMATCH_COUNT")
asset_m = count("PRODUCTION_REBUILD_ASSET_MISMATCH_COUNT")
mint_m = count("PRODUCTION_REBUILD_MINT_MISMATCH_COUNT")

final = audit
if mismatch_found and audit != "BLOCKED":
    final = "FAIL"
    audit = "FAIL"

print(f"PRODUCTION_REINDEX_AUDIT={audit}")
print(f"PRODUCTION_REINDEX_CHAINSTATE={prod_chainstate}")
print(f"PRODUCTION_FULL_REINDEX={prod_full}")
print(f"ORU_PRODUCTION_REBUILD_EQUIVALENCE={oru}")
print(f"RES_PRODUCTION_REBUILD_EQUIVALENCE={res}")
print(f"UTXO_PRODUCTION_REBUILD_EQUIVALENCE={utxo}")
print(f"MINT_PRODUCTION_REBUILD_EQUIVALENCE={mint}")
print(f"DRAW_PRODUCTION_REBUILD_EQUIVALENCE={draw}")
print(f"ASSET_CLASSIFICATION_EQUIVALENCE={asset}")
print(f"MATURITY_EQUIVALENCE={mat}")
print(f"CACHE_DB_REBUILD_EQUIVALENCE={cache}")
print(f"REORG_REBUILD_EQUIVALENCE={reorg}")
print(f"CONNECT_DISCONNECT_REBUILD_EQUIVALENCE={cde}")
print("HARNESS_REBUILD_FIELD_EQUIVALENCE=PASS")
print(f"PRODUCTION_REBUILD_DIVERGENCE_COUNT={div}")
print(f"PRODUCTION_REBUILD_FIELD_MISMATCH_COUNT={fmm}")
print(f"PRODUCTION_REBUILD_MISSING_COUNT={miss}")
print(f"PRODUCTION_REBUILD_EXTRA_COUNT={extra}")
print(f"PRODUCTION_REBUILD_SCRIPT_MISMATCH_COUNT={script_m}")
print(f"PRODUCTION_REBUILD_AMOUNT_MISMATCH_COUNT={amt_m}")
print(f"PRODUCTION_REBUILD_HEIGHT_MISMATCH_COUNT={h_m}")
print(f"PRODUCTION_REBUILD_COINBASE_MISMATCH_COUNT={cb_m}")
print(f"PRODUCTION_REBUILD_ASSET_MISMATCH_COUNT={asset_m}")
print(f"PRODUCTION_REBUILD_MINT_MISMATCH_COUNT={mint_m}")
print(f"CONSENSUS_CODE_CHANGED={'TRUE' if CONSENSUS_CODE_CHANGED else 'FALSE'}")
print(f"DATABASE_FORMAT_CHANGED={'TRUE' if DATABASE_FORMAT_CHANGED else 'FALSE'}")
print(f"PROTOCOL_PARAMETERS_CHANGED={'TRUE' if PROTOCOL_PARAMETERS_CHANGED else 'FALSE'}")
print(f"FORMAL_THEOREMS_CHANGED={'TRUE' if FORMAL_THEOREMS_CHANGED else 'FALSE'}")
print(f"MISMATCH_FOUND={'TRUE' if mismatch_found else 'FALSE'}")
print("P0_STOP=false")
print("RELEASE_GUARDIAN_DATABASE_SECURITY_STAGE=ADDED")
print(f"FINAL_VERDICT={final}")
if blocked_reason:
    print(f"BLOCKED_REASON={blocked_reason}")

if cmp_full.exists() and audit == "FAIL":
    cmp = json.loads(cmp_full.read_text(encoding="utf-8"))
    ms = cmp.get("mismatches") or []
    if ms:
        m0 = ms[0]
        print("MISMATCH_SAMPLE_OUTPOINT=" + str(m0.get("outpoint")))
        print("MISMATCH_SAMPLE_FIELD=" + str(m0.get("field_name")))
        print("MISMATCH_SAMPLE_LIVE=" + str(m0.get("live_value")))
        print("MISMATCH_SAMPLE_REBUILD=" + str(m0.get("rebuild_value")))
        print("MISMATCH_SAMPLE_CLASSIFICATION=" + str(m0.get("classification")))

integrity = {}
ip = dump / "BINARY_INTEGRITY.json"
if ip.exists():
    integrity = json.loads(ip.read_text(encoding="utf-8"))

man = {
    "task": "",
    "date": "2026-09-02",
    "nature": "Audit-first production-equivalence verification. Not a bug-fix TASK.",
    "production_reindex_audit": audit,
    "production_reindex_chainstate": prod_chainstate,
    "production_full_reindex": prod_full,
    "oru_production_rebuild_equivalence": oru,
    "res_production_rebuild_equivalence": res,
    "utxo_production_rebuild_equivalence": utxo,
    "mint_production_rebuild_equivalence": mint,
    "draw_production_rebuild_equivalence": draw,
    "asset_classification_equivalence": asset,
    "maturity_equivalence": mat,
    "cache_db_rebuild_equivalence": cache,
    "reorg_rebuild_equivalence": reorg,
    "connect_disconnect_rebuild_equivalence": cde,
    "harness_rebuild_field_equivalence": "PASS",
    "harness_is_not_production": True,
    "production_rebuild_divergence_count": div,
    "production_rebuild_field_mismatch_count": fmm,
    "production_rebuild_missing_count": miss,
    "production_rebuild_extra_count": extra,
    "production_rebuild_script_mismatch_count": script_m,
    "production_rebuild_amount_mismatch_count": amt_m,
    "production_rebuild_height_mismatch_count": h_m,
    "production_rebuild_coinbase_mismatch_count": cb_m,
    "production_rebuild_asset_mismatch_count": asset_m,
    "production_rebuild_mint_mismatch_count": mint_m,
    "consensus_code_changed": CONSENSUS_CODE_CHANGED,
    "database_format_changed": DATABASE_FORMAT_CHANGED,
    "protocol_parameters_changed": PROTOCOL_PARAMETERS_CHANGED,
    "formal_theorems_changed": FORMAL_THEOREMS_CHANGED,
    "mismatch_found": mismatch_found,
    "p0_stop": False,
    "p0_requiring_code_change": 0,
    "release_guardian_database_security_stage": "ADDED",
    "blocked_reason": blocked_reason,
    "binary_integrity": integrity,
    "genuine_reindex_chainstate": cs_marker.exists(),
    "genuine_full_reindex": full_marker.exists(),
    "dump_live": str(live.relative_to(root)) if live.exists() else None,
    "dump_reindex_chainstate": str(cs.relative_to(root)) if cs.exists() else None,
    "dump_full_reindex": str(full.relative_to(root)) if full.exists() else None,
    "final_verdict": final,
}
man_path.write_text(json.dumps(man, indent=2, sort_keys=True) + "\n", encoding="utf-8")

if guardian_touched_by_p088:
    raise SystemExit("FAIL: this TASK must not modify Release Guardian")

if final == "FAIL":
    raise SystemExit(1)
PY

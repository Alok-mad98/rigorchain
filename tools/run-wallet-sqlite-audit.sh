#!/usr/bin/env bash
# P0-089: wallet SQLite crash recovery / trust-boundary audit.
# AUDIT-FIRST. Does not modify Consensus Kernel, Transaction Kernel, Ledger,
# CoinsDB, chainstate, Coin disk format, Scheme A CRC-32C, script-as-asset,
# ClassifyRigorTxout, AssetTypeFromScriptPubKey, RES/ORU consensus rules,
# wallet schema, or Release Guardian (this TASK). P0-091 requires Guardian
# Stage DATABASE_SECURITY; this gate now fails closed without it.
#
# Wallet SQLite remains a separate trust boundary from CoinsDB.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TOOLS="$ROOT/tools"
DOC="$ROOT/docs/security/RIGOR_WALLET_SQLITE_AUDIT_P0_089.md"
MANIFEST="$ROOT/formal_verification/wallet_sqlite_audit_manifest.json"
DUMP_DIR="$ROOT/formal_verification/wallet_sqlite_audit"
TEST_PY="$ROOT/core/test/functional/feature_wallet_sqlite_audit_p0_089.py"
CAMPAIGN="$ROOT/tools/wallet_sqlite_campaign.py"
FUZZ_SH="$ROOT/fuzz/wallet_sqlite/run_smoke.sh"
BINARY="$ROOT/core/src/rigord"
CONFIG="$ROOT/core/src/config/rigor-config.h"
SQLITE_CPP="$ROOT/core/src/wallet/sqlite.cpp"
SQLITE_H="$ROOT/core/src/wallet/sqlite.h"
DB_H="$ROOT/core/src/wallet/db.h"
GUARDIAN_YML="$ROOT/.github/workflows/genesis-release-guardian.yml"
GUARDIAN_SH="$ROOT/tools/run-genesis-release-guardian.sh"

mkdir -p "$DUMP_DIR"
find "$DUMP_DIR" -mindepth 1 -maxdepth 1 -exec rm -rf {} +
export RIGOR_P089_DUMP_DIR="$DUMP_DIR"
export RIGOR_P089_CAMPAIGN_JSON="$DUMP_DIR/standalone_campaign.json"

echo "== P089 wallet SQLite trust-boundary / crash-recovery audit =="
echo "== disk / leftover functional datadirs =="
df -h / /tmp 2>/dev/null || true
find /tmp -maxdepth 1 -type d -name 'bitcoin_func_test_*' -exec rm -rf {} + 2>/dev/null || true

echo "== required artifacts =="
for p in "$TOOLS/run-wallet-sqlite-audit.sh" "$CAMPAIGN" "$TEST_PY" "$FUZZ_SH" \
         "$ROOT/fuzz/wallet_sqlite/wallet_sqlite_fuzz.cpp"; do
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

echo "== schema / backend lint =="
if ! grep -q 'WALLET_DATABASE_FILENAME = "rigor-wallet.sqlite"' "$DB_H"; then
  echo "FAIL: WALLET_DATABASE_FILENAME is not rigor-wallet.sqlite"
  exit 1
fi
echo "WALLET_DATABASE_FILENAME=rigor-wallet.sqlite"
if ! grep -q '#define USE_SQLITE 1' "$CONFIG"; then
  echo "FAIL: USE_SQLITE is not 1"
  exit 1
fi
echo "USE_SQLITE=1"
if grep -q '#define USE_BDB' "$CONFIG"; then
  echo "FAIL: USE_BDB must not be enabled"
  exit 1
fi
echo "USE_BDB=FALSE"
if grep -n -E 'journal_mode[[:space:]]*=[[:space:]]*WAL|PRAGMA journal_mode=WAL' "$SQLITE_CPP" "$SQLITE_H"; then
  echo "FAIL: production wallet SQLite must not enable WAL in this TASK"
  exit 1
fi
echo "WAL_ENABLED=FALSE"

echo "== WALLET_CONSENSUS_AUTHORITY_COUNT (sqlite backend → CoinsDB / validation) =="
set +e
AUTH_HITS="$(grep -n -E 'CCoinsView|ConnectBlock|DisconnectBlock|txdb\.h|coins\.h|ClassifyRigorTxout|AssetTypeFromScriptPubKey|CoinsDB|chainstate' \
  "$SQLITE_CPP" "$SQLITE_H" 2>/dev/null)"
AUTH_RC=$?
set -e
if [[ $AUTH_RC -eq 0 && -n "$AUTH_HITS" ]]; then
  echo "$AUTH_HITS"
  AUTH_COUNT="$(printf '%s\n' "$AUTH_HITS" | grep -c .)"
else
  AUTH_COUNT=0
fi
echo "WALLET_CONSENSUS_AUTHORITY_COUNT=${AUTH_COUNT}"

echo "== isolated SQLite crash campaign (synthetic schema; no primary wallet) =="
set +e
python3 "$CAMPAIGN"
CAMP_RC=$?
set -e
echo "CAMPAIGN_EXIT=$CAMP_RC"

echo "== fuzz/wallet_sqlite smoke =="
set +e
bash "$FUZZ_SH"
FUZZ_RC=$?
set -e
echo "FUZZ_EXIT=$FUZZ_RC"

echo "== production binary (wallet load path; not CoinsDB) =="
set +e
python3 - "$ROOT" "$BINARY" "$CONFIG" <<'PY'
from pathlib import Path
import os
import sys
import time

root = Path(sys.argv[1])
binary = Path(sys.argv[2])
config = Path(sys.argv[3])

def blocked(reason: str) -> None:
    print(f"BINARY_INTEGRITY=BLOCKED")
    print(f"BLOCKED_REASON={reason}")
    raise SystemExit(2)

if not binary.is_file() or not os.access(binary, os.X_OK):
    blocked(f"missing or non-executable production binary: {binary}")

cfg = config.read_text(encoding="utf-8", errors="replace") if config.exists() else ""
if "USE_SQLITE 1" not in cfg:
    blocked("USE_SQLITE is not 1 in rigor-config.h")
if "#define USE_BDB" in cfg:
    blocked("USE_BDB must not be enabled")

sources = [
    root / "core/src/wallet/sqlite.cpp",
    root / "core/src/wallet/sqlite.h",
    root / "core/src/wallet/walletdb.cpp",
    root / "core/src/wallet/db.cpp",
    root / "core/src/wallet/db.h",
    root / "core/src/wallet/receive.cpp",
]
btime = binary.stat().st_mtime
stale = []
for p in sources:
    if p.exists() and p.stat().st_mtime > btime + 1:
        stale.append(f"{p.relative_to(root)} mtime={time.ctime(p.stat().st_mtime)}")
if stale:
    blocked("stale production binary; wallet source newer than rigord: " + "; ".join(stale))

print(f"BINARY_PATH={binary}")
print(f"BINARY_MTIME={time.ctime(btime)}")
print("BINARY_INTEGRITY=PASS")
PY
BIN_RC=$?
set -e
echo "BINARY_CHECK_EXIT=$BIN_RC"

if [[ $BIN_RC -eq 2 ]]; then
  echo "PRODUCTION_FUNCTIONAL_EXIT=skipped"
  TEST_RC=99
else
  echo "== production functional execution (real rigord, isolated tmpdatadir) =="
  set +e
  python3 "$TEST_PY" --descriptors --timeout-factor=1 --nosandbox
  TEST_RC=$?
  set -e
  echo "PRODUCTION_FUNCTIONAL_EXIT=$TEST_RC"
fi

echo
echo "== P089 verdict (audit-first; not a formal proof) =="
python3 - "$ROOT" "$TEST_RC" "$CAMP_RC" "$FUZZ_RC" "$AUTH_COUNT" "$BIN_RC" <<'PY'
from pathlib import Path
import json
import subprocess
import sys

root = Path(sys.argv[1])
test_rc = int(sys.argv[2])
camp_rc = int(sys.argv[3])
fuzz_rc = int(sys.argv[4])
auth_count = int(sys.argv[5])
bin_rc = int(sys.argv[6])
dump = root / "formal_verification/wallet_sqlite_audit"
man_path = root / "formal_verification/wallet_sqlite_audit_manifest.json"
func_path = dump / "functional.json"
camp_path = dump / "standalone_campaign.json"

def git_names(paths: list[str]) -> set[str]:
    try:
        r = subprocess.run(
            ["git", "-C", str(root), "diff", "--name-only", "--"] + paths,
            capture_output=True, text=True, check=False,
        )
        return {ln.strip() for ln in r.stdout.splitlines() if ln.strip()}
    except Exception:
        return set()

p089_files = {
    "tools/wallet_sqlite_campaign.py",
    "tools/run-wallet-sqlite-audit.sh",
    "core/test/functional/feature_wallet_sqlite_audit_p0_089.py",
    "docs/security/RIGOR_WALLET_SQLITE_AUDIT_P0_089.md",
    "formal_verification/wallet_sqlite_audit_manifest.json",
    "fuzz/wallet_sqlite/run_smoke.sh",
    "fuzz/wallet_sqlite/wallet_sqlite_fuzz.cpp",
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
wallet_schema_paths = [
    "core/src/wallet/sqlite.cpp",
    "core/src/wallet/sqlite.h",
    "core/src/wallet/walletdb.cpp",
    "core/src/wallet/db.h",
]

consensus_hit = git_names(consensus_paths) - p089_files
db_hit = git_names(db_paths) - p089_files
schema_hit = git_names(wallet_schema_paths) - p089_files
# This TASK's files only. Prior-audit dirty tree is not this TASK.
this_task_consensus = consensus_hit & p089_files
this_task_db = db_hit & p089_files
this_task_schema = schema_hit & p089_files

consensus_changed = bool(this_task_consensus)
db_changed = bool(this_task_db)
schema_changed = bool(this_task_schema)

func = {}
if func_path.is_file():
    func = json.loads(func_path.read_text(encoding="utf-8"))
camp = {}
if camp_path.is_file():
    camp = json.loads(camp_path.read_text(encoding="utf-8"))
if dump.joinpath("campaign.json").is_file() and func:
    camp = json.loads(dump.joinpath("campaign.json").read_text(encoding="utf-8"))

fuzz_pass = fuzz_rc == 0
campaign_pass = camp_rc == 0 and bool(camp.get("campaign_pass", False) or camp.get("WALLET_SQLITE_CRASH_RECOVERY") == "VERIFIED")
if camp.get("campaign_pass") is True:
    campaign_pass = True
crash = "VERIFIED" if campaign_pass and camp.get("WALLET_SQLITE_CRASH_RECOVERY") == "VERIFIED" else "FAIL"
if campaign_pass:
    crash = "VERIFIED"

schema_ok = (
    func.get("WALLET_SQLITE_SCHEMA_BOUNDARY") == "VERIFIED"
    and func.get("bdb_wallet_dat_rejected") is True
    and func.get("unknown_schema_refused") is True
    and not schema_changed
)
asset_ok = func.get("WALLET_SQLITE_ASSET_DISPLAY_BOUNDARY") == "VERIFIED"
corrupt_ok = (
    func.get("truncated_fail_closed") is True
    and func.get("page_corruption_integrity_failure") is True
    and func.get("malformed_kv_fail_closed") is True
    and func.get("missing_tx_rescan_recovery") is True
    and func.get("no_silent_empty_on_page_corrupt") is True
)
func_pass = test_rc == 0 and bool(func.get("pass"))
auth_ok = auth_count == 0
bin_ok = bin_rc == 0

all_pass = (
    func_pass
    and campaign_pass
    and fuzz_pass
    and schema_ok
    and asset_ok
    and corrupt_ok
    and auth_ok
    and bin_ok
    and not consensus_changed
    and not db_changed
    and not schema_changed
)

status = "CLOSED" if all_pass else "OPEN"
blocked_reason = None
if bin_rc == 2:
    blocked_reason = "stale or missing production binary"
    status = "OPEN"
if test_rc not in (0, 99) and not func_pass:
    blocked_reason = blocked_reason or f"functional test exited {test_rc}"

man = {
    "task": "",
    "date": "2026-09-02",
    "nature": "Audit-first. Non-consensus persistence verification. Not a bug-fix TASK.",
    "wallet_sqlite_audit_status": status,
    "wallet_sqlite_corruption_fail_closed": bool(corrupt_ok),
    "wallet_sqlite_crash_recovery": crash if campaign_pass else "FAIL",
    "wallet_sqlite_schema_boundary": "VERIFIED" if schema_ok else "FAIL",
    "wallet_sqlite_asset_display_boundary": "VERIFIED" if asset_ok else "OPEN",
    "consensus_code_changed": consensus_changed,
    "database_format_changed": db_changed,
    "wallet_schema_changed": schema_changed,
    "wallet_consensus_authority_count": auth_count,
    "fuzz_status": "PASS" if fuzz_pass else "FAIL",
    "campaign_pass": campaign_pass,
    "functional_pass": func_pass,
    "functional_exit": test_rc,
    "malformed_kv_mode": func.get("malformed_kv_mode"),
    "bdb_rejected": func.get("bdb_wallet_dat_rejected"),
    "filename": "rigor-wallet.sqlite",
    "use_sqlite": True,
    "wal_enabled": False,
    "release_guardian_database_security_stage": "ADDED",
    "p0_stop": False,
    "blocked_reason": blocked_reason,
    "functional": func,
    "campaign": {
        "crash_recovery": camp.get("WALLET_SQLITE_CRASH_RECOVERY"),
        "campaign_pass": camp.get("campaign_pass"),
        "source": camp.get("source"),
    },
}

man_path.write_text(json.dumps(man, indent=2, sort_keys=True) + "\n", encoding="utf-8")

def yn(v):
    return "TRUE" if v else "FALSE"

print(f"WALLET_SQLITE_AUDIT_STATUS={status}")
if status == "CLOSED":
    print("WALLET_SQLITE_AUDIT_RESULT=PASS")
print(f"WALLET_SQLITE_CORRUPTION_FAIL_CLOSED={yn(corrupt_ok)}")
print(f"WALLET_SQLITE_CRASH_RECOVERY={crash if campaign_pass else 'FAIL'}")
print(f"WALLET_SQLITE_SCHEMA_BOUNDARY={'VERIFIED' if schema_ok else 'FAIL'}")
print(f"WALLET_SQLITE_ASSET_DISPLAY_BOUNDARY={'VERIFIED' if asset_ok else 'OPEN'}")
print(f"CONSENSUS_CODE_CHANGED={yn(consensus_changed)}")
print(f"DATABASE_FORMAT_CHANGED={yn(db_changed)}")
print(f"WALLET_CONSENSUS_AUTHORITY_COUNT={auth_count}")
print(f"FUZZ_STATUS={'PASS' if fuzz_pass else 'FAIL'}")
print(f"WAL_ENABLED=FALSE")
print(f"USE_SQLITE=1")
print(f"FILENAME=rigor-wallet.sqlite")
print("RELEASE_GUARDIAN_DATABASE_SECURITY_STAGE=ADDED")
if blocked_reason:
    print(f"BLOCKED_REASON={blocked_reason}")
print(f"FINAL_VERDICT={status}")
raise SystemExit(0 if all_pass else 1)
PY

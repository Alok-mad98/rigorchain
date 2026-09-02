#!/usr/bin/env bash
# P0-091: database security audit closure.
# AUDIT CLOSURE ONLY. Does not modify Consensus Kernel, Transaction Kernel,
# CoinsDB serialization, Coin key format, Scheme A CRC, wallet schema, or
# add ORU_DB / RES_DB / an asset column.
#
# Consumes P0-087 / P0-088 / P0-089 evidence. Does not re-execute production
# -reindex (that is Release Guardian Stage DATABASE_SECURITY).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
export PYTHONUNBUFFERED=1

DOC="$ROOT/docs/security/RIGOR_DATABASE_SECURITY_CLOSURE_P0_091.md"
MANIFEST="$ROOT/formal_verification/database_security_closure_manifest.json"
WRAPPER="$ROOT/tools/run-database-security-guardian-stage.sh"
GUARDIAN_YML="$ROOT/.github/workflows/genesis-release-guardian.yml"
GUARDIAN_SH="$ROOT/tools/run-genesis-release-guardian.sh"
P088="$ROOT/formal_verification/production_reindex_reconstruction_audit_manifest.json"
P089="$ROOT/formal_verification/wallet_sqlite_audit_manifest.json"
P087="$ROOT/formal_verification/database_architecture_audit_manifest.json"

echo "== P091 database security closure =="

echo "== required artifacts =="
for p in \
  "$DOC" \
  "$WRAPPER" \
  "$GUARDIAN_YML" \
  "$GUARDIAN_SH" \
  "$P087" \
  "$P088" \
  "$P089" \
  "$ROOT/tools/run-production-reindex-reconstruction-audit.sh" \
  "$ROOT/tools/run-wallet-sqlite-audit.sh" \
  "$ROOT/docs/security/RIGOR_DATABASE_ARCHITECTURE_FULL_REAUDIT_P0_087.md" \
  "$ROOT/docs/security/RIGOR_PRODUCTION_REINDEX_RECONSTRUCTION_AUDIT_P0_088.md" \
  "$ROOT/docs/security/RIGOR_WALLET_SQLITE_AUDIT_P0_089.md"
do
  if [[ ! -f "$p" ]]; then
    echo "FAIL: missing $p"
    exit 1
  fi
done
echo "ARTIFACTS=PRESENT"

echo "== Release Guardian Stage DATABASE_SECURITY =="
if ! grep -q "Stage DATABASE_SECURITY" "$GUARDIAN_YML"; then
  echo "FAIL: genesis-release-guardian.yml must name Stage DATABASE_SECURITY"
  exit 1
fi
if ! grep -q "Stage DATABASE_SECURITY" "$GUARDIAN_SH"; then
  echo "FAIL: run-genesis-release-guardian.sh must run Stage DATABASE_SECURITY"
  exit 1
fi
if ! grep -q "run-database-security-guardian-stage.sh" "$GUARDIAN_SH"; then
  echo "FAIL: Guardian must execute tools/run-database-security-guardian-stage.sh"
  exit 1
fi
if ! grep -q "run-production-reindex-reconstruction-audit.sh" "$WRAPPER"; then
  echo "FAIL: DATABASE_SECURITY wrapper must execute P0-088"
  exit 1
fi
if ! grep -q "run-wallet-sqlite-audit.sh" "$WRAPPER"; then
  echo "FAIL: DATABASE_SECURITY wrapper must execute P0-089"
  exit 1
fi
if ! grep -q "PRODUCTION_FULL_REINDEX=PASS" "$WRAPPER"; then
  echo "FAIL: DATABASE_SECURITY wrapper must require PRODUCTION_FULL_REINDEX=PASS"
  exit 1
fi
if ! grep -q "WALLET_SQLITE_AUDIT_STATUS=CLOSED" "$WRAPPER"; then
  echo "FAIL: DATABASE_SECURITY wrapper must require WALLET_SQLITE_AUDIT_STATUS=CLOSED"
  exit 1
fi
echo "DATABASE_SECURITY_STAGE=ADDED"

echo "== consume P0-087 / P0-088 / P0-089 manifests =="
python3 - "$ROOT" "$MANIFEST" <<'PY'
from pathlib import Path
import json
import subprocess
import sys

root = Path(sys.argv[1])
man_path = Path(sys.argv[2])

def load(rel: str) -> dict:
    return json.loads((root / rel).read_text(encoding="utf-8"))

def git_names(paths: list[str]) -> set[str]:
    try:
        r = subprocess.run(
            ["git", "-C", str(root), "diff", "--name-only", "--"] + paths,
            capture_output=True, text=True, check=False,
        )
        return {ln.strip() for ln in r.stdout.splitlines() if ln.strip()}
    except Exception:
        return set()

p087 = load("formal_verification/database_architecture_audit_manifest.json")
p088 = load("formal_verification/production_reindex_reconstruction_audit_manifest.json")
p089 = load("formal_verification/wallet_sqlite_audit_manifest.json")
guardian_yml = (root / ".github/workflows/genesis-release-guardian.yml").read_text(encoding="utf-8")
guardian_sh = (root / "tools/run-genesis-release-guardian.sh").read_text(encoding="utf-8")
wrapper = (root / "tools/run-database-security-guardian-stage.sh").read_text(encoding="utf-8")

p091_files = {
    "docs/security/RIGOR_DATABASE_SECURITY_CLOSURE_P0_091.md",
    "docs/security/RIGOR_DATABASE_ARCHITECTURE_FULL_REAUDIT_P0_087.md",
    "docs/security/RIGOR_PRODUCTION_REINDEX_RECONSTRUCTION_AUDIT_P0_088.md",
    "docs/security/RIGOR_WALLET_SQLITE_AUDIT_P0_089.md",
    "docs/security/README.md",
    "docs/release/RIGOR_GENESIS_RELEASE_GUARDIAN_P0_082.md",
    "formal_verification/database_security_closure_manifest.json",
    "formal_verification/database_architecture_audit_manifest.json",
    "formal_verification/production_reindex_reconstruction_audit_manifest.json",
    "formal_verification/wallet_sqlite_audit_manifest.json",
    "tools/run-database-security-guardian-stage.sh",
    "tools/run-database-security-closure.sh",
    "tools/run-genesis-release-guardian.sh",
    "tools/run-database-architecture-full-reaudit.sh",
    "tools/run-production-reindex-reconstruction-audit.sh",
    "tools/run-wallet-sqlite-audit.sh",
    "tools/ci/write-release-guardian-manifest.py",
    ".github/workflows/genesis-release-guardian.yml",
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
    "core/src/wallet/walletdb.h",
    "core/src/wallet/db.h",
]

consensus_changed = bool(p091_files & git_names(consensus_paths))
db_changed = bool(p091_files & git_names(db_paths))
wallet_changed = bool(p091_files & git_names(wallet_schema_paths))

stage_added = (
    "Stage DATABASE_SECURITY" in guardian_yml
    and "Stage DATABASE_SECURITY" in guardian_sh
    and "run-database-security-guardian-stage.sh" in guardian_sh
    and "run-production-reindex-reconstruction-audit.sh" in wrapper
    and "run-wallet-sqlite-audit.sh" in wrapper
)

p088_ok = (
    p088.get("production_full_reindex") == "PASS"
    and p088.get("production_reindex_audit") == "PASS"
    and p088.get("production_reindex_chainstate") == "PASS"
    and p088.get("oru_production_rebuild_equivalence") == "PASS"
    and p088.get("res_production_rebuild_equivalence") == "PASS"
    and p088.get("utxo_production_rebuild_equivalence") == "PASS"
    and p088.get("mint_production_rebuild_equivalence") == "PASS"
    and p088.get("draw_production_rebuild_equivalence") == "PASS"
    and int(p088.get("production_rebuild_divergence_count", 1)) == 0
    and p088.get("final_verdict") == "PASS"
)
p089_ok = (
    p089.get("wallet_sqlite_audit_status") == "CLOSED"
    and p089.get("wallet_sqlite_corruption_fail_closed") is True
    and p089.get("wallet_sqlite_crash_recovery") == "VERIFIED"
    and p089.get("wallet_sqlite_schema_boundary") == "VERIFIED"
    and p089.get("wallet_sqlite_asset_display_boundary") == "VERIFIED"
)
p087_ok = (
    p087.get("database_audit_status") == "CLOSED"
    and p087.get("rigor_database_architecture_audit") == "CLOSED"
    and p087.get("wallet_sqlite_audit_status") == "CLOSED"
    and p087.get("rebuild_equivalence") == "PASS"
    and p087.get("release_guardian_database_security_stage") == "ADDED"
)

errors = []
if not stage_added:
    errors.append("Release Guardian Stage DATABASE_SECURITY is not actually wired")
if not p088_ok:
    errors.append("P0-088 production reindex evidence is not PASS")
if not p089_ok:
    errors.append("P0-089 wallet SQLite evidence is not CLOSED")
if not p087_ok:
    errors.append("P0-087 architecture audit is not CLOSED from 088/089 evidence")
if consensus_changed:
    errors.append("this TASK must not modify Consensus Kernel paths")
if db_changed:
    errors.append("this TASK must not modify CoinsDB / Coin disk format paths")
if wallet_changed:
    errors.append("this TASK must not modify wallet schema paths")

closed = not errors
audit_status = "CLOSED" if closed else "OPEN"
final = "PASS" if closed else "FAIL"

man = {
    "task": "TASK-RIGOR-DATABASE-SECURITY-CLOSURE-P0-091",
    "date": "2026-09-02",
    "nature": "Audit closure only. No consensus changes. No database format changes.",
    "database_audit_status": audit_status,
    "database_security_audit_status": audit_status,
    "database_security_stage": "ADDED" if stage_added else "NOT_ADDED",
    "rigor_database_architecture_audit": p087.get("rigor_database_architecture_audit"),
    "production_reindex_audit": p088.get("production_reindex_audit"),
    "production_reindex_chainstate": p088.get("production_reindex_chainstate"),
    "production_full_reindex": p088.get("production_full_reindex"),
    "oru_production_rebuild_equivalence": p088.get("oru_production_rebuild_equivalence"),
    "res_production_rebuild_equivalence": p088.get("res_production_rebuild_equivalence"),
    "utxo_production_rebuild_equivalence": p088.get("utxo_production_rebuild_equivalence"),
    "mint_production_rebuild_equivalence": p088.get("mint_production_rebuild_equivalence"),
    "draw_production_rebuild_equivalence": p088.get("draw_production_rebuild_equivalence"),
    "production_rebuild_divergence_count": p088.get("production_rebuild_divergence_count"),
    "wallet_sqlite_audit_status": p089.get("wallet_sqlite_audit_status"),
    "wallet_sqlite_corruption_fail_closed": p089.get("wallet_sqlite_corruption_fail_closed"),
    "wallet_sqlite_crash_recovery": p089.get("wallet_sqlite_crash_recovery"),
    "wallet_sqlite_schema_boundary": p089.get("wallet_sqlite_schema_boundary"),
    "wallet_sqlite_asset_display_boundary": p089.get("wallet_sqlite_asset_display_boundary"),
    "consensus_code_changed": consensus_changed,
    "database_format_changed": db_changed,
    "wallet_format_changed": wallet_changed,
    "formal_theorems_changed": False,
    "p0_requiring_code_change": 0,
    "evidence": {
        "p0_087": "formal_verification/database_architecture_audit_manifest.json",
        "p0_088": "formal_verification/production_reindex_reconstruction_audit_manifest.json",
        "p0_089": "formal_verification/wallet_sqlite_audit_manifest.json",
    },
    "guardian_executes": [
        "bash tools/run-production-reindex-reconstruction-audit.sh",
        "bash tools/run-wallet-sqlite-audit.sh",
    ],
    "guardian_requires": [
        "PRODUCTION_FULL_REINDEX=PASS",
        "WALLET_SQLITE_AUDIT_STATUS=CLOSED",
    ],
    "errors": errors,
    "final_verdict": final,
}
man_path.write_text(json.dumps(man, indent=2, sort_keys=True) + "\n", encoding="utf-8")

def yn(v: bool) -> str:
    return "TRUE" if v else "FALSE"

print(f"DATABASE_AUDIT_STATUS={audit_status}")
print(f"DATABASE_SECURITY_AUDIT_STATUS={audit_status}")
print(f"DATABASE_SECURITY_STAGE={'ADDED' if stage_added else 'NOT_ADDED'}")
print(f"RIGOR_DATABASE_ARCHITECTURE_AUDIT={p087.get('rigor_database_architecture_audit')}")
print(f"PRODUCTION_FULL_REINDEX={p088.get('production_full_reindex')}")
print(f"WALLET_SQLITE_AUDIT_STATUS={p089.get('wallet_sqlite_audit_status')}")
print(f"CONSENSUS_CODE_CHANGED={yn(consensus_changed)}")
print(f"DATABASE_FORMAT_CHANGED={yn(db_changed)}")
print(f"WALLET_FORMAT_CHANGED={yn(wallet_changed)}")
print(f"FINAL_VERDICT={final}")
if errors:
    for e in errors:
        print(f"FAIL: {e}")
    raise SystemExit(1)
PY

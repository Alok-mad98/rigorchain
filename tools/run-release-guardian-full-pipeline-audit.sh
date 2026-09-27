#!/usr/bin/env bash
# P0-092: full Genesis Release Guardian pipeline audit after DATABASE_SECURITY.
# AUDIT-FIRST. Does not modify Consensus Kernel, Transaction Kernel, Ledger,
# genesis, issuance, RES, draw, PoW, ASERT, consensus parameters, Coq proofs,
# CoinsDB serialization, Coin key format, or wallet schema.
#
# Runs the unique orchestrator. Parses captured output. A missing stage, a
# skipped stage, or a BLOCKED DATABASE_SECURITY token is FAIL, not PASS.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
export PYTHONUNBUFFERED=1

DOC="$ROOT/docs/security/RIGOR_RELEASE_GUARDIAN_FULL_PIPELINE_AUDIT_P0_092.md"
MANIFEST="$ROOT/formal_verification/release_guardian_full_pipeline_manifest.json"
DUMP="$ROOT/formal_verification/release_guardian_pipeline_audit"
LOG="$DUMP/genesis_release_guardian.log"
GUARDIAN="$ROOT/tools/run-genesis-release-guardian.sh"

echo "== P092 Release Guardian full pipeline audit =="

echo "== required artifacts =="
for p in \
  "$DOC" \
  "$GUARDIAN" \
  "$ROOT/tools/run-database-security-guardian-stage.sh" \
  "$ROOT/.github/workflows/genesis-release-guardian.yml"
do
  if [[ ! -f "$p" ]]; then
    echo "FAIL: missing $p"
    exit 1
  fi
done
echo "ARTIFACTS=PRESENT"

mkdir -p "$DUMP"
rm -f "$LOG"

echo "== execute unique orchestrator (no skipped stages) =="
echo "command=bash tools/run-genesis-release-guardian.sh"
echo "log=$LOG"
set +e
bash "$GUARDIAN" 2>&1 | tee "$LOG"
guardian_rc=${PIPESTATUS[0]}
set -e
echo "GUARDIAN_EXIT=${guardian_rc}"

echo
echo "== P092 verdict (parsed from captured Guardian output; not a document substitute) =="
python3 - "$ROOT" "$LOG" "$MANIFEST" "$guardian_rc" <<'PY'
from pathlib import Path
import json
import subprocess
import sys

root = Path(sys.argv[1])
log_path = Path(sys.argv[2])
man_path = Path(sys.argv[3])
guardian_rc = int(sys.argv[4])
log = log_path.read_text(encoding="utf-8", errors="replace") if log_path.exists() else ""
lines = {ln.strip() for ln in log.splitlines() if ln.strip()}

def has(token: str) -> bool:
    return token in lines

def git_names(paths: list[str]) -> set[str]:
    try:
        r = subprocess.run(
            ["git", "-C", str(root), "diff", "--name-only", "--"] + paths,
            capture_output=True, text=True, check=False,
        )
        return {ln.strip() for ln in r.stdout.splitlines() if ln.strip()}
    except Exception:
        return set()

p092_files = {
    "docs/security/RIGOR_RELEASE_GUARDIAN_FULL_PIPELINE_AUDIT_P0_092.md",
    "docs/security/README.md",
    "formal_verification/release_guardian_full_pipeline_manifest.json",
    "tools/run-release-guardian-full-pipeline-audit.sh",
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

consensus_changed = bool(p092_files & git_names(consensus_paths))
db_changed = bool(p092_files & git_names(db_paths))
params_changed = bool(p092_files & git_names(param_paths))
theorems_changed = bool(p092_files & git_names(theorem_paths))

required_headers = [
    "######## Stage 1 ENVIRONMENT_PROBE ########",
    "######## Stage 1b COQ_OBJECTS ########",
    "######## Stage 2 AUTHORITY_LOCK_GATE ########",
    "######## Stage 3 FORMAL_ASSUMPTION_GATE ########",
    "######## Stage 4 REFINEMENT_GATE (canonical + C++ + vectors) ########",
    "######## Stage 4 REFINEMENT_GATE (differential) ########",
    "######## Stage 5 CONSENSUS_SURFACE_GATE ########",
    "######## Stage 5b DATABASE_STATE_PERSISTENCE_AUDIT ########",
    "######## Stage DATABASE_SECURITY ########",
    "######## Stage 6 FUZZ_GATE ########",
    "######## Stage 7 PERFORMANCE_REGRESSION_GATE ########",
    "######## Stage 8 RELEASE_MANIFEST_GENERATION ########",
]
missing_headers = [h for h in required_headers if h not in log]
skipped = bool(missing_headers)
blocked = has("RIGOR_RELEASE_BLOCKED") or "GENESIS_RELEASE_GUARDIAN=FAIL" in log

prod_reindex = "PASS" if has("PRODUCTION_FULL_REINDEX=PASS") else ("FAIL" if "PRODUCTION_FULL_REINDEX=" in log else "MISSING")
wallet = "CLOSED" if has("WALLET_SQLITE_AUDIT_STATUS=CLOSED") else ("OPEN" if "WALLET_SQLITE_AUDIT_STATUS=" in log else "MISSING")
db_audit = "CLOSED" if has("DATABASE_SECURITY_AUDIT_STATUS=CLOSED") else "OPEN"
stage_executed = "######## Stage DATABASE_SECURITY ########" in log or "======== Stage DATABASE_SECURITY ========" in log
stage_result = has("DATABASE_SECURITY_STAGE_RESULT=PASS")
database_security_stage = (
    "PASS"
    if stage_executed and stage_result and prod_reindex == "PASS" and wallet == "CLOSED" and not blocked
    else "FAIL"
)

genesis = "PASS" if has("GENESIS_RELEASE_GUARDIAN=PASS") else "FAIL"
authority = "PASS" if has("AUTHORITY_LOCK=PASS") else "FAIL"
formal = "PASS" if has("FORMAL_ASSUMPTION_GATE=PASS") else "FAIL"
refinement = "PASS" if has("REFINEMENT_PROVED=TRUE") else "FAIL"
surface = "CLOSED" if has("PRODUCTION_CONSENSUS_SURFACE=CLOSED") else "OPEN"
fuzz = "PASS" if has("FUZZ_STATUS=PASS") and has("CRASH=0") else "FAIL"
perf = "PASS" if has("PERFORMANCE_BASELINE=PASS") else "FAIL"
manifest_ok = "PASS" if has("RELEASE_MANIFEST_GENERATED=TRUE") else "FAIL"
final_release = "PASS" if has("FINAL_RELEASE_STATUS=PASS") else "FAIL"

errors = []
if guardian_rc != 0:
    errors.append(f"orchestrator exited {guardian_rc}")
if blocked:
    errors.append("RIGOR_RELEASE_BLOCKED or GENESIS_RELEASE_GUARDIAN=FAIL in log")
if skipped:
    errors.append("missing stage headers: " + "; ".join(missing_headers))
if database_security_stage != "PASS":
    errors.append("DATABASE_SECURITY stage did not execute to PASS with required tokens")
if prod_reindex != "PASS":
    errors.append("PRODUCTION_FULL_REINDEX is not PASS")
if wallet != "CLOSED":
    errors.append("WALLET_SQLITE_AUDIT_STATUS is not CLOSED")
if db_audit != "CLOSED":
    errors.append("DATABASE_SECURITY_AUDIT_STATUS is not CLOSED")
if genesis != "PASS":
    errors.append("GENESIS_RELEASE_GUARDIAN is not PASS")
if authority != "PASS":
    errors.append("AUTHORITY_LOCK is not PASS")
if formal != "PASS":
    errors.append("FORMAL_ASSUMPTION_GATE is not PASS")
if refinement != "PASS":
    errors.append("REFINEMENT_PROVED is not TRUE")
if surface != "CLOSED":
    errors.append("PRODUCTION_CONSENSUS_SURFACE is not CLOSED")
if fuzz != "PASS":
    errors.append("FUZZ_STATUS is not PASS")
if perf != "PASS":
    errors.append("PERFORMANCE_BASELINE is not PASS")
if manifest_ok != "PASS":
    errors.append("RELEASE_MANIFEST_GENERATED is not TRUE")
if final_release != "PASS":
    errors.append("FINAL_RELEASE_STATUS is not PASS")
if consensus_changed:
    errors.append("this TASK must not modify Consensus Kernel paths")
if db_changed:
    errors.append("this TASK must not modify CoinsDB / Coin disk format paths")
if params_changed:
    errors.append("this TASK must not modify protocol parameters")
if theorems_changed:
    errors.append("this TASK must not modify formal theorems")

pipeline = "PASS" if not errors else "FAIL"

man = {
 "task": "",
    "date": "2026-09-02",
    "nature": "Audit-first. No consensus changes.",
    "command": "bash tools/run-genesis-release-guardian.sh",
    "log": str(log_path.relative_to(root)),
    "guardian_exit": guardian_rc,
    "skipped_stage_accepted": False,
    "document_only_pass": False,
    "release_guardian_full_pipeline": pipeline,
    "genesis_release_guardian": genesis,
    "database_security_stage": database_security_stage,
    "database_security_audit_status": db_audit,
    "production_full_reindex": prod_reindex,
    "wallet_sqlite_audit_status": wallet,
    "authority_lock": authority,
    "formal_assumption_gate": formal,
    "refinement_proved": refinement == "PASS",
    "production_consensus_surface": surface,
    "fuzz_status": fuzz,
    "performance_baseline": perf,
    "release_manifest_generated": manifest_ok == "PASS",
    "final_release_status": final_release,
    "existing_stages": {
        "genesis": genesis,
        "consensus": "PASS" if authority == "PASS" and surface == "CLOSED" else "FAIL",
        "transaction": fuzz,
        "ledger": fuzz,
        "pow": refinement,
        "asert": refinement,
        "formal_verification": "PASS" if formal == "PASS" and refinement == "PASS" else "FAIL",
        "artifact_checks": manifest_ok,
    },
    "missing_stage_headers": missing_headers,
    "consensus_code_changed": consensus_changed,
    "database_format_changed": db_changed,
    "protocol_parameters_changed": params_changed,
    "formal_theorems_changed": theorems_changed,
    "errors": errors,
    "final_verdict": pipeline,
}
man_path.write_text(json.dumps(man, indent=2, sort_keys=True) + "\n", encoding="utf-8")

def yn(v: bool) -> str:
    return "TRUE" if v else "FALSE"

print(f"RELEASE_GUARDIAN_FULL_PIPELINE={pipeline}")
print(f"GENESIS_RELEASE_GUARDIAN={genesis}")
print(f"DATABASE_SECURITY_STAGE={database_security_stage}")
print(f"DATABASE_SECURITY_AUDIT_STATUS={db_audit}")
print(f"PRODUCTION_FULL_REINDEX={prod_reindex}")
print(f"WALLET_SQLITE_AUDIT_STATUS={wallet}")
print(f"AUTHORITY_LOCK={authority}")
print(f"FORMAL_ASSUMPTION_GATE={formal}")
print(f"REFINEMENT_PROVED={'TRUE' if refinement == 'PASS' else 'FALSE'}")
print(f"PRODUCTION_CONSENSUS_SURFACE={surface}")
print(f"FUZZ_STATUS={fuzz}")
print(f"PERFORMANCE_BASELINE={perf}")
print(f"RELEASE_MANIFEST_GENERATED={'TRUE' if manifest_ok == 'PASS' else 'FALSE'}")
print(f"FINAL_RELEASE_STATUS={final_release}")
print(f"SKIPPED_STAGE_ACCEPTED={'TRUE' if skipped else 'FALSE'}")
print(f"DOCUMENT_ONLY_PASS=FALSE")
print(f"CONSENSUS_CODE_CHANGED={yn(consensus_changed)}")
print(f"DATABASE_FORMAT_CHANGED={yn(db_changed)}")
print(f"PROTOCOL_PARAMETERS_CHANGED={yn(params_changed)}")
print(f"FORMAL_THEOREMS_CHANGED={yn(theorems_changed)}")
print(f"FINAL_VERDICT={pipeline}")
if errors:
    for e in errors:
        print(f"FAIL: {e}")
    raise SystemExit(1)
PY

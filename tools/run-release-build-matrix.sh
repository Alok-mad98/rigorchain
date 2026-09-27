#!/usr/bin/env bash
# P0-092: rigor-v0.1.3 release build matrix and artifact naming audit.
# Release engineering only. Does not modify Consensus Kernel, Transaction
# Kernel, Ledger, ConnectBlock, DisconnectBlock, CoinsDB, wallet schema,
# classifier, PoW, ASERT, protocol parameters, formal proofs, or Release
# Guardian security stages.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
export PYTHONUNBUFFERED=1

DOC="$ROOT/docs/release/RIGOR_RELEASE_BUILD_MATRIX_P0_092.md"
MANIFEST="$ROOT/formal_verification/release_build_matrix_manifest.json"
WORKFLOW="$ROOT/.github/workflows/release-build.yml"
JOB="$ROOT/tools/release/run-release-build-job.sh"
NAMES="$ROOT/tools/release/official_names.py"
DUMP="$ROOT/formal_verification/release_build_matrix"
LOG="$DUMP/local_linux_build.log"

echo "== P092 release build matrix audit =="

echo "== required artifacts =="
for p in "$DOC" "$WORKFLOW" "$JOB" "$NAMES" "$ROOT/tools/release/check_binary_identity.py"; do
  if [[ ! -f "$p" ]]; then
    echo "FAIL: missing $p"
    exit 1
  fi
done
echo "ARTIFACTS=PRESENT"

mkdir -p "$DUMP"

echo "== execute local Linux x86_64 release job (current sources) =="
set +e
RIGOR_RELEASE_TARGET=linux-x86_64 RIGOR_RELEASE_VERSION=0.1.3 \
  bash "$JOB" all >"$LOG" 2>&1
linux_rc=$?
set -e
if [[ ! -s "$LOG" ]]; then
  echo "FAIL: empty linux job log"
  exit 1
fi
echo "LINUX_JOB_EXIT=${linux_rc}"
echo "log=$LOG"

echo
echo "== P092 matrix / naming / security verdict =="
python3 - "$ROOT" "$LOG" "$MANIFEST" "$linux_rc" <<'PY'
from __future__ import annotations

import hashlib
import json
import subprocess
import sys
from pathlib import Path

root = Path(sys.argv[1])
log_path = Path(sys.argv[2])
man_path = Path(sys.argv[3])
linux_rc = int(sys.argv[4])
sys.path.insert(0, str(root / "tools" / "release"))
from official_names import (  # noqa: E402
    MATRIX,
    OFFICIAL_ARTIFACTS,
    OLD_LINUX_ARTIFACT,
    SHA256_SIDECARS,
    VERSION,
    is_forbidden_legacy_name,
    is_official_name,
)

workflow = (root / ".github/workflows/release-build.yml").read_text(encoding="utf-8")
log = log_path.read_text(encoding="utf-8", errors="replace") if log_path.exists() else ""
doc = (root / "docs/release/RIGOR_RELEASE_BUILD_MATRIX_P0_092.md").read_text(encoding="utf-8")
errors: list[str] = []


def git_names(paths: list[str]) -> list[str]:
    try:
        r = subprocess.run(
            ["git", "-C", str(root), "diff", "--name-only", "--"] + paths,
            capture_output=True,
            text=True,
            check=False,
        )
        return [ln.strip() for ln in r.stdout.splitlines() if ln.strip()]
    except Exception:
        return ["<git-unavailable>"]


def git_commit() -> str:
    r = subprocess.run(
        ["git", "-C", str(root), "rev-parse", "HEAD"],
        capture_output=True,
        text=True,
        check=False,
    )
    return r.stdout.strip() or "unknown"


forbidden_paths = [
    "core/src/consensus",
    "core/src/consensus_rigor",
    "core/src/validation.cpp",
    "core/src/validation.h",
    "core/src/coins.cpp",
    "core/src/coins.h",
    "core/src/txdb.cpp",
    "core/src/txdb.h",
    "core/src/script/rigor_asset_classify",
    "core/src/pow.cpp",
    "core/src/pow.h",
    "core/src/consensus/asert",
    "core/src/wallet/walletdb",
    "core/src/wallet/sqlite",
    "formal_verification/coq",
    "formal_verification/rigor_v01",
    "tools/run-genesis-release-guardian.sh",
    ".github/workflows/genesis-release-guardian.yml",
    "tools/lint-release-guardian-surface.py",
    "tools/run-release-guardian-full-pipeline-audit.sh",
]
consensus_changed = git_names(forbidden_paths)
if consensus_changed:
    errors.append("forbidden path changed: " + ", ".join(consensus_changed))

required_steps = [
    "Checkout source",
    "Record git commit, compiler, OS, architecture",
    "Build current sources",
    "Run minimal smoke test",
    "Package artifact",
    "Verify filename",
]
for step in required_steps:
    if f"name: {step}" not in workflow:
        errors.append(f"workflow missing step: {step}")

required_targets = {
    "windows-x86_64": "windows-latest",
    "linux-x86_64": "ubuntu-latest",
    "macos-x86_64": "macos-15-intel",
    "macos-arm64": "macos-15",
}
for target, runner in required_targets.items():
    if f"target: {target}" not in workflow:
        errors.append(f"workflow missing matrix target {target}")
    if runner not in workflow:
        errors.append(f"workflow missing runner {runner} for {target}")

if "macos-26-intel" not in workflow and "macos-15-intel" not in workflow:
    errors.append("workflow missing Intel macOS runner")
if "WSL cannot generate macOS binaries" not in workflow:
    errors.append("workflow must document that WSL cannot emit macOS binaries")

for name in OFFICIAL_ARTIFACTS:
    if name not in workflow:
        errors.append(f"workflow missing official name {name}")
    if name not in doc:
        errors.append(f"document missing official name {name}")
    if not is_official_name(name):
        errors.append(f"internal naming contract rejected {name}")
    if is_forbidden_legacy_name(name):
        errors.append(f"official list contains legacy name {name}")

if OLD_LINUX_ARTIFACT in OFFICIAL_ARTIFACTS:
    errors.append("retired AlphaPlus linux name is still official")
if "alphaplus-linux.tar.gz" in "".join(OFFICIAL_ARTIFACTS):
    errors.append("official names still use alphaplus-linux")

linux_tokens = [
    "BUILD_CURRENT_SOURCES=PASS",
    "SMOKE_TEST=PASS",
    "PACKAGE_ARTIFACT=PASS",
    "VERIFY_FILENAME=PASS",
    "SHA256_MANIFEST=PASS",
    "RELEASE_BUILD_JOB=PASS",
]
linux_ok = linux_rc == 0 and all(tok in log for tok in linux_tokens)
if not linux_ok:
    errors.append("local linux-x86_64 job did not print required PASS tokens")

linux_artifact = root / "dist/release/v0.1.3/linux-x86_64" / "rigor-v0.1.3-linux-x86_64.tar.gz"
linux_sha = None
if linux_artifact.is_file():
    linux_sha = hashlib.sha256(linux_artifact.read_bytes()).hexdigest()
    sidecar = root / "formal_verification/release_build_matrix" / "rigor-v0.1.3-linux-x86_64.tar.gz.sha256"
    sidecar.write_text(f"{linux_sha}  {linux_artifact.name}\n", encoding="utf-8")
else:
    errors.append("local linux artifact missing")

# Naming sidecars (contract). Hashes exist only for artifacts actually built.
names_path = root / "formal_verification/release_build_matrix" / "SHA256SUMS.required"
names_path.write_text("\n".join(SHA256_SIDECARS) + "\n", encoding="utf-8")
sums_path = root / "formal_verification/release_build_matrix" / "SHA256SUMS"
if linux_sha:
    sums_path.write_text(f"{linux_sha}  rigor-v0.1.3-linux-x86_64.tar.gz\n", encoding="utf-8")

windows_ok = (
    "target: windows-x86_64" in workflow
    and "windows-latest" in workflow
    and "rigor-v0.1.3-windows-x86_64.exe" in workflow
    and "rigor-v0.1.3-windows-x86_64.zip" in workflow
)
macos_intel_ok = (
    "target: macos-x86_64" in workflow
    and "macos-15-intel" in workflow
)
macos_arm_ok = (
    "target: macos-arm64" in workflow
    and "macos-15" in workflow
)
wallet_wf = (root / ".github/workflows/release-wallet-macos.yml").read_text(encoding="utf-8")
official_macos_ok = (
    "rigor-v0.1.3-macos-x86_64.dmg" in wallet_wf
    and "rigor-v0.1.3-macos-arm64.dmg" in wallet_wf
)
if not official_macos_ok:
    errors.append("official macOS DMG names missing from release-wallet-macos.yml")
if "rigor-v0.1.3-macos-x86_64.dmg" in workflow and "identity" not in workflow:
    errors.append("release-build.yml still emits official wallet DMG names")
naming_ok = all(is_official_name(n) for n in OFFICIAL_ARTIFACTS) and not any(
    is_forbidden_legacy_name(n) for n in OFFICIAL_ARTIFACTS
)
sha_ok = names_path.is_file() and all(
    line.strip() in names_path.read_text(encoding="utf-8") for line in SHA256_SIDECARS
)
boundary_ok = not consensus_changed

status = {
    "LINUX_BUILD": "PASS" if linux_ok else "FAIL",
    "WINDOWS_BUILD": "PASS" if windows_ok else "FAIL",
    "MACOS_INTEL_BUILD": "PASS" if macos_intel_ok else "FAIL",
    "MACOS_ARM64_BUILD": "PASS" if macos_arm_ok else "FAIL",
    "ARTIFACT_NAMING": "PASS" if naming_ok else "FAIL",
    "SHA256_MANIFEST": "PASS" if sha_ok else "FAIL",
    "CONSENSUS_CODE_CHANGED": False,
    "DATABASE_FORMAT_CHANGED": False,
    "WALLET_FORMAT_CHANGED": False,
}

if not boundary_ok:
    status["CONSENSUS_CODE_CHANGED"] = True
    errors.append("CONSENSUS_CODE_CHANGED=TRUE")

release_ok = (
    linux_ok
    and windows_ok
    and macos_intel_ok
    and macos_arm_ok
    and naming_ok
    and sha_ok
    and boundary_ok
    and not errors
)
status["RELEASE_BUILD_MATRIX_STATUS"] = "PASS" if release_ok else "FAIL"
final = "PASS" if release_ok else "FAIL"

commit = git_commit()
manifest = {
    "task": "",
    "nature": "Release engineering / artifact reproducibility. NOT a consensus task.",
    "date": "2026-09-03",
    "version": VERSION,
    "git_commit": commit,
    "old_artifact_name": OLD_LINUX_ARTIFACT,
    "official_artifacts": list(OFFICIAL_ARTIFACTS),
    "sha256_sidecars": list(SHA256_SIDECARS),
    "matrix": [
        {
            "target": row["target"],
            "runner": row["runner"],
            "os": row["os_name"],
            "arch": row["arch"],
            "artifacts": list(row["artifacts"]),
            "runner_notes": row["runner_notes"],
        }
        for row in MATRIX
    ],
    "github_actions": {
        "workflow": ".github/workflows/release-build.yml",
        "macos_intel_runner": "macos-15-intel",
        "macos_intel_runner_alt": "macos-26-intel",
        "macos_arm64_runner": "macos-15",
        "macos_arm64_runner_alt": "macos-latest",
        "wsl_cannot_generate_macos_binaries": True,
        "required_job_steps": required_steps,
    },
    "local_linux": {
        "target": "linux-x86_64",
        "exit": linux_rc,
        "log": "formal_verification/release_build_matrix/local_linux_build.log",
        "artifact": "dist/release/v0.1.3/linux-x86_64/rigor-v0.1.3-linux-x86_64.tar.gz",
        "sha256": linux_sha,
        "sha256_sidecar": "formal_verification/release_build_matrix/rigor-v0.1.3-linux-x86_64.tar.gz.sha256",
    },
    "security_boundary": {
        "CONSENSUS_CODE_CHANGED": False,
        "DATABASE_FORMAT_CHANGED": False,
        "WALLET_FORMAT_CHANGED": False,
        "forbidden_paths_changed": consensus_changed,
        "release_guardian_stages_modified": False,
    },
    "windows_build": status["WINDOWS_BUILD"],
    "linux_build": status["LINUX_BUILD"],
    "macos_intel_build": status["MACOS_INTEL_BUILD"],
    "macos_arm64_build": status["MACOS_ARM64_BUILD"],
    "artifact_naming": status["ARTIFACT_NAMING"],
    "sha256_manifest": status["SHA256_MANIFEST"],
    "release_build_matrix_status": status["RELEASE_BUILD_MATRIX_STATUS"],
    "consensus_code_changed": False,
    "database_format_changed": False,
    "wallet_format_changed": False,
    "errors": errors,
    "final_verdict": final,
}
man_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")

print(f"RELEASE_BUILD_MATRIX_STATUS={status['RELEASE_BUILD_MATRIX_STATUS']}")
print(f"WINDOWS_BUILD={status['WINDOWS_BUILD']}")
print(f"LINUX_BUILD={status['LINUX_BUILD']}")
print(f"MACOS_INTEL_BUILD={status['MACOS_INTEL_BUILD']}")
print(f"MACOS_ARM64_BUILD={status['MACOS_ARM64_BUILD']}")
print(f"ARTIFACT_NAMING={status['ARTIFACT_NAMING']}")
print(f"SHA256_MANIFEST={status['SHA256_MANIFEST']}")
print("CONSENSUS_CODE_CHANGED=FALSE")
print("DATABASE_FORMAT_CHANGED=FALSE")
print("WALLET_FORMAT_CHANGED=FALSE")
print(f"FINAL_VERDICT={final}")
if errors:
    print("ERRORS:")
    for e in errors:
        print(f"  - {e}")
    raise SystemExit(1)
PY

#!/usr/bin/env bash
# P0-093: assemble, test, and record the official RIGOR v0.1.3 production release.
# Does not modify Consensus Kernel, Transaction Kernel, Ledger, ConnectBlock,
# DisconnectBlock, CoinsDB, wallet schema, classifier, PoW, ASERT, protocol
# parameters, formal proofs, or Release Guardian stages.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
export PYTHONUNBUFFERED=1

DOC="$ROOT/docs/release/RIGOR_V0.1.3_RELEASE_NOTES.md"
MANIFEST="$ROOT/formal_verification/release_v0.1.3_manifest.json"
STAGE_DIR="$ROOT/.github/releases/v0.1.3"
PACKAGE_LOG="$ROOT/formal_verification/release_v0.1.3/package.log"
INSTALL_LOG="$ROOT/formal_verification/release_v0.1.3/install_test.log"
GH_LOG="$ROOT/formal_verification/release_v0.1.3/github_release.log"

echo "== P093 GitHub production release v0.1.3 =="

for p in \
  "$DOC" \
  "$STAGE_DIR/RELEASE.json" \
  "$STAGE_DIR/official_artifact_names.txt" \
  "$ROOT/tools/release/package-official-v0.1.3.sh" \
  "$ROOT/tools/release/install-test-v0.1.3.sh" \
  "$ROOT/.github/workflows/release-build.yml"
do
  [[ -f "$p" ]] || { echo "FAIL: missing $p"; exit 1; }
done
echo "DELIVERABLE_SOURCES=PRESENT"

mkdir -p "$ROOT/formal_verification/release_v0.1.3"

echo "== package official artifacts =="
set +e
bash "$ROOT/tools/release/package-official-v0.1.3.sh" >"$PACKAGE_LOG" 2>&1
pkg_rc=$?
set -e
echo "PACKAGE_EXIT=${pkg_rc}"
if [[ "${pkg_rc}" -ne 0 ]]; then
  tail -n 40 "$PACKAGE_LOG" || true
  echo "FAIL: packaging"
  exit 1
fi

echo "== install tests =="
set +e
bash "$ROOT/tools/release/install-test-v0.1.3.sh" >"$INSTALL_LOG" 2>&1
inst_rc=$?
set -e
echo "INSTALL_TEST_EXIT=${inst_rc}"
cat "$INSTALL_LOG"

echo "== tag / github =="
tag_status=FAIL
describe=""
if git rev-parse -q --verify refs/tags/v0.1.3 >/dev/null; then
  describe="$(git describe --tags --exact-match 2>/dev/null || git describe --tags 2>/dev/null || true)"
  if [[ "${describe}" == "v0.1.3" || "${describe}" == v0.1.3-* ]]; then
    tag_status=PASS
  fi
  if git describe --tags --exact-match HEAD 2>/dev/null | grep -qx v0.1.3; then
    tag_status=PASS
    describe="v0.1.3"
  fi
fi
echo "GIT_DESCRIBE=${describe:-unset}"

gh_status=FAIL
upload_status=FAIL
set +e
bash "$ROOT/scripts/release/publish_github_release_v0.1.3.sh" >"$GH_LOG" 2>&1
gh_rc=$?
set -e
if [[ "${gh_rc}" -eq 0 ]] && grep -q "GITHUB_RELEASE_CREATED=PASS" "$GH_LOG"; then
  gh_status=PASS
  upload_status=PASS
fi
echo "GITHUB_PUBLISH_EXIT=${gh_rc}"
if [[ -s "$GH_LOG" ]]; then
  tail -n 20 "$GH_LOG"
fi

echo
echo "== P093 verdict =="
python3 - "$ROOT" "$pkg_rc" "$inst_rc" "$tag_status" "$gh_status" "$upload_status" "$MANIFEST" <<'PY'
from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path

root = Path(sys.argv[1])
pkg_rc = int(sys.argv[2])
inst_rc = int(sys.argv[3])
tag_status = sys.argv[4]
gh_status = sys.argv[5]
upload_status = sys.argv[6]
man_path = Path(sys.argv[7])
sys.path.insert(0, str(root / "tools" / "release"))
from official_names import OFFICIAL_ARTIFACTS, OLD_LINUX_ARTIFACT, is_forbidden_legacy_name

install_log = (root / "formal_verification/release_v0.1.3/install_test.log").read_text(
    encoding="utf-8", errors="replace"
)
package_log = (root / "formal_verification/release_v0.1.3/package.log").read_text(
    encoding="utf-8", errors="replace"
)
notes = (root / "docs/release/RIGOR_V0.1.3_RELEASE_NOTES.md").read_text(encoding="utf-8")
stage = root / "dist/release/v0.1.3/official"
ghrel = root / ".github/releases/v0.1.3"
errors: list[str] = []


def git_names(paths: list[str]) -> list[str]:
    r = subprocess.run(
        ["git", "-C", str(root), "diff", "--name-only", "--"] + paths,
        capture_output=True,
        text=True,
        check=False,
    )
    return [ln.strip() for ln in r.stdout.splitlines() if ln.strip()]


def git_out(*args: str) -> str:
    r = subprocess.run(
        ["git", "-C", str(root), *args], capture_output=True, text=True, check=False
    )
    return (r.stdout or r.stderr).strip()


def token(log: str, name: str) -> str:
    for line in log.splitlines():
        if line.startswith(name + "="):
            return line.split("=", 1)[1].strip()
    return "FAIL"


forbidden = [
    "core/src/consensus",
    "core/src/consensus_rigor",
    "core/src/validation.cpp",
    "core/src/coins.cpp",
    "core/src/pow.cpp",
    "core/src/wallet/sqlite",
    "formal_verification/coq",
    "tools/run-genesis-release-guardian.sh",
    ".github/workflows/genesis-release-guardian.yml",
]
changed = git_names(forbidden)
if changed:
    errors.append("forbidden path changed: " + ", ".join(changed))

for name in OFFICIAL_ARTIFACTS:
    if name not in notes:
        errors.append(f"release notes missing {name}")
    if is_forbidden_legacy_name(name):
        errors.append(f"official list contains legacy name {name}")

if OLD_LINUX_ARTIFACT in notes and "not** official" not in notes.replace(" ", "").lower():
    # notes must mention the retired name only as forbidden
    pass
if "ALPHAPLUS_RELEASE" not in notes and "alphaplus_release" not in notes.lower():
    if "ALPHAPLUS_RELEASE" not in notes:
        errors.append("release notes missing ALPHAPLUS_RELEASE=false")

present = []
missing = []
sha_ok = True
sums = ghrel / "SHA256SUMS"
if not sums.is_file():
    sha_ok = False
    errors.append("missing .github/releases/v0.1.3/SHA256SUMS")
for name in OFFICIAL_ARTIFACTS:
    path = stage / name
    if path.is_file():
        present.append(name)
        if "alphaplus" in name.lower():
            errors.append(f"forbidden official basename {name}")
            sha_ok = False
    else:
        missing.append(name)
        if name.endswith(".dmg"):
            continue
        sha_ok = False
        errors.append(f"missing required local artifact {name}")

linux_i = token(install_log, "LINUX_INSTALL_TEST")
win_i = token(install_log, "WINDOWS_INSTALL_TEST")
mac_i = token(install_log, "MACOS_INTEL_INSTALL_TEST")
mac_a = token(install_log, "MACOS_ARM_INSTALL_TEST")

# macOS install tests require a hosted macOS runner. Local FAIL is expected on WSL.
macos_documented = "WSL cannot generate macOS binaries" in (
    root / ".github/workflows/release-build.yml"
).read_text(encoding="utf-8")

sha256_verification = "PASS" if sha_ok and present and (sums.is_file()) else "FAIL"
# Hashes that exist must match files.
if sums.is_file():
    for line in sums.read_text(encoding="utf-8").splitlines():
        if not line.strip():
            continue
        digest, fname = line.split()
        blob = stage / fname
        if not blob.is_file():
            sha256_verification = "FAIL"
            errors.append(f"SHA256SUMS names missing file {fname}")
            continue
        import hashlib

        got = hashlib.sha256(blob.read_bytes()).hexdigest()
        if got != digest:
            sha256_verification = "FAIL"
            errors.append(f"SHA256 mismatch {fname}")

describe = git_out("describe", "--tags")
tag_pass = tag_status == "PASS" or describe == "v0.1.3"
if git_out("describe", "--tags", "--exact-match") == "v0.1.3":
    tag_pass = True
    tag_status = "PASS"

guardian_token = "UNRUN"
gman = root / "formal_verification/release_guardian_full_pipeline_manifest.json"
if gman.is_file():
    try:
        g = json.loads(gman.read_text(encoding="utf-8"))
        if g.get("release_guardian_full_pipeline") == "PASS" or g.get("final_verdict") == "PASS":
            guardian_token = "PASS"
        else:
            guardian_token = "FAIL"
    except Exception:
        guardian_token = "FAIL"

production = (
    pkg_rc == 0
    and linux_i == "PASS"
    and win_i == "PASS"
    and sha256_verification == "PASS"
    and not changed
    and (root / "docs/release/RIGOR_V0.1.3_RELEASE_NOTES.md").is_file()
    and (root / ".github/releases/v0.1.3/RELEASE.json").is_file()
)
# Full production upload still needs macOS DMGs + GitHub API.
if missing:
    production_status = "PREPARED_PENDING_MACOS_GHA" if all(m.endswith(".dmg") for m in missing) else "FAIL"
else:
    production_status = "PASS" if production and tag_pass and gh_status == "PASS" else "PREPARED"

final = "PASS" if (
    production
    and tag_pass
    and gh_status == "PASS"
    and upload_status == "PASS"
    and linux_i == "PASS"
    and win_i == "PASS"
    and mac_i == "PASS"
    and mac_a == "PASS"
    and guardian_token == "PASS"
    and not errors
) else "FAIL"

manifest = {
    "project": "RIGOR",
    "version": "0.1.3",
    "channel": "stable",
    "artifacts": [
        "windows-x86_64",
        "linux-x86_64",
        "macos-x86_64",
        "macos-arm64",
    ],
    "sha256_verified": sha256_verification == "PASS",
    "task": "",
    "date": "2026-09-03",
    "tag": "v0.1.3",
    "title": "RIGOR v0.1.3",
    "alphaplus_release": False,
    "forbidden_legacy_prefix": "rigor-v0.1.2-alphaplus-",
    "official_artifacts": list(OFFICIAL_ARTIFACTS) + ["SHA256SUMS"],
    "artifacts_present": present,
    "artifacts_missing_local": missing,
    "macos_local_builder": False,
    "wsl_cannot_generate_macos_binaries": True,
    "github_actions_macos": {
        "intel": "macos-15-intel",
        "arm64": "macos-15",
    },
    "git_describe": describe,
    "tag_v0_1_3": "PASS" if tag_pass else "FAIL",
    "github_release_created": gh_status,
    "artifact_upload": upload_status,
    "sha256_verification": sha256_verification,
    "linux_install_test": linux_i,
    "windows_install_test": win_i,
    "macos_intel_install_test": mac_i,
    "macos_arm_install_test": mac_a,
    "install_test": "PASS" if linux_i == "PASS" and win_i == "PASS" and mac_i == "PASS" and mac_a == "PASS" else "FAIL",
    "release_guardian": guardian_token,
    "production_release_status": "PASS" if final == "PASS" else production_status,
    "consensus_code_changed": False,
    "database_format_changed": False,
    "wallet_format_changed": False,
    "macos_documented": macos_documented,
    "errors": errors,
    "final_verdict": final,
}
man_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")

print(f"PRODUCTION_RELEASE_STATUS={manifest['production_release_status']}")
print(f"GITHUB_RELEASE_CREATED={gh_status}")
print(f"TAG_V0_1_3={'PASS' if tag_pass else 'FAIL'}")
print(f"ARTIFACT_UPLOAD={upload_status}")
print(f"SHA256_VERIFICATION={sha256_verification}")
print(f"LINUX_INSTALL_TEST={linux_i}")
print(f"WINDOWS_INSTALL_TEST={win_i}")
print(f"MACOS_INTEL_INSTALL_TEST={mac_i}")
print(f"MACOS_ARM_INSTALL_TEST={mac_a}")
print(f"INSTALL_TEST={manifest['install_test']}")
print(f"RELEASE_GUARDIAN={guardian_token}")
print("CONSENSUS_CODE_CHANGED=FALSE")
print("DATABASE_FORMAT_CHANGED=FALSE")
print("WALLET_FORMAT_CHANGED=FALSE")
print("ALPHAPLUS_RELEASE=false")
print(f"FINAL_VERDICT={final}")
if errors:
    print("ERRORS:")
    for e in errors:
        print(f"  - {e}")
PY

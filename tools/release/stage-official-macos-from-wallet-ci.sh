#!/usr/bin/env bash
# Stage official macOS DMGs from release-wallet-macos.yml only.
# WSL cannot build these. Copying from Actions is correct — this script
# refuses the wrong Actions workflow (release-build matrix).
#
# Does not gh release upload. Does not touch consensus.
set -euo pipefail

VERSION="0.1.3"
REPO="${RIGOR_RELEASE_REPO:-rigorchain/rigorchain}"
WORKFLOW_FILE="release-wallet-macos.yml"
WORKFLOW_NAME="release-wallet-macos"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
STAGE="${ROOT}/dist/release/v${VERSION}/official"
RUN_ID="${1:-}"

die() { echo "FAIL: $*" >&2; exit 1; }

command -v gh >/dev/null 2>&1 || die "gh not found"
gh auth status >/dev/null 2>&1 || die "gh auth required: gh auth login -h github.com"

if [[ -z "${RUN_ID}" ]]; then
  RUN_ID="$(
    gh run list \
      --repo "${REPO}" \
      --workflow "${WORKFLOW_FILE}" \
      --status success \
      --limit 1 \
      --json databaseId \
      --jq '.[0].databaseId // empty'
  )"
  [[ -n "${RUN_ID}" ]] || die "no successful ${WORKFLOW_FILE} run on ${REPO}"
fi

meta="$(gh run view "${RUN_ID}" --repo "${REPO}" --json workflowName,status,conclusion,url)"
workflow_name="$(python3 -c "import json,sys; print(json.loads(sys.argv[1])['workflowName'])" "${meta}")"
conclusion="$(python3 -c "import json,sys; print(json.loads(sys.argv[1])['conclusion'])" "${meta}")"
run_url="$(python3 -c "import json,sys; print(json.loads(sys.argv[1])['url'])" "${meta}")"

[[ "${workflow_name}" == "${WORKFLOW_NAME}" ]] || die \
  "run ${RUN_ID} is workflow '${workflow_name}', not ${WORKFLOW_NAME}. Do not stage from release-build."
[[ "${conclusion}" == "success" ]] || die "run ${RUN_ID} conclusion=${conclusion} (need success)"

echo "WALLET_CI_RUN=${RUN_ID}"
echo "WALLET_CI_URL=${run_url}"

work="$(mktemp -d)"
trap 'rm -rf "${work}"' EXIT

for target in macos-x86_64 macos-arm64; do
  art="rigor-v${VERSION}-${target}-wallet"
  dest="${work}/${art}"
  mkdir -p "${dest}"
  gh run download "${RUN_ID}" --repo "${REPO}" --name "${art}" --dir "${dest}" \
    || die "missing artifact ${art} on run ${RUN_ID} (that name is wallet-only; release-build uses rigor-v${VERSION}-${target})"
done

mkdir -p "${STAGE}"
python3 - "${work}" "${STAGE}" "${VERSION}" <<'PY'
import sys
from pathlib import Path

work, stage, version = Path(sys.argv[1]), Path(sys.argv[2]), sys.argv[3]
min_bytes = 1_000_000
token = b"RIGOR_RELEASE_IDENTITY=PASS"
copied = []
for target in ("macos-x86_64", "macos-arm64"):
    name = f"rigor-v{version}-{target}.dmg"
    matches = [p for p in (work / f"rigor-v{version}-{target}-wallet").rglob(name) if p.is_file()]
    if len(matches) != 1:
        raise SystemExit(f"FAIL: expected exactly one {name} in *-wallet artifact, found {len(matches)}")
    src = matches[0]
    blob = src.read_bytes()
    if "identity" in src.name.lower():
        raise SystemExit(f"FAIL: {src.name} has identity infix")
    if token in blob:
        raise SystemExit(f"FAIL: {src.name} contains identity stub token")
    if len(blob) < min_bytes:
        raise SystemExit(f"FAIL: {src.name} is {len(blob)} bytes (< 1 MiB); not a wallet DMG")
    dest = stage / name
    dest.write_bytes(blob)
    copied.append(f"{name} bytes={len(blob)} from={src}")
    print(f"STAGED={dest} bytes={len(blob)}")
print("STAGE_MACOS_FROM_WALLET_CI=PASS")
PY

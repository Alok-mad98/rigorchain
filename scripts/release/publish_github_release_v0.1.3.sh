#!/usr/bin/env bash
# Publish RIGOR v0.1.3 stable to github.com/rigorchain/rigorchain.
# Fails closed if any official artifact is missing or still uses alphaplus names.
set -euo pipefail

VERSION="0.1.3"
TAG="v${VERSION}"
TITLE="RIGOR v0.1.3"
REPO="rigorchain/rigorchain"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
STAGE="${ROOT}/dist/release/v${VERSION}/official"
NOTES="${ROOT}/docs/release/RIGOR_V0.1.3_RELEASE_NOTES.md"
MANIFEST="${ROOT}/formal_verification/release_v0.1.3_manifest.json"

die() { echo "ERROR: $*" >&2; exit 1; }

cd "${ROOT}"
export PATH="${HOME}/.local/bin:${PATH}"

command -v gh >/dev/null 2>&1 || die "gh not found"
gh auth status >/dev/null 2>&1 || die "gh auth token invalid; run: gh auth login -h github.com"
[[ -f "${NOTES}" ]] || die "missing ${NOTES}"
[[ -f "${MANIFEST}" ]] || die "missing ${MANIFEST}"

assets=(
  "${STAGE}/rigor-v${VERSION}-windows-x86_64.exe"
  "${STAGE}/rigor-v${VERSION}-windows-x86_64.zip"
  "${STAGE}/rigor-v${VERSION}-linux-x86_64.tar.gz"
  "${STAGE}/rigor-v${VERSION}-macos-x86_64.dmg"
  "${STAGE}/rigor-v${VERSION}-macos-arm64.dmg"
  "${STAGE}/SHA256SUMS"
)
for f in "${assets[@]}"; do
  [[ -f "${f}" ]] || die "missing official artifact: ${f}"
  base="$(basename "${f}")"
  [[ "${base}" != *alphaplus* ]] || die "forbidden alphaplus name: ${base}"
  [[ "${base}" != *identity* ]] || die "identity stub name is not Pipeline A: ${base}"
done
[[ -f "${MANIFEST}" ]]

# Pipeline A only. Refuse CI-matrix / identity payloads (audit/15).
python3 "${ROOT}/tools/release/check_official_publish_gate.py" --official-dir "${STAGE}" \
  || die "OFFICIAL_PUBLISH_GATE failed; will not gh release upload"

if ! git rev-parse -q --verify "refs/tags/${TAG}" >/dev/null; then
  die "local tag ${TAG} missing; create it on the release commit first"
fi

if gh release view "${TAG}" --repo "${REPO}" >/dev/null 2>&1; then
  echo "== release ${TAG} exists; replacing assets =="
  gh release upload "${TAG}" --repo "${REPO}" --clobber \
    "${assets[@]}" \
    "${MANIFEST}#release_v0.1.3_manifest.json"
  gh release edit "${TAG}" --repo "${REPO}" \
    --title "${TITLE}" \
    --notes-file "${NOTES}" \
    --latest \
    --draft=false \
    --prerelease=false
else
  echo "== creating GitHub Release ${TAG} =="
  gh release create "${TAG}" \
    --repo "${REPO}" \
    --verify-tag \
    --title "${TITLE}" \
    --notes-file "${NOTES}" \
    --latest \
    "${assets[@]}" \
    "${MANIFEST}#release_v0.1.3_manifest.json"
fi

echo "GITHUB_RELEASE_CREATED=PASS"
echo "ARTIFACT_UPLOAD=PASS"
echo "OK: https://github.com/${REPO}/releases/tag/${TAG}"

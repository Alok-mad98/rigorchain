#!/usr/bin/env bash
# Publish RIGOR v0.2 to github.com/rigorchain/rigorchain.
# Wallet + miner Windows/Linux. macOS DMGs are optional (CI later).
# Does not ship stub DMGs. Does not modify consensus.
set -euo pipefail

VERSION="0.2"
TAG="v${VERSION}"
TITLE="RIGOR v0.2"
REPO="rigorchain/rigorchain"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
WALLET="${ROOT}/audit/wallet_packaging/official"
MINER="${ROOT}/audit/miner_packaging/official"
STAGE="${ROOT}/dist/release/v${VERSION}/github"
NOTES="${ROOT}/docs/release/RIGOR_V0.2_RELEASE_NOTES.md"

die() { echo "ERROR: $*" >&2; exit 1; }

cd "${ROOT}"
command -v gh >/dev/null 2>&1 || die "gh not found"
gh auth status >/dev/null 2>&1 || die "gh auth token invalid"
[[ -f "${NOTES}" ]] || die "missing ${NOTES}"
git rev-parse -q --verify "refs/tags/${TAG}" >/dev/null || die "local tag ${TAG} missing"

wallet_assets=(
  "rigor-v${VERSION}-windows-x86_64.exe"
  "rigor-v${VERSION}-windows-x86_64.zip"
  "rigor-v${VERSION}-linux-x86_64.tar.gz"
)
miner_assets=(
  "rigor-v${VERSION}-miner-windows-x86_64.exe"
  "rigor-v${VERSION}-miner-windows-x86_64.zip"
  "rigor-v${VERSION}-miner-linux-x86_64.tar.gz"
)

rm -rf "${STAGE}"
mkdir -p "${STAGE}"
for name in "${wallet_assets[@]}"; do
  [[ -f "${WALLET}/${name}" ]] || die "missing ${WALLET}/${name}"
  [[ "${name}" != *alphaplus* ]] || die "forbidden alphaplus name: ${name}"
  cp -a "${WALLET}/${name}" "${STAGE}/${name}"
done
for name in "${miner_assets[@]}"; do
  [[ -f "${MINER}/${name}" ]] || die "missing ${MINER}/${name}"
  cp -a "${MINER}/${name}" "${STAGE}/${name}"
done

python3 - "${STAGE}" "${VERSION}" "${ROOT}" <<'PY'
import hashlib, json, sys
from datetime import date
from pathlib import Path

stage = Path(sys.argv[1])
ver = sys.argv[2]
root = Path(sys.argv[3])
sys.path.insert(0, str(root / "tools" / "release"))
from official_packaging import NAMING_FOLLOWS, PACKED_CLIENT_VERSION, macos_pending

files = [
    f"rigor-v{ver}-windows-x86_64.exe",
    f"rigor-v{ver}-windows-x86_64.zip",
    f"rigor-v{ver}-linux-x86_64.tar.gz",
    f"rigor-v{ver}-miner-windows-x86_64.exe",
    f"rigor-v{ver}-miner-windows-x86_64.zip",
    f"rigor-v{ver}-miner-linux-x86_64.tar.gz",
]
sha = {}
lines = []
for name in files:
    p = stage / name
    digest = hashlib.sha256(p.read_bytes()).hexdigest()
    sha[name] = digest
    lines.append(f"{digest}  {name}")
(stage / "SHA256SUMS").write_text("\n".join(lines) + "\n", encoding="utf-8")
pending = list(macos_pending(ver))
manifest = {
    "project": "RIGOR",
    "version": ver,
    "tag": f"v{ver}",
    "title": f"RIGOR v{ver}",
    "naming_follows": NAMING_FOLLOWS,
    "alphaplus_release": False,
    "packed_client_version": PACKED_CLIENT_VERSION,
    "refinement_proved": False,
    "LEVEL_5_CLAIMED": False,
    "CPP_VERIFIED": False,
    "wallet_package_includes_miner": False,
    "macos_pending": pending,
    "wsl_cannot_generate_macos_binaries": True,
    "present": files + ["SHA256SUMS"],
    "sha256": sha,
    "date": date.today().isoformat(),
}
(stage / f"release_v{ver}_manifest.json").write_text(
    json.dumps(manifest, indent=2) + "\n", encoding="utf-8"
)
print("GITHUB_SHA256SUMS=PASS")
PY

assets=(
  "${STAGE}/rigor-v${VERSION}-windows-x86_64.exe"
  "${STAGE}/rigor-v${VERSION}-windows-x86_64.zip"
  "${STAGE}/rigor-v${VERSION}-linux-x86_64.tar.gz"
  "${STAGE}/rigor-v${VERSION}-miner-windows-x86_64.exe"
  "${STAGE}/rigor-v${VERSION}-miner-windows-x86_64.zip"
  "${STAGE}/rigor-v${VERSION}-miner-linux-x86_64.tar.gz"
  "${STAGE}/SHA256SUMS"
  "${STAGE}/release_v${VERSION}_manifest.json"
)

exe="$(od -An -tx1 -N2 "${STAGE}/rigor-v${VERSION}-windows-x86_64.exe" | tr -d ' \n')"
[[ "${exe}" == "4d5a" ]] || die "wallet installer is not MZ"
# Never attach placeholder DMGs.
for stub in "${STAGE}/rigor-v${VERSION}-macos-x86_64.dmg" "${STAGE}/rigor-v${VERSION}-macos-arm64.dmg"; do
  [[ -e "${stub}" ]] && die "refusing stub/pending dmg ${stub}"
done

if gh release view "${TAG}" --repo "${REPO}" >/dev/null 2>&1; then
  echo "== release ${TAG} exists; replacing listed assets =="
  gh release upload "${TAG}" --repo "${REPO}" --clobber "${assets[@]}"
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
    "${assets[@]}"
fi

echo "GITHUB_RELEASE_CREATED=PASS"
echo "OK: https://github.com/${REPO}/releases/tag/${TAG}"
echo "MACOS_PENDING=rigor-v${VERSION}-macos-x86_64.dmg rigor-v${VERSION}-macos-arm64.dmg"

#!/usr/bin/env bash
# Publish RIGOR v0.1.2-alphaplus to github.com/rigorchain/rigorchain
#
# Two channels (do not mix them):
#   1) git push     — source on the current branch (usually main)
#   2) gh release   — Linux/Windows *stripped* installers as Release assets
#
# Linux packages come from scripts/alpha/build_alpha_linux.sh
# (copy + strip --strip-unneeded, then tar/deb).
# Do not upload unstripped debug ELFs (the ~360 MiB wallet tarball).
#
# RING (~/ring) is not a publish source. Do not retarget the v0.1.0-alpha script.
set -euo pipefail

VERSION="${RIGOR_ALPHA_VERSION:-0.1.2-alphaplus}"
TAG="v${VERSION}"
TITLE="RIGOR ${TAG}"
BRANCH="$(git rev-parse --abbrev-ref HEAD)"
REPO="rigorchain/rigorchain"
EXPECTED_ORIGIN_SUFFIX="github.com:rigorchain/rigorchain.git"
EXPECTED_ORIGIN_HTTPS="github.com/rigorchain/rigorchain.git"

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
export PATH="${HOME}/.local/bin:${PATH}"
cd "${ROOT}"

die() { echo "ERROR: $*" >&2; exit 1; }

case "${ROOT}" in
  */ring) die "RING is not a publish source. Run from ~/rigor-official-tree." ;;
esac

origin="$(git remote get-url origin 2>/dev/null || true)"
[[ "${origin}" == *"${EXPECTED_ORIGIN_SUFFIX}" || "${origin}" == *"${EXPECTED_ORIGIN_HTTPS}" ]] \
  || die "origin is not ${EXPECTED_ORIGIN_HTTPS} (got: ${origin})"

command -v gh >/dev/null 2>&1 || die "gh not found"
gh auth status >/dev/null 2>&1 || die "Run: gh auth login"

notes="${ROOT}/docs/release/RIGOR_ALPHAPLUS_RELEASE_NOTES.md"
[[ -f "${notes}" ]] || die "missing ${notes}"

linux_ap="${ROOT}/dist/alphaplus/linux"
win="${ROOT}/dist/alphaplus/windows"

assets=(
  "${linux_ap}/rigor-v${VERSION}-linux.tar.gz"
  "${win}/rigor-v${VERSION}-windows.exe"
  "${win}/rigor-v${VERSION}-windows.zip"
)
for f in "${assets[@]}"; do
  [[ -f "${f}" ]] || die "missing installer asset: ${f}"
done

# Refuse to publish an unstripped debug wallet pack (~360 MiB).
wallet_bytes="$(stat -c '%s' "${linux_ap}/rigor-v${VERSION}-linux.tar.gz")"
if (( wallet_bytes > 80*1024*1024 )); then
  die "Linux tarball is ${wallet_bytes} bytes — expected stripped ~24 MiB."
fi

RIGOR_ALPHA_VERSION="${VERSION}" bash "${ROOT}/scripts/alpha/verify_release_assets.sh"

sums="$(mktemp)"
{
  echo "# RIGOR ${TAG} GitHub Release assets"
  (cd "${linux_ap}" && sha256sum "rigor-v${VERSION}-linux.tar.gz")
  (cd "${win}" && sha256sum \
    "rigor-v${VERSION}-windows.exe" \
    "rigor-v${VERSION}-windows.zip")
} > "${sums}"
cp -a "${sums}" "${ROOT}/dist/alphaplus/SHA256SUMS"

echo "== git push origin ${BRANCH} =="
git push -u origin "HEAD:${BRANCH}"

if git rev-parse -q --verify "refs/tags/${TAG}" >/dev/null; then
  echo "Tag ${TAG} already exists locally."
else
  git tag -a "${TAG}" -m "${TITLE}"
fi
echo "== git push origin ${TAG} =="
git push origin "refs/tags/${TAG}"

if gh release view "${TAG}" --repo "${REPO}" >/dev/null 2>&1; then
  echo "== release ${TAG} exists; replacing assets =="
  gh release upload "${TAG}" --repo "${REPO}" --clobber \
    "${assets[@]}" \
    "${sums}#SHA256SUMS"
  gh release edit "${TAG}" --repo "${REPO}" \
    --title "${TITLE}" \
    --notes-file "${notes}" \
    --draft=false
else
  echo "== creating GitHub Release ${TAG} =="
  gh release create "${TAG}" \
    --repo "${REPO}" \
    --verify-tag \
    --title "${TITLE}" \
    --notes-file "${notes}" \
    "${assets[@]}" \
    "${sums}#SHA256SUMS"
fi

rm -f "${sums}"
echo "OK: https://github.com/${REPO}/releases/tag/${TAG}"

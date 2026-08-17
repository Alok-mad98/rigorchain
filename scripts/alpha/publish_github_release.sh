#!/usr/bin/env bash
# Historical Alpha1 naming. Publishes the prior GitHub release tag v0.1.0-alpha only.
# AlphaPlus (v0.1.1-alphaplus) uses publish_github_release_alphaplus.sh. Do not retarget this file.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
export PATH="${HOME}/.local/bin:${PATH}"
cd "${ROOT}"

if ! command -v gh >/dev/null 2>&1; then
  echo "ERROR: gh not found. Install to ~/.local/bin or PATH." >&2
  exit 1
fi

if ! gh auth status >/dev/null 2>&1; then
  echo "Run: gh auth login" >&2
  echo "Then re-run this script." >&2
  exit 1
fi

gh release view v0.1.0-alpha >/dev/null 2>&1 && EXISTS=1 || EXISTS=0
if [[ "${EXISTS}" -eq 0 ]]; then
  gh release create v0.1.0-alpha \
    --repo rigorchain/rigorchain \
    --target mainnet-release-candidate-001 \
    --verify-tag \
    --title "RIGOR v0.1.0-alpha" \
    --notes-file docs/release/RIGOR_ALPHA_RELEASE_NOTES.md \
    release/windows/rigor-wallet-0.1.0-alpha.exe \
    release/linux/rigor-wallet.AppImage \
    release/linux/rigor-wallet.deb \
    release/linux/rigor-alpha-linux.tar.gz \
    release/checksums/SHA256SUMS \
    release/windows/rigord.exe \
    release/windows/rigor-miner.exe
else
  gh release upload v0.1.0-alpha --clobber \
    release/windows/rigor-wallet-0.1.0-alpha.exe \
    release/linux/rigor-wallet.AppImage \
    release/linux/rigor-wallet.deb \
    release/linux/rigor-alpha-linux.tar.gz \
    release/checksums/SHA256SUMS \
    release/windows/rigord.exe \
    release/windows/rigor-miner.exe
fi

echo "OK: https://github.com/rigorchain/rigorchain/releases/tag/v0.1.0-alpha"

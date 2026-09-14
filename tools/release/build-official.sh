#!/usr/bin/env bash
# Frozen official wallet packaging flow (v0.1.3 GitHub Release asset names).
#
#   bash tools/release/build-official.sh
#
# Env:
#   RELEASE_VERSION   default: from core/src/clientversion.h (strip leading v)
#   CHANNEL           default: internal-mainnet-validation
#   SKIP_LINUX=1      skip native Linux rebuild (reuse staging)
#   SKIP_WINDOWS=1    skip mingw/NSIS (reuse staging)
#   FORCE=1           rebuild even if staging exists
#   SKIP_DEPENDS=1    default for Windows PE (depends already built)
#
# Does not push. Does not build macOS DMGs.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

if [[ "${SKIP_LINUX:-0}" != "1" ]]; then
  bash "${ROOT}/tools/release/stage-official-linux.sh"
else
  echo "[official] SKIP_LINUX=1"
fi
if [[ "${SKIP_WINDOWS:-0}" != "1" ]]; then
  bash "${ROOT}/tools/release/stage-official-windows.sh"
else
  echo "[official] SKIP_WINDOWS=1"
fi
bash "${ROOT}/tools/release/package-official.sh"
echo "BUILD_OFFICIAL=PASS"
echo "DO_NOT_PUSH=1"
echo "NEXT=attach audit/wallet_packaging/official/* to GitHub Release when you choose to publish"

#!/usr/bin/env bash
# Official miner packaging (parallel to wallet; never mixed into wallet assets).
#
#   bash tools/release/build-official-miner.sh
#
# Env:
#   RELEASE_VERSION   default: from core/src/clientversion.h
#   CHANNEL           default: internal-mainnet-validation
#   SKIP_LINUX=1      reuse staged Linux miner
#   SKIP_WINDOWS=1    reuse staged Windows miner NSIS/zip
#   SKIP_DEPENDS=1    default for Windows PE
#
# Does not push. Does not modify RFC / consensus / Coq / claim flags.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

if [[ "${SKIP_LINUX:-0}" != "1" ]]; then
  bash "${ROOT}/tools/release/stage-official-miner-linux.sh"
else
  echo "[official-miner] SKIP_LINUX=1"
fi
if [[ "${SKIP_WINDOWS:-0}" != "1" ]]; then
  bash "${ROOT}/tools/release/stage-official-miner-windows.sh"
else
  echo "[official-miner] SKIP_WINDOWS=1"
fi
bash "${ROOT}/tools/release/package-official-miner.sh"
echo "BUILD_OFFICIAL_MINER=PASS"
echo "DO_NOT_PUSH=1"
echo "NEXT=internal sim-test with audit/miner_packaging/official/; do not merge into wallet SHA256SUMS"

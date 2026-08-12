#!/usr/bin/env bash
# Continue Windows PE build after depends HOST=x86_64-w64-mingw32 finishes.
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
HOST=x86_64-w64-mingw32
CONFIG_SITE="${REPO_ROOT}/core/depends/${HOST}/share/config.site"
LOG="${REPO_ROOT}/.runtime/alpha-local-stack/mingw-pe-build.log"
mkdir -p "$(dirname "$LOG")"

if [[ ! -f "${CONFIG_SITE}" ]]; then
  echo "depends not ready: ${CONFIG_SITE}" >&2
  exit 1
fi

export SKIP_DEPENDS=1
bash "${REPO_ROOT}/scripts/alpha/build_alpha_windows_pe.sh" 2>&1 | tee "$LOG"
bash "${REPO_ROOT}/scripts/alpha/build_alpha_windows_nsis.sh" 2>&1 | tee -a "$LOG"
echo "DONE — see dist/alpha/windows/rigor-wallet-0.1.0-alpha.exe"

#!/usr/bin/env bash
# Cross-build Windows rigor-miner.exe and run the miner NSIS/zip packager.
# Does not rebuild the wallet PE set (SKIP_CORE=1).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"
# shellcheck source=official_packaging.inc.sh
source "${ROOT}/tools/release/official_packaging.inc.sh"
official_miner_init

SKIP_CORE=1 SKIP_DEPENDS="${SKIP_DEPENDS:-1}" \
  bash "${ROOT}/scripts/alpha/build_alpha_windows_pe.sh"

PE_STAGING="${ROOT}/dist/alpha/windows/staging"
[[ -f "${PE_STAGING}/rigor-miner.exe" ]] || {
  echo "FAIL: rigor-miner.exe missing after PE build" >&2
  exit 1
}
# Do not use grep -q here. Under pipefail, grep -q closes the pipe early and
# strings dies with SIGPIPE, which looks like a missing string.
lottery_hit="$(strings "${PE_STAGING}/rigor-miner.exe" | grep -F 'lottery-era GBT field %s is not a shipping template field' || true)"
[[ -n "${lottery_hit}" ]] || { echo "FAIL: rigor-miner.exe is stale (RFC-020 lottery string missing)" >&2; exit 1; }
rfc019_hit="$(strings "${PE_STAGING}/rigor-miner.exe" | grep -F 'is not RFC-019' || true)"
if [[ -n "${rfc019_hit}" ]]; then
  echo "FAIL: rigor-miner.exe still contains retired RFC-019 error text" >&2
  exit 1
fi

PRODUCT_VERSION="${RELEASE_VERSION}" bash "${ROOT}/scripts/alpha/package_windows_miner.sh"

ALPHA_OUT="${ROOT}/dist/alpha/windows"
[[ -f "${ALPHA_OUT}/${MINER_WIN_EXE_NAME}" ]] || {
  echo "FAIL: missing ${ALPHA_OUT}/${MINER_WIN_EXE_NAME}" >&2
  exit 1
}
[[ -f "${ALPHA_OUT}/${MINER_WIN_ZIP_NAME}" ]] || {
  echo "FAIL: missing ${ALPHA_OUT}/${MINER_WIN_ZIP_NAME}" >&2
  exit 1
}

mkdir -p "${MINER_STAGING}/windows"
cp -a "${ALPHA_OUT}/${MINER_WIN_EXE_NAME}" "${MINER_STAGING}/windows/"
cp -a "${ALPHA_OUT}/${MINER_WIN_ZIP_NAME}" "${MINER_STAGING}/windows/"
echo "STAGE_MINER_WINDOWS=PASS"
ls -la "${MINER_STAGING}/windows"

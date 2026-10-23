#!/usr/bin/env bash
# Stage Linux SOLO miner (minerd → rigor-miner). Wallet staging is untouched.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"
# shellcheck source=official_packaging.inc.sh
source "${ROOT}/tools/release/official_packaging.inc.sh"
official_miner_init

die() { echo "FAIL: $*" >&2; exit 1; }

MINER_SRC="${ROOT}/miner"
[[ -x "${MINER_SRC}/minerd" ]] || die "missing ${MINER_SRC}/minerd (cd miner && make)"
# Do not use grep -q here. Under pipefail, grep -q closes the pipe early and
# strings dies with SIGPIPE, which looks like a missing string.
lottery_hit="$(strings "${MINER_SRC}/minerd" | grep -F 'lottery-era GBT field %s is not a shipping template field' || true)"
[[ -n "${lottery_hit}" ]] || die "${MINER_SRC}/minerd is stale (still RFC-019 lottery string or unrebuild)"
rfc019_hit="$(strings "${MINER_SRC}/minerd" | grep -F 'is not RFC-019' || true)"
if [[ -n "${rfc019_hit}" ]]; then
  die "${MINER_SRC}/minerd still contains retired RFC-019 error text — rebuild"
fi

OUT="${MINER_STAGING}/linux"
rm -rf "${OUT}"
mkdir -p "${OUT}/bin"
cp -a "${MINER_SRC}/minerd" "${OUT}/bin/rigor-miner"
strip --strip-unneeded "${OUT}/bin/rigor-miner" 2>/dev/null || true
chmod +x "${OUT}/bin/rigor-miner"
ln -sfn rigor-miner "${OUT}/bin/minerd"
cp -a "${ROOT}/installer/linux/MINER_SOLO_README.txt" "${OUT}/README.txt"
cp -a "${ROOT}/installer/linux/rigor-miner.sh" "${OUT}/rigor-miner.sh"
chmod 755 "${OUT}/rigor-miner.sh"

echo "STAGE_MINER_LINUX=PASS"
ls -la "${OUT}/bin"

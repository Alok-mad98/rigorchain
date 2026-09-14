#!/usr/bin/env bash
# Stage and package the Windows SOLO miner + local dashboard.
# Does not modify the wallet installer. Wallet packages must stay miner-free.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
VERSION="${PRODUCT_VERSION:-0.2}"
PE_STAGING="${MINER_PE_STAGING:-${ROOT}/dist/alpha/windows/staging}"
OUT_DIR="${MINER_OUT_DIR:-${ROOT}/dist/alpha/windows}"
STAGE="${OUT_DIR}/miner-staging"

log() { echo "[package-windows-miner] $*"; }

rm -rf "${STAGE}"
mkdir -p "${STAGE}"

cp -a "${ROOT}/installer/windows/rigor-miner.cmd" "${STAGE}/rigor-miner.cmd"
cp -a "${ROOT}/installer/windows/load-rpc-auth.cmd" "${STAGE}/load-rpc-auth.cmd"
if [[ -f "${ROOT}/installer/windows/gen-rpc-auth.ps1" ]]; then
  cp -a "${ROOT}/installer/windows/gen-rpc-auth.ps1" "${STAGE}/gen-rpc-auth.ps1"
fi
bash "${ROOT}/scripts/alpha/stage_windows_miner_dashboard.sh" "${STAGE}"

OPTIONAL_NSH="${STAGE}/optional-binaries.nsh"
: > "${OPTIONAL_NSH}"
HAVE_MINER=0
if [[ -f "${PE_STAGING}/rigor-miner.exe" ]]; then
  cp -a "${PE_STAGING}/rigor-miner.exe" "${STAGE}/rigor-miner.exe"
  printf '  File "${STAGING_DIR}/rigor-miner.exe"\n' >> "${OPTIONAL_NSH}"
  HAVE_MINER=1
  log "included rigor-miner.exe"
else
  log "WARNING: ${PE_STAGING}/rigor-miner.exe missing; packaging scripts + dashboard only"
fi
if [[ -f "${PE_STAGING}/rigor-cli.exe" ]]; then
  cp -a "${PE_STAGING}/rigor-cli.exe" "${STAGE}/rigor-cli.exe"
  printf '  File "${STAGING_DIR}/rigor-cli.exe"\n' >> "${OPTIONAL_NSH}"
  log "included rigor-cli.exe (GBT preflight)"
fi

cat > "${STAGE}/README.txt" <<EOF
RIGOR Miner ${VERSION}

This package is the Windows SOLO miner. It is not the wallet installer.

1. Start Rigor Wallet / local rigord so RPC is on 127.0.0.1:8761
2. Run rigor-miner.cmd and pass your ORU payout address (rg1q…)
3. Run start-miner-dashboard.cmd and open http://127.0.0.1:18083/

The dashboard is read-only. It does not sign, spend, or mine.
Python 3 is required for the local dashboard server.
EOF

ZIP_OUT="${OUT_DIR}/rigor-v${VERSION}-miner-windows-x86_64.zip"
python3 - <<PY
import zipfile
from pathlib import Path
stage = Path("${STAGE}")
out = Path("${ZIP_OUT}")
out.parent.mkdir(parents=True, exist_ok=True)
prefix = "rigor-miner"
with zipfile.ZipFile(out, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as zf:
    for path in stage.rglob("*"):
        if not path.is_file():
            continue
        if path.name == "optional-binaries.nsh":
            continue
        zf.write(path, f"{prefix}/{path.relative_to(stage).as_posix()}")
print("zip written", out, out.stat().st_size)
PY
log "Zip: ${ZIP_OUT}"

NSIS_OUT="${OUT_DIR}/rigor-v${VERSION}-miner-windows-x86_64.exe"
if command -v makensis >/dev/null 2>&1; then
  makensis \
    -DREPO_ROOT="${ROOT}" \
    -DSTAGING_DIR="${STAGE}" \
    -DPRODUCT_VERSION="${VERSION}" \
    -DOUTFILE="${NSIS_OUT}" \
    "${ROOT}/installer/windows/rigor_miner_installer.nsi"
  log "NSIS: ${NSIS_OUT}"
else
  log "makensis not found; zip package is still available"
fi

(
  cd "${OUT_DIR}"
  files=()
  [[ -f "rigor-v${VERSION}-miner-windows-x86_64.zip" ]] && files+=("rigor-v${VERSION}-miner-windows-x86_64.zip")
  [[ -f "rigor-v${VERSION}-miner-windows-x86_64.exe" ]] && files+=("rigor-v${VERSION}-miner-windows-x86_64.exe")
  if ((${#files[@]})); then
    sha256sum "${files[@]}" > "rigor-v${VERSION}-miner-SHA256SUMS.txt"
    cat "rigor-v${VERSION}-miner-SHA256SUMS.txt"
  fi
)

if [[ "${HAVE_MINER}" != "1" ]]; then
  log "MINER_PE_MISSING=1"
  exit 0
fi
log "MINER_PACKAGE=OK"

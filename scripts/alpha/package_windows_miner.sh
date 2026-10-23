#!/usr/bin/env bash
# Stage and package the Windows SOLO miner.
# Dashboard sources stay in the repository and are not part of this package.
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

python3 - "${ROOT}/installer/windows/rigor-miner.cmd" "${STAGE}/rigor-miner.cmd" <<'PY'
import pathlib, sys
src, dest = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2])
data = src.read_bytes().replace(b"\r\n", b"\n").replace(b"\n", b"\r\n")
dest.write_bytes(data)
PY
python3 - "${ROOT}/installer/windows/MINER_SOLO_README.txt" "${STAGE}/README.txt" <<'PY'
import pathlib, sys
src, dest = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2])
data = src.read_bytes().replace(b"\r\n", b"\n").replace(b"\n", b"\r\n")
dest.write_bytes(data)
PY

OPTIONAL_NSH="${STAGE}/optional-binaries.nsh"
: > "${OPTIONAL_NSH}"
HAVE_MINER=0
if [[ -f "${PE_STAGING}/rigor-miner.exe" ]]; then
  cp -a "${PE_STAGING}/rigor-miner.exe" "${STAGE}/rigor-miner.exe"
  printf '  File "${STAGING_DIR}/rigor-miner.exe"\n' >> "${OPTIONAL_NSH}"
  HAVE_MINER=1
  log "included rigor-miner.exe"
else
  log "WARNING: ${PE_STAGING}/rigor-miner.exe missing"
fi
# rigor-cli.exe is not required. rigor-miner.exe has its own RPC client.

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

#!/usr/bin/env bash
# Build Windows AlphaPlus NSIS installer → dist/alpha/windows/rigor-v${VERSION}-windows.exe
# Historical Alpha1 naming before AlphaPlus release identity migration (script path still scripts/alpha/).
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
STAGING="${REPO_ROOT}/dist/alpha/windows/staging"
OUT_DIR="${REPO_ROOT}/dist/alpha/windows"
VERSION="${RIGOR_ALPHA_VERSION:-0.1.2-alphaplus}"
OUTFILE="${OUT_DIR}/rigor-v${VERSION}-windows.exe"
NSI="${REPO_ROOT}/installer/windows/rigor_alpha_installer.nsi"

log() { printf '[alpha-windows] %s\n' "$*"; }

mkdir -p "${STAGING}" "${OUT_DIR}"

is_pe() {
  local f="$1"
  [[ -f "$f" ]] || return 1
  local magic
  magic="$(od -An -tx1 -N2 "$f" | tr -d ' \n')"
  [[ "$magic" == "4d5a" ]]
}

for req in rigor-wallet.exe rigord.exe rigor-miner.exe rigor-cli.exe; do
  if ! is_pe "${STAGING}/${req}"; then
    log "ERROR: ${STAGING}/${req} missing or not PE (MZ)."
    log "Run: bash scripts/alpha/build_alpha_windows_pe.sh"
    log "Or place real PE files in ${STAGING}/"
    cat > "${OUT_DIR}/WINDOWS_PE_BUILD_REQUIRED.txt" <<EOF
PE staging incomplete for ${req}.
Build with: bash scripts/alpha/build_alpha_windows_pe.sh
Or native Windows: powershell scripts/ci/windows_native_build_pipeline.ps1
Then re-run: bash scripts/alpha/build_alpha_windows_nsis.sh
EOF
    exit 1
  fi
done

# Datadir + conf helpers for installer
cat > "${STAGING}/DATADIR.txt" <<'EOF'
RIGOR AlphaPlus data directory (Windows)
====================================

Default datadir: %APPDATA%\RIGOR
Config file:     %APPDATA%\RIGOR\rigor.conf

Examples:
  Wallet / node (mainnet):  rigor-wallet.exe

Production artifacts default exclusively to AlphaPlus mainnet.
Testnet/regtest are available only in development builds.
RPC credentials are generated on first start (rpc-auth.conf). Do not use user/pass.

Uninstall does NOT delete %APPDATA%\RIGOR (protects wallet funds).

Note: older Alpha builds mistakenly used %APPDATA%\Bitcoin\bitcoin.conf.
If you already have chain data there, either move it to RIGOR or start with
  rigor-wallet.exe -datadir="%APPDATA%\Bitcoin"

Persistence: with solomining=1 each new tip is flushed to disk. Still prefer a normal
wallet quit (wait for shutdown dialog). Ending the process in Task Manager can lose
unflushed state on older builds.

MAINNET DIFFICULTY RESET (genesis nBits 0x1e010000): old datadirs are incompatible.
Run these in Windows CMD (not WSL), then recreate rigor.conf with solomining=1:
  rmdir /s /q "%APPDATA%\RIGOR\blocks"
  rmdir /s /q "%APPDATA%\RIGOR\chainstate"
Miner log must show: GBT nBits ~0x1e01xxxx (BAD if 0x207fffff)
If it shows 0x207fffff you are still on the old easy chain/binary.
EOF

cat > "${STAGING}/README-ALPHA.txt" <<EOF
Rigor Wallet AlphaPlus ${VERSION}
Unsigned alpha — not a Guix-signed mainnet release.

Windows users: double-click rigor-v${VERSION}-windows.exe (or the
same file inside the zip). That opens the setup wizard. After install,
use the Desktop / Start Menu "Rigor Wallet" shortcut (rigor-wallet.exe).

Data directory: %APPDATA%\\RIGOR
Config file:    %APPDATA%\\RIGOR\\rigor.conf
Do NOT use %APPDATA%\\Bitcoin (Bitcoin Core / old v0.1.0-alpha).
If the wallet still does not open, rename leftover rigor.conf / bitcoin.conf
that contain rpcuser=user, then check %APPDATA%\\RIGOR\\debug.log.
EOF

# Console launchers (keeps window open; CRLF required for Windows cmd.exe).
# Missing launchers must fail the production packager (NEW-4: no /nonfatal skip).
stage_cmd() {
  local src="$1" dst="$2"
  if [[ ! -f "$src" ]]; then
    log "ERROR: required launcher missing: $src"
    exit 1
  fi
  python3 - "$src" "$dst" <<'PY'
import sys
from pathlib import Path
src, dst = Path(sys.argv[1]), Path(sys.argv[2])
text = src.read_text(encoding="utf-8")
text = text.replace("\r\n", "\n").replace("\n", "\r\n")
dst.write_bytes(text.encode("utf-8"))
print("staged", dst.name, "CRLF bytes", dst.stat().st_size)
PY
}

stage_cmd "${REPO_ROOT}/installer/windows/rigor-miner.cmd" "${STAGING}/rigor-miner.cmd"
stage_cmd "${REPO_ROOT}/installer/windows/rigor-cli.cmd" "${STAGING}/rigor-cli.cmd"
stage_cmd "${REPO_ROOT}/installer/windows/load-rpc-auth.cmd" "${STAGING}/load-rpc-auth.cmd"

cat > "${STAGING}/rigor.conf.template" <<'EOF'
# RIGOR Alpha template — copy to %APPDATA%\RIGOR\rigor.conf
server=1
txindex=1
listen=1
# RPC credentials are generated on first mainnet start (rpc-auth.conf).
# Do not ship default rpcuser/rpcpassword.
rpcallowip=127.0.0.1
# Allow getblocktemplate with zero peers (local Alpha solo mining).
# Also forces a full chain flush after every new tip so Task Manager kills do not rewind to genesis.
solomining=1
# Mainnet RPC listens on 8761 by default (override with -rpcport=).
# Always quit via the wallet window (wait for "Rigor Core is shutting down") — do not End Task.
EOF

required_staged=(
  rigor-wallet.exe rigord.exe rigor-cli.exe rigor-miner.exe
  README-ALPHA.txt DATADIR.txt rigor.conf.template
)
for req in "${required_staged[@]}"; do
  if [[ ! -e "${STAGING}/${req}" ]]; then
    log "ERROR: required staging file missing: ${STAGING}/${req}"
    exit 1
  fi
done

# Strip debug symbols so the installed wallet is a normal-sized PE (AV / first-launch).
if command -v x86_64-w64-mingw32-strip >/dev/null 2>&1; then
  for pe in rigor-wallet.exe rigord.exe rigor-cli.exe rigor-miner.exe; do
    x86_64-w64-mingw32-strip --strip-unneeded "${STAGING}/${pe}" || true
  done
  log "Stripped staging PEs"
fi

if ! command -v makensis >/dev/null 2>&1; then
  log "ERROR: makensis not found"
  exit 1
fi

nsis_args=(
  -V2
  -DREPO_ROOT="${REPO_ROOT}"
  -DSTAGING_DIR="${STAGING}"
  -DOUTFILE="${OUTFILE}"
  -DPRODUCT_VERSION="${VERSION}"
)
if [[ -d "${STAGING}/qt-runtime" ]]; then
  nsis_args+=(-DINCLUDE_QT_RUNTIME=1)
  log "Including qt-runtime in installer (dynamic Qt)"
else
  log "No qt-runtime dir — static Qt wallet (depends). qt-runtime not required."
fi

makensis "${nsis_args[@]}" "${NSI}"
log "Built ${OUTFILE}"
ls -la "${OUTFILE}"
# Installer itself must be PE
magic="$(od -An -tx1 -N2 "${OUTFILE}" | tr -d ' \n')"
[[ "$magic" == "4d5a" ]] || { log "ERROR: outfile not PE"; exit 1; }
log "Installer MZ OK"

# User zip: the setup wizard (double-click after unzip).
ZIP_OUT="${OUT_DIR}/rigor-v${VERSION}-windows.zip"
rm -f "${ZIP_OUT}"
python3 - <<PY
import zipfile, pathlib, sys
installer = pathlib.Path("${OUTFILE}")
readme = pathlib.Path("${STAGING}/README-ALPHA.txt")
out = pathlib.Path("${ZIP_OUT}")
inner = "rigor-v${VERSION}-windows.exe"
if not installer.is_file():
    print("ERROR: NSIS installer missing", installer, file=sys.stderr)
    raise SystemExit(1)
with zipfile.ZipFile(out, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as zf:
    zf.write(installer, inner)
    if readme.is_file():
        zf.write(readme, "README.txt")
print("zip written", out, out.stat().st_size)
PY
log "Zip: ${ZIP_OUT}"

# Integrity manifests for Windows copy verification
(
  cd "${OUT_DIR}"
  sha256sum "rigor-v${VERSION}-windows.exe" "rigor-v${VERSION}-windows.zip" > SHA256SUMS.txt
  cat SHA256SUMS.txt
)
rm -f "${OUT_DIR}/WINDOWS_PE_BUILD_REQUIRED.txt"
# Drop prior 0.1.1 installer names so operators do not pick the old file.
rm -f "${OUT_DIR}/rigor-v0.1.1-alphaplus-windows.exe" \
  "${OUT_DIR}/rigor-v0.1.1-alphaplus-windows.zip" \
  "${OUT_DIR}/rigor-wallet-0.1.1-alphaplus.exe" \
  "${OUT_DIR}/rigor-wallet-0.1.1-alphaplus-portable.zip"

# Publish layout used by scripts/alpha/publish_github_release_alphaplus.sh
AP="${REPO_ROOT}/dist/alphaplus/windows"
mkdir -p "${AP}"
cp -a "${OUTFILE}" "${AP}/rigor-v${VERSION}-windows.exe"
cp -a "${ZIP_OUT}" "${AP}/rigor-v${VERSION}-windows.zip"
rm -f "${AP}/rigor-v0.1.1-alphaplus-windows.exe" \
  "${AP}/rigor-v0.1.1-alphaplus-windows.zip" \
  "${AP}/rigor-wallet-0.1.1-alphaplus.exe" \
  "${AP}/rigor-wallet-0.1.1-alphaplus-portable.zip"
cp -a "${STAGING}/rigord.exe" "${AP}/rigord.exe"
cp -a "${STAGING}/rigor-miner.exe" "${AP}/rigor-miner.exe"
cp -a "${STAGING}/rigor-cli.exe" "${AP}/rigor-cli.exe"
(
  cd "${AP}"
  sha256sum \
    "rigor-v${VERSION}-windows.exe" \
    "rigor-v${VERSION}-windows.zip" \
    rigord.exe rigor-miner.exe rigor-cli.exe \
    > SHA256SUMS
)
log "Staged AlphaPlus Windows assets under ${AP}"

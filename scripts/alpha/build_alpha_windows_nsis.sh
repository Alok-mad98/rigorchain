#!/usr/bin/env bash
# Build Windows Alpha NSIS installer → dist/alpha/windows/rigor-wallet-0.1.0-alpha.exe
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
STAGING="${REPO_ROOT}/dist/alpha/windows/staging"
OUT_DIR="${REPO_ROOT}/dist/alpha/windows"
VERSION="${RIGOR_ALPHA_VERSION:-0.1.0-alpha.1}"
OUTFILE="${OUT_DIR}/rigor-wallet-0.1.0-alpha.exe"
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

for req in rigor-wallet.exe rigord.exe rigor-miner.exe; do
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
RIGOR Alpha data directory (Windows)
====================================

Default datadir: %APPDATA%\RIGOR
Config file:     %APPDATA%\RIGOR\rigor.conf

Examples:
  Wallet / node (mainnet):  rigor-wallet.exe
  Regtest:                  rigor-wallet.exe -regtest
  Testnet:                  rigor-wallet.exe -testnet

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
RIGOR Wallet Alpha ${VERSION}
Binaries: rigor-wallet.exe, rigord.exe, rigor-miner.exe, rigor-cli.exe
Launchers: rigor-miner.cmd, rigor-cli.cmd (keep console open)
Unsigned alpha — not a Guix-signed mainnet release.

Config: %APPDATA%\\RIGOR\\rigor.conf  (server=1, solomining=1 for local solo)
Miner: rigor-miner.cmd — prompts for ORU payout address; RPC port 8761
Peers may be empty on Alpha until public seeds are live; solomining=1 allows GBT anyway.
EOF

# Console miner launcher (keeps window open; CRLF required for Windows cmd.exe)
python3 - "${REPO_ROOT}/installer/windows/rigor-miner.cmd" "${STAGING}/rigor-miner.cmd" <<'PY'
import sys
from pathlib import Path
src, dst = Path(sys.argv[1]), Path(sys.argv[2])
text = src.read_text(encoding="utf-8")
text = text.replace("\r\n", "\n").replace("\n", "\r\n")
dst.write_bytes(text.encode("ascii", errors="strict"))
print("staged rigor-miner.cmd CRLF bytes", dst.stat().st_size)
PY

python3 - "${REPO_ROOT}/installer/windows/rigor-cli.cmd" "${STAGING}/rigor-cli.cmd" <<'PY'
import sys
from pathlib import Path
src, dst = Path(sys.argv[1]), Path(sys.argv[2])
text = src.read_text(encoding="utf-8")
text = text.replace("\r\n", "\n").replace("\n", "\r\n")
dst.write_bytes(text.encode("ascii", errors="strict"))
print("staged rigor-cli.cmd CRLF bytes", dst.stat().st_size)
PY

cat > "${STAGING}/rigor.conf.template" <<'EOF'
# RIGOR Alpha template — copy to %APPDATA%\RIGOR\rigor.conf
server=1
txindex=1
listen=1
rpcuser=user
rpcpassword=pass
rpcallowip=127.0.0.1
# Allow getblocktemplate with zero peers (local Alpha solo mining).
# Also forces a full chain flush after every new tip so Task Manager kills do not rewind to genesis.
solomining=1
# Mainnet RPC listens on 8761 by default (override with -rpcport=).
# Always quit via the wallet window (wait for "Rigor Core is shutting down") — do not End Task.
EOF

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
  log "Including qt-runtime in installer"
fi

makensis "${nsis_args[@]}" "${NSI}"
log "Built ${OUTFILE}"
ls -la "${OUTFILE}"
# Installer itself must be PE
magic="$(od -An -tx1 -N2 "${OUTFILE}" | tr -d ' \n')"
[[ "$magic" == "4d5a" ]] || { log "ERROR: outfile not PE"; exit 1; }
log "Installer MZ OK"

# Portable zip fallback (no NSIS) for operators who hit "NSIS Error"
ZIP_OUT="${OUT_DIR}/rigor-wallet-0.1.0-alpha-portable.zip"
rm -f "${ZIP_OUT}"
(
  cd "${STAGING}"
  zip -9 -q "${ZIP_OUT}" rigor-wallet.exe rigord.exe rigor-cli.exe rigor-miner.exe \
    rigor-miner.cmd rigor-cli.cmd README-ALPHA.txt DATADIR.txt rigor.conf.template 2>/dev/null \
    || python3 - <<PY
import zipfile, pathlib
staging = pathlib.Path("${STAGING}")
out = pathlib.Path("${ZIP_OUT}")
names = [
    "rigor-wallet.exe", "rigord.exe", "rigor-cli.exe", "rigor-miner.exe",
    "rigor-miner.cmd", "rigor-cli.cmd",
    "README-ALPHA.txt", "DATADIR.txt", "rigor.conf.template",
]
with zipfile.ZipFile(out, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as zf:
    for n in names:
        p = staging / n
        if p.is_file():
            zf.write(p, n)
print("zip written", out, out.stat().st_size)
PY
)
log "Portable zip: ${ZIP_OUT}"

# Integrity manifests for Windows copy verification
(
  cd "${OUT_DIR}"
  sha256sum "rigor-wallet-0.1.0-alpha.exe" "rigor-wallet-0.1.0-alpha-portable.zip" > SHA256SUMS.txt
  cat SHA256SUMS.txt
)
rm -f "${OUT_DIR}/WINDOWS_PE_BUILD_REQUIRED.txt"

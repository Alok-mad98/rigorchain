#!/usr/bin/env bash
# — Installer / package local validation (Linux)
set -euo pipefail

ALPHA_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${ALPHA_DIR}/../.." && pwd)"
OUT="${REPO_ROOT}/.runtime/alpha-local-stack"
INSTALLERS="${REPO_ROOT}/dist/alpha/linux/installers"
STAGE="${OUT}/install-stage"
mkdir -p "${OUT}" "${STAGE}"

pass=0; fail=0
check() {
  local id="$1" ok="$2" note="${3:-}"
  if [[ "$ok" == "1" ]]; then echo "PASS  $id  $note"; pass=$((pass+1)); else echo "FAIL  $id  $note"; fail=$((fail+1)); fi
}

DEB="$(ls "${INSTALLERS}"/rigor-alpha_*.deb 2>/dev/null | head -1 || true)"
APP="$(ls "${INSTALLERS}"/rigor-wallet.AppImage 2>/dev/null | head -1 || true)"
TAR="$(ls "${INSTALLERS}"/rigor-alpha-*-linux-*.tar.gz 2>/dev/null | head -1 || true)"

check "deb_artifact" "$([[ -n "$DEB" ]] && echo 1 || echo 0)" "${DEB:-missing}"
check "appimage_artifact" "$([[ -n "$APP" ]] && echo 1 || echo 0)" "${APP:-missing}"
check "tarball_artifact" "$([[ -n "$TAR" ]] && echo 1 || echo 0)" "${TAR:-missing}"

# Clean extract / "install" to staging (no root required)
rm -rf "${STAGE}"
mkdir -p "${STAGE}"
if [[ -n "$TAR" ]]; then
  tar -C "${STAGE}" -xzf "$TAR"
  check "extract_tarball" "$([[ -x "${STAGE}/bin/rigor-wallet" && -x "${STAGE}/bin/rigord" && -x "${STAGE}/bin/rigor-miner" ]] && echo 1 || echo 0)"
else
  check "extract_tarball" 0
fi

# Deb extract without dpkg -i (user may lack sudo)
if [[ -n "$DEB" ]]; then
  rm -rf "${STAGE}/deb"
  mkdir -p "${STAGE}/deb"
  dpkg-deb -x "$DEB" "${STAGE}/deb"
  check "extract_deb" "$([[ -x "${STAGE}/deb/opt/rigor/bin/rigor-wallet" ]] && echo 1 || echo 0)"
else
  check "extract_deb" 0
fi

# Wallet / node binary startup smoke (no GUI display required for --version / -version)
if [[ -x "${STAGE}/bin/rigord" ]]; then
  "${STAGE}/bin/rigord" -version >/dev/null 2>&1 && check "node_startup_version" 1 || check "node_startup_version" 0
else
  check "node_startup_version" 0
fi
if [[ -x "${STAGE}/bin/rigor-wallet" ]]; then
  "${STAGE}/bin/rigor-wallet" -version >/dev/null 2>&1 && check "wallet_startup_version" 1 || check "wallet_startup_version" 0
else
  check "wallet_startup_version" 0
fi

# Datadir init smoke
DATADIR="${STAGE}/datadir-smoke"
rm -rf "${DATADIR}"
mkdir -p "${DATADIR}"
if [[ -x "${STAGE}/bin/rigord" ]]; then
  timeout 15 "${STAGE}/bin/rigord" -regtest -datadir="${DATADIR}" -server=0 -daemon=0 -printtoconsole=0 >/dev/null 2>&1 || true
  # If daemon didn't stay, still check genesis files created after short run with -daemon
  timeout 20 "${STAGE}/bin/rigord" -regtest -datadir="${DATADIR}" -daemon
  sleep 2
  "${STAGE}/bin/rigor-cli" -regtest -datadir="${DATADIR}" -rpcwait getblockchaininfo >/dev/null 2>&1 \
    && check "datadir_init" 1 "regtest datadir + RPC" \
    || check "datadir_init" 0
  "${STAGE}/bin/rigor-cli" -regtest -datadir="${DATADIR}" stop >/dev/null 2>&1 || true
else
  check "datadir_init" 0
fi

# Uninstall = remove staging
rm -rf "${STAGE}"
check "uninstall_cleanup" 1 "staging removed"

# Windows
WIN_EXE="$(ls "${REPO_ROOT}/dist/alpha/windows/installers/"*.exe 2>/dev/null | head -1 || true)"
WIN_GAP="${REPO_ROOT}/dist/alpha/windows/installers/WINDOWS_PE_BUILD_REQUIRED.txt"
if [[ -n "$WIN_EXE" ]]; then
  check "windows_installer" 1 "$WIN_EXE"
elif [[ -f "$WIN_GAP" ]]; then
  check "windows_installer" 0 "PE build required on Windows host (documented)"
else
  check "windows_installer" 0 "no windows artifacts"
fi

python3 -c "
import json
from pathlib import Path
Path('${OUT}/installer_validation.json').write_text(json.dumps({
  'task': '',
  'pass': ${pass},
  'fail': ${fail},
  'verdict': 'PASS' if ${fail} == 0 else ('PARTIAL' if ${pass} > 0 else 'FAIL'),
}, indent=2)+'\n')
"
echo "Installer validation: pass=${pass} fail=${fail}"
# Soft: windows may fail on Linux host — do not hard-fail entire script if only windows missing
if [[ "${fail}" -gt 0 ]]; then
  # Allow PARTIAL when only windows_installer failed
  if [[ "${fail}" -eq 1 ]] && [[ ! -f "$WIN_EXE" ]]; then
    echo "PARTIAL — Linux packages OK; Windows PE pending"
    exit 0
  fi
  exit 1
fi

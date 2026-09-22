#!/usr/bin/env bash
# Internal testnet wallet packager (Windows NSIS + Linux tree).
# Opens -testnet on existing v0.2 wallet objects. Does not edit consensus,
# RFC, Coq, protocol parameters, or packed CLIENT_VERSION.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

VER="${RELEASE_VERSION:-0.2}"
OUT="${ROOT}/dist/testnet_packaging"
LINUX_NAME="rigor-wallet-v${VER}-testnet-linux"
WIN_NAME="rigor-wallet-v${VER}-testnet-windows.exe"
LINUX_SRC="${ROOT}/core/build-linux-wallet"
LINUX_BUILD="${ROOT}/core/build-linux-testnet-wallet"
MINGW_SRC="${ROOT}/core/build-mingw-alpha"
MINGW_BUILD="${ROOT}/core/build-mingw-testnet-wallet"
WIN_STAGE="${OUT}/windows/staging"

die() { echo "FAIL: $*" >&2; exit 1; }
log() { printf '[testnet-wallet] %s\n' "$*"; }

INTREE_O_HOLD=""
INTREE_O_FILES=(
  libbitcoin_common_a-chainparams.o
  libbitcoin_util_a-chainparamsbase.o
  util/libbitcoin_util_a-system.o
)

park_intree_objects() {
  local src="${ROOT}/core/src"
  INTREE_O_HOLD="$(mktemp -d)"
  local f
  for f in "${INTREE_O_FILES[@]}"; do
    if [[ -f "${src}/${f}" ]]; then
      mkdir -p "${INTREE_O_HOLD}/$(dirname "${f}")"
      mv "${src}/${f}" "${INTREE_O_HOLD}/${f}"
    fi
  done
}

restore_intree_objects() {
  local src="${ROOT}/core/src"
  local f
  [[ -n "${INTREE_O_HOLD}" && -d "${INTREE_O_HOLD}" ]] || return 0
  for f in "${INTREE_O_FILES[@]}"; do
    if [[ -f "${INTREE_O_HOLD}/${f}" ]]; then
      mv "${INTREE_O_HOLD}/${f}" "${src}/${f}"
    fi
  done
  rmdir "${INTREE_O_HOLD}/util" 2>/dev/null || true
  rmdir "${INTREE_O_HOLD}" 2>/dev/null || true
  INTREE_O_HOLD=""
}

unlock_testnet_build() {
  local build="$1"
  local cfg="${build}/src/config/rigor-config.h"
  [[ -f "${cfg}" ]] || die "missing ${cfg}"
  python3 - "${cfg}" "${build}" <<'PY'
from pathlib import Path
import os, sys
cfg = Path(sys.argv[1])
root = Path(sys.argv[2])
stamp_ns = cfg.stat().st_mtime_ns
cfg.write_text(
    cfg.read_text(encoding="utf-8").replace(
        "#define RIGOR_PRODUCTION_MAINNET 1",
        "/* #undef RIGOR_PRODUCTION_MAINNET */"),
    encoding="utf-8")
os.utime(cfg, ns=(stamp_ns, stamp_ns))
needle = " -DRIGOR_PRODUCTION_MAINNET"
for path in [root / "Makefile", root / "src" / "Makefile", root / "doc" / "man" / "Makefile"]:
    if not path.is_file():
        continue
    stamp_ns = path.stat().st_mtime_ns
    path.write_text(path.read_text(encoding="utf-8").replace(needle, ""), encoding="utf-8")
    os.utime(path, ns=(stamp_ns, stamp_ns))
PY
  rm -f \
    "${build}/src/libbitcoin_common_a-chainparams.o" \
    "${build}/src/libbitcoin_util_a-chainparamsbase.o" \
    "${build}/src/util/libbitcoin_util_a-system.o" \
    "${build}/src/libbitcoin_common.a" \
    "${build}/src/libbitcoin_util.a" \
    "${build}/src/rigord" "${build}/src/rigord.exe" \
    "${build}/src/rigor-cli" "${build}/src/rigor-cli.exe" \
    "${build}/src/qt/rigor-qt" "${build}/src/qt/rigor-qt.exe"
}

relink_testnet_bins() {
  local build="$1"
  shift
  park_intree_objects
  trap restore_intree_objects EXIT
  make -j"$(nproc)" -C "${build}/src" \
    -o Makefile -o ../Makefile -o ../config.status -o config/stamp-h1 \
    "$@"
  restore_intree_objects
  trap - EXIT
}

write_wrapper() {
  local dest="$1"
  local inner="$2"
  cat > "${dest}" <<EOF
#!/bin/sh
here=\$(dirname "\$(readlink -f "\$0")")
root=\$(dirname "\$here")
mkdir -p "\$root/data"
exec "\$here/internal/${inner}" -testnet -server -datadir="\$root/data" -rpcallowip=127.0.0.1 -addnode=seed1.testnet.rigor.network:18760 -addnode=seed2.testnet.rigor.network:18760 "\$@"
EOF
  chmod +x "${dest}"
}

mkdir -p "${OUT}" "${WIN_STAGE}"

[[ -x "${LINUX_SRC}/src/qt/rigor-qt" ]] || die "missing Linux GUI at ${LINUX_SRC}"
[[ -f "${MINGW_SRC}/src/qt/rigor-qt.exe" ]] || die "missing Windows GUI at ${MINGW_SRC}"

if [[ "${FORCE:-0}" == "1" || ! -x "${LINUX_BUILD}/src/qt/rigor-qt" ]]; then
  if [[ "${FORCE:-0}" == "1" || ! -d "${LINUX_BUILD}/src" ]]; then
    log "copy Linux wallet tree → ${LINUX_BUILD}"
    rm -rf "${LINUX_BUILD}"
    cp -a "${LINUX_SRC}" "${LINUX_BUILD}"
  fi
  unlock_testnet_build "${LINUX_BUILD}"
  log "relink Linux bins with -testnet enabled"
  relink_testnet_bins "${LINUX_BUILD}" rigord rigor-cli qt/rigor-qt
fi
[[ -x "${LINUX_BUILD}/src/qt/rigor-qt" ]] || die "Linux testnet GUI missing after relink"

if [[ "${FORCE:-0}" == "1" || ! -f "${MINGW_BUILD}/src/qt/rigor-qt.exe" ]]; then
  if [[ "${FORCE:-0}" == "1" || ! -d "${MINGW_BUILD}/src" ]]; then
    log "copy MinGW wallet tree → ${MINGW_BUILD}"
    rm -rf "${MINGW_BUILD}"
    cp -a "${MINGW_SRC}" "${MINGW_BUILD}"
  fi
  unlock_testnet_build "${MINGW_BUILD}"
  log "relink Windows PEs with -testnet enabled"
  relink_testnet_bins "${MINGW_BUILD}" rigord.exe rigor-cli.exe qt/rigor-qt.exe
fi
[[ -f "${MINGW_BUILD}/src/qt/rigor-qt.exe" ]] || die "Windows testnet GUI missing after relink"

log "stage Linux ${LINUX_NAME}"
linux_dir="${OUT}/${LINUX_NAME}"
rm -rf "${linux_dir}"
mkdir -p "${linux_dir}/bin/internal" "${linux_dir}/config" \
  "${linux_dir}/share/applications" \
  "${linux_dir}/share/pixmaps" \
  "${linux_dir}/share/icons/hicolor/256x256/apps" \
  "${linux_dir}/share/icons/hicolor/512x512/apps" \
  "${linux_dir}/share/icons/hicolor/scalable/apps"
cp -a "${LINUX_BUILD}/src/rigord" "${linux_dir}/bin/internal/rigord"
cp -a "${LINUX_BUILD}/src/rigor-cli" "${linux_dir}/bin/internal/rigor-cli"
cp -a "${LINUX_BUILD}/src/qt/rigor-qt" "${linux_dir}/bin/internal/rigor-wallet"
cp -a "${LINUX_BUILD}/src/qt/rigor-qt" "${linux_dir}/bin/internal/rigor-qt"
strip --strip-unneeded "${linux_dir}/bin/internal/"* || true
write_wrapper "${linux_dir}/bin/rigord" rigord
write_wrapper "${linux_dir}/bin/rigor-cli" rigor-cli
write_wrapper "${linux_dir}/bin/rigor-wallet" rigor-wallet
write_wrapper "${linux_dir}/bin/rigor-qt" rigor-qt

ICON256="${ROOT}/core/share/icons/rigor/rigor-wallet-256.png"
ICON512="${ROOT}/core/share/icons/rigor/rigor-wallet-512.png"
ICONSVG="${ROOT}/core/src/qt/res/icons/rigor.svg"
[[ -f "${ICON256}" ]] || die "missing ${ICON256}"
[[ -f "${ICON512}" ]] || die "missing ${ICON512}"
[[ -f "${ICONSVG}" ]] || die "missing ${ICONSVG}"
cp -a "${ICON256}" "${linux_dir}/share/icons/hicolor/256x256/apps/rigor-wallet-testnet.png"
cp -a "${ICON512}" "${linux_dir}/share/icons/hicolor/512x512/apps/rigor-wallet-testnet.png"
cp -a "${ICONSVG}" "${linux_dir}/share/icons/hicolor/scalable/apps/rigor-wallet-testnet.svg"
cp -a "${ICON256}" "${linux_dir}/share/pixmaps/rigor-wallet-testnet.png"

cat > "${linux_dir}/README.txt" <<EOF
Rigor Wallet v${VER} Linux testnet — Ubuntu 22.04 x86_64

Install on the server:
  sudo ./install.sh
  rigord
  rigor-cli createwallet server
  rigor-cli getnewaddress

Expect trg1q... and Overview "RIGOR Testnet".
Datadir: <this-tree>/data   (chain files in data/testnet3)
This package changes TESTNET consensus. Delete data/testnet3 (or the
whole data folder) after install and sync from genesis. Do not reuse
an older TESTNET chain. Mainnet ASERT is not changed.
RPC: 18761 localhost
Official TESTNET packages add the public P2P peers. You do not open RPC to any public host.
rigor.conf: <this-tree>/data/rigor.conf
Do not run bin/internal/* directly.
EOF
cat > "${linux_dir}/TESTNET.txt" <<'EOF'
This is the RIGOR testnet wallet.
Install: sudo ./install.sh
Start:   rigord
Address: trg1q...
Overview: Network: RIGOR Testnet
After install, delete the old testnet3 datadir and sync from genesis.
Mainnet ASERT is not changed.
EOF
cat > "${linux_dir}/install.sh" <<'EOF'
#!/bin/sh
# Direct install for a dedicated Ubuntu 22.04 x86_64 testnet server.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
PREFIX="${PREFIX:-/opt/rigor-wallet-testnet}"
BINDIR="${BINDIR:-/usr/local/bin}"
ICON_PNG="${PREFIX}/share/icons/hicolor/256x256/apps/rigor-wallet-testnet.png"

if [ "$(id -u)" -ne 0 ]; then
  echo "Run: sudo ./install.sh" >&2
  exit 1
fi

if command -v apt-get >/dev/null 2>&1; then
  DEBIAN_FRONTEND=noninteractive apt-get install -y \
    libevent-2.1-7 libzmq5 libsqlite3-0 libsodium23 \
    libqt5core5a libqt5gui5 libqt5widgets5 libqt5network5 libqt5dbus5 \
    libqrencode4 >/dev/null
fi

mkdir -p "${PREFIX}"
cp -a "${ROOT}/bin" "${ROOT}/config" "${ROOT}/share" \
  "${ROOT}/README.txt" "${ROOT}/TESTNET.txt" "${PREFIX}/"
for cmd in rigord rigor-cli rigor-wallet rigor-qt; do
  printf '%s\n' '#!/bin/sh' "exec \"${PREFIX}/bin/${cmd}\" \"\$@\"" > "${BINDIR}/${cmd}"
  chmod 755 "${BINDIR}/${cmd}"
done

write_desktop() {
  dest="$1"
  cat > "${dest}" <<DESK
[Desktop Entry]
Version=1.0
Type=Application
Name=Rigor Wallet Testnet
Comment=RIGOR Testnet — addresses start with trg1q
Exec=${BINDIR}/rigor-wallet
Icon=${ICON_PNG}
Terminal=false
Categories=Finance;Network;
StartupNotify=true
DESK
  chmod 644 "${dest}"
}

mkdir -p \
  /usr/share/applications \
  /usr/share/pixmaps \
  /usr/share/icons/hicolor/256x256/apps \
  /usr/share/icons/hicolor/512x512/apps \
  /usr/share/icons/hicolor/scalable/apps
cp -a "${PREFIX}/share/icons/hicolor/256x256/apps/rigor-wallet-testnet.png" \
  /usr/share/icons/hicolor/256x256/apps/rigor-wallet-testnet.png
cp -a "${PREFIX}/share/icons/hicolor/512x512/apps/rigor-wallet-testnet.png" \
  /usr/share/icons/hicolor/512x512/apps/rigor-wallet-testnet.png
cp -a "${PREFIX}/share/icons/hicolor/scalable/apps/rigor-wallet-testnet.svg" \
  /usr/share/icons/hicolor/scalable/apps/rigor-wallet-testnet.svg
cp -a "${PREFIX}/share/pixmaps/rigor-wallet-testnet.png" \
  /usr/share/pixmaps/rigor-wallet-testnet.png
write_desktop /usr/share/applications/rigor-wallet-testnet.desktop
write_desktop "${PREFIX}/share/applications/rigor-wallet-testnet.desktop"
write_desktop "${PREFIX}/share/rigor-wallet.desktop"

if [ -n "${SUDO_USER:-}" ] && [ "${SUDO_USER}" != "root" ]; then
  SUDO_HOME=$(getent passwd "${SUDO_USER}" | cut -d: -f6)
  DESKTOP=""
  if command -v sudo >/dev/null 2>&1 && command -v xdg-user-dir >/dev/null 2>&1; then
    DESKTOP=$(sudo -u "${SUDO_USER}" xdg-user-dir DESKTOP 2>/dev/null || true)
  fi
  if [ -z "${DESKTOP}" ] || [ ! -d "${DESKTOP}" ]; then
    if [ -d "${SUDO_HOME}/Desktop" ]; then
      DESKTOP="${SUDO_HOME}/Desktop"
    elif [ -d "${SUDO_HOME}/桌面" ]; then
      DESKTOP="${SUDO_HOME}/桌面"
    fi
  fi
  if [ -n "${DESKTOP}" ] && [ -d "${DESKTOP}" ]; then
    write_desktop "${DESKTOP}/rigor-wallet-testnet.desktop"
    chown "${SUDO_USER}:" "${DESKTOP}/rigor-wallet-testnet.desktop" || true
  fi
fi

if command -v gtk-update-icon-cache >/dev/null 2>&1; then
  gtk-update-icon-cache -f /usr/share/icons/hicolor >/dev/null 2>&1 || true
fi
if command -v update-desktop-database >/dev/null 2>&1; then
  update-desktop-database /usr/share/applications >/dev/null 2>&1 || true
fi

echo "Installed ${PREFIX}"
echo "Start node:  rigord"
echo "New address: rigor-cli createwallet server && rigor-cli getnewaddress"
echo "GUI:         rigor-wallet   (needs a desktop or VNC)"
echo "Icon:        ${ICON_PNG}"
EOF
chmod +x "${linux_dir}/install.sh"

cat > "${linux_dir}/config/rigor.conf.template" <<'EOF'
testnet=1
server=1
listen=1
txindex=1
rpcallowip=127.0.0.1
addnode=seed1.testnet.rigor.network:18760
addnode=seed2.testnet.rigor.network:18760
EOF

cat > "${linux_dir}/share/applications/rigor-wallet-testnet.desktop" <<EOF
[Desktop Entry]
Version=1.0
Type=Application
Name=Rigor Wallet Testnet
Comment=RIGOR Testnet — addresses start with trg1q
Exec=rigor-wallet
Icon=rigor-wallet-testnet
Terminal=false
Categories=Finance;Network;
StartupNotify=true
EOF
cp -a "${linux_dir}/share/applications/rigor-wallet-testnet.desktop" \
  "${linux_dir}/share/rigor-wallet.desktop"

[[ -s "${linux_dir}/share/icons/hicolor/256x256/apps/rigor-wallet-testnet.png" ]] \
  || die "Linux testnet icon PNG missing"
grep -q 'Icon=rigor-wallet-testnet' \
  "${linux_dir}/share/applications/rigor-wallet-testnet.desktop" \
  || die "Linux desktop entry missing Icon=rigor-wallet-testnet"
grep -q 'hicolor/256x256/apps/rigor-wallet-testnet.png' "${linux_dir}/install.sh" \
  || die "install.sh does not install the desktop icon"

"${linux_dir}/bin/internal/rigor-cli" -version | head -6 | tee "${linux_dir}/share/version.txt"
ldd "${linux_dir}/bin/internal/rigord" > "${linux_dir}/share/ldd-rigord.txt" || true
ldd "${linux_dir}/bin/internal/rigor-wallet" > "${linux_dir}/share/ldd-rigor-wallet.txt" || true

tar --sort=name --mtime="@0" --owner=0 --group=0 --numeric-owner \
  -C "${OUT}" -czf "${OUT}/${LINUX_NAME}.tar.gz" "${LINUX_NAME}"
log "PACKAGED=${OUT}/${LINUX_NAME}"
log "PACKAGED=${OUT}/${LINUX_NAME}.tar.gz"

log "stage Windows PEs"
mkdir -p "${WIN_STAGE}/internal"
cp -a "${MINGW_BUILD}/src/qt/rigor-qt.exe" "${WIN_STAGE}/internal/rigor-qt.exe"
cp -a "${MINGW_BUILD}/src/rigord.exe" "${WIN_STAGE}/rigord.exe"
cp -a "${MINGW_BUILD}/src/rigor-cli.exe" "${WIN_STAGE}/rigor-cli.exe"
ICO="${ROOT}/core/src/qt/res/icons/rigor_testnet.ico"
[[ -f "${ICO}" ]] || die "missing ${ICO}"
cp -a "${ICO}" "${WIN_STAGE}/rigor.ico"
command -v x86_64-w64-mingw32-windres >/dev/null || die "missing x86_64-w64-mingw32-windres"
x86_64-w64-mingw32-windres \
  -I "${ROOT}/core/src/qt/res/icons" \
  -O coff \
  -o "${WIN_STAGE}/testnet-wallet-launcher.res" \
  "${ROOT}/tools/release/testnet-wallet-launcher.rc"
x86_64-w64-mingw32-gcc -O2 -s -mwindows \
  -o "${WIN_STAGE}/rigor-wallet.exe" \
  "${ROOT}/tools/release/testnet-wallet-launcher.c" \
  "${WIN_STAGE}/testnet-wallet-launcher.res"
python3 - "${WIN_STAGE}/rigor-wallet.exe" <<'PY' || die "rigor-wallet.exe has no GROUP_ICON"
import struct, sys
p = sys.argv[1]
data = open(p, "rb").read()
e_lfanew = struct.unpack_from("<I", data, 0x3C)[0]
magic = struct.unpack_from("<H", data, e_lfanew + 24)[0]
num_sections = struct.unpack_from("<H", data, e_lfanew + 6)[0]
opt_size = struct.unpack_from("<H", data, e_lfanew + 20)[0]
opt_off = e_lfanew + 24
dd_off = opt_off + (112 if magic == 0x20B else 96)
rva, sz = struct.unpack_from("<II", data, dd_off + 16)
if not rva or not sz:
    raise SystemExit("no resource directory")
sec_off = opt_off + opt_size
roff = None
for i in range(num_sections):
    vsz, va, rsz, raw = struct.unpack_from("<IIII", data, sec_off + i * 40 + 8)
    if va <= rva < va + max(vsz, rsz):
        roff = raw + (rva - va)
        break
if roff is None:
    raise SystemExit("resource RVA not mapped")
named, ids = struct.unpack_from("<HH", data, roff + 12)
types = []
for e in range(named + ids):
    name, _off = struct.unpack_from("<II", data, roff + 16 + e * 8)
    if name & 0x80000000 == 0:
        types.append(name)
if 14 not in types:
    raise SystemExit("GROUP_ICON (14) missing; types=%s" % types)
print("LAUNCHER_ICON=PASS types=%s" % types)
PY
x86_64-w64-mingw32-strip --strip-unneeded \
  "${WIN_STAGE}/internal/rigor-qt.exe" "${WIN_STAGE}/rigord.exe" "${WIN_STAGE}/rigor-cli.exe" || true

cat > "${WIN_STAGE}/README.txt" <<EOF
Rigor Wallet v${VER} Testnet (Windows x86_64)

ALWAYS start rigor-wallet.exe or start-wallet.cmd.
Do not open files inside internal\\.

Testnet check:
  Window title includes [testnet]
  New receive address starts with trg1q
  If you see rg1q, you are on mainnet. Close and use the launcher.

Datadir: folder named data next to rigor-wallet.exe (not the registry path from an old install).
This package changes TESTNET consensus. Delete the old data\\testnet3
folder (or the whole data folder) after install and sync from genesis.
Do not reuse an older TESTNET chain. Mainnet ASERT is not changed.
RPC port: 18761 (localhost). Put rigor.conf in that data folder.
Do not put rigor.conf next to rigor-wallet.exe. The wallet ignores that file.
Address HRP: trg
Packed CLIENT_VERSION remains 25.0.0.
EOF
cat > "${WIN_STAGE}/DATADIR.txt" <<'EOF'
RIGOR testnet data directory (Windows)
Always: <this-install>\data
Chain files: <this-install>\data\testnet3
rigor.conf: <this-install>\data\rigor.conf
Wrong: <this-install>\rigor.conf  (next to the exe; ignored)
This follows the wallet program. It does not reuse a previous install path from the registry.
Start rigor-wallet.exe. Addresses must start with trg1q.
EOF
cat > "${WIN_STAGE}/rigor.conf.template" <<'EOF'
testnet=1
server=1
listen=1
txindex=1
rpcallowip=127.0.0.1
addnode=seed1.testnet.rigor.network:18760
addnode=seed2.testnet.rigor.network:18760
# Set these yourself, then use the same values in the miner command:
# rpcuser=YOUR_NAME
# rpcpassword=YOUR_PASSWORD
EOF
cat > "${WIN_STAGE}/start-wallet.cmd" <<'EOF'
@echo off
cd /d "%~dp0"
start "" rigor-wallet.exe
EOF
cat > "${WIN_STAGE}/rigor-cli-testnet.cmd" <<'EOF'
@echo off
cd /d "%~dp0"
rigor-cli.exe -testnet -datadir="%~dp0data" -addnode=seed1.testnet.rigor.network:18760 -addnode=seed2.testnet.rigor.network:18760 %*
EOF

command -v makensis >/dev/null || die "makensis not found"
makensis -V2 \
  -DREPO_ROOT="${ROOT}" \
  -DSTAGING_DIR="${WIN_STAGE}" \
  -DOUTFILE="${OUT}/${WIN_NAME}" \
  -DPRODUCT_VERSION="${VER}" \
  "${ROOT}/installer/windows/rigor_testnet_installer.nsi"

magic="$(od -An -tx1 -N2 "${OUT}/${WIN_NAME}" | tr -d ' \n')"
[[ "${magic}" == "4d5a" ]] || die "Windows installer is not MZ"
log "PACKAGED=${OUT}/${WIN_NAME}"
ls -lh "${OUT}/${WIN_NAME}" "${OUT}/${LINUX_NAME}.tar.gz"

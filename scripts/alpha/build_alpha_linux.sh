#!/usr/bin/env bash
# Linux AlphaPlus package (deb + AppImage/portable) — RIGOR v0.1.1-alphaplus
# Historical Alpha1 naming before AlphaPlus release identity migration (path still scripts/alpha/).
# No consensus / RFC / RES model changes.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
CORE="${REPO_ROOT}/core"
SRC="${CORE}/src"
OUT="${REPO_ROOT}/dist/alpha/linux"
VERSION="${RIGOR_ALPHA_VERSION:-0.1.1-alphaplus}"
ARCH=amd64

log() { printf '[alpha-linux] %s\n' "$*"; }

need() {
  local p="$1"
  if [[ ! -x "$p" ]]; then
    log "ERROR: missing executable: $p"
    log "Build first: cd core && ./configure --enable-production-mainnet && make -j\$(nproc) rigord rigor-cli rigor-wallet rigor-qt"
    log "Development builds may omit --enable-production-mainnet (keeps testnet/regtest)."
    log "Also build Linux minerd (see miner path below)."
    exit 1
  fi
}

need "${SRC}/rigord"
need "${SRC}/rigor-cli"
need "${SRC}/qt/rigor-qt"

# Official-tree miner lives in miner/; cpuminer/ is the historical path.
MINERD="${RIGOR_MINERD:-}"
if [[ -z "${MINERD}" ]]; then
  for cand in \
    "${REPO_ROOT}/cpuminer/minerd" \
    "${REPO_ROOT}/miner/minerd" \
    "${REPO_ROOT}/.runtime/miner-linux/minerd"
  do
    if [[ -x "${cand}" ]] && file "${cand}" | grep -q 'ELF'; then
      MINERD="${cand}"
      break
    fi
  done
fi
if [[ -z "${MINERD}" || ! -x "${MINERD}" ]]; then
  log "ERROR: missing Linux ELF minerd"
  log "Build Linux miner in a copy (do not reconfigure miner/ if it is MinGW):"
  log "  rm -rf .runtime/miner-linux && cp -a miner .runtime/miner-linux"
  log "  cd .runtime/miner-linux && ./autogen.sh && ./configure && make -j\$(nproc)"
  exit 1
fi
log "minerd: ${MINERD}"

rm -rf "${OUT}"
mkdir -p "${OUT}/bin" "${OUT}/config" "${OUT}/installers" "${OUT}/share"

# Product names required by alpha brief (strip for distributable size)
cp -a "${SRC}/qt/rigor-qt" "${OUT}/bin/rigor-wallet"
cp -a "${SRC}/rigord" "${OUT}/bin/rigord"
cp -a "${SRC}/rigor-cli" "${OUT}/bin/rigor-cli"
cp -a "${MINERD}" "${OUT}/bin/rigor-miner"
cp -a "${SRC}/qt/rigor-qt" "${OUT}/bin/rigor-qt"
ln -sfn rigor-miner "${OUT}/bin/minerd"
for b in rigor-wallet rigor-qt rigord rigor-cli rigor-miner; do
  strip --strip-unneeded "${OUT}/bin/${b}" 2>/dev/null || true
done

cat > "${OUT}/config/rigor-alpha.conf" <<'EOF'
# RIGOR AlphaPlus — local / regtest-style template
# For local alpha stack use contrib/alpha/start_local_stack.sh
server=1
txindex=1
listen=1
fallbackfee=0.0001
EOF

cat > "${OUT}/README-ALPHA.txt" <<EOF
RIGOR AlphaPlus ${VERSION} (Linux amd64)
========================================

Binaries (this directory):
  bin/rigor-wallet   GUI wallet (rigor-qt)
  bin/rigord         Full node
  bin/rigor-cli      CLI
  bin/rigor-miner    Solo GBT miner (cpuminer/minerd)

Quick start:
  bin/rigor-wallet          (GUI; datadir ~/.rigor)
  bin/rigord                (node)
  bin/rigor-cli             (CLI)
  bin/rigor-miner           (solo GBT miner)

Default data directory is ~/.rigor (not ~/.bitcoin).
Config file is ~/.rigor/rigor.conf (bitcoin.conf is still read if rigor.conf is absent).

No consensus parameters changed in this packaging step.
EOF

# Portable tarball
TAR="${OUT}/installers/rigor-alphaplus-${VERSION}-linux-${ARCH}.tar.gz"
tar -C "${OUT}" -czf "${TAR}" bin config README-ALPHA.txt
log "Tarball: ${TAR}"

# .deb
DEB_ROOT="${OUT}/installers/deb-root"
rm -rf "${DEB_ROOT}"
mkdir -p "${DEB_ROOT}/DEBIAN" \
  "${DEB_ROOT}/opt/rigor/bin" \
  "${DEB_ROOT}/usr/share/applications" \
  "${DEB_ROOT}/usr/share/icons/hicolor/256x256/apps" \
  "${DEB_ROOT}/etc/rigor"

cp -a "${OUT}/bin/." "${DEB_ROOT}/opt/rigor/bin/"
cp -a "${OUT}/config/rigor-alpha.conf" "${DEB_ROOT}/etc/rigor/rigor.conf.template"

ICON_SRC="${REPO_ROOT}/resources/branding/rigor-wallet.png"
[[ -f "${ICON_SRC}" ]] || ICON_SRC="${REPO_ROOT}/core/share/icons/rigor/rigor-wallet.png"
cp -a "${ICON_SRC}" "${DEB_ROOT}/usr/share/icons/hicolor/256x256/apps/rigor-wallet.png"

cat > "${DEB_ROOT}/usr/share/applications/rigor-wallet.desktop" <<'EOF'
[Desktop Entry]
Name=RIGOR Wallet
Comment=RIGOR AlphaPlus Wallet
Exec=/opt/rigor/bin/rigor-wallet
Path=/opt/rigor/bin
Icon=rigor-wallet
Terminal=false
Type=Application
Categories=Finance;Network;
EOF

cat > "${DEB_ROOT}/DEBIAN/control" <<EOF
Package: rigor-alphaplus
Version: ${VERSION}
Section: net
Priority: optional
Architecture: ${ARCH}
Maintainer: RIGOR Protocol <ops@rigorchain.com>
Description: RIGOR AlphaPlus (node, wallet, miner)
 Locally distributable AlphaPlus package. Not a signed mainnet release.
 Depends on system Qt/Boost libraries matching the build host.
EOF

DEB_FILE="${OUT}/installers/rigor-alphaplus_${VERSION}_${ARCH}.deb"
dpkg-deb --build "${DEB_ROOT}" "${DEB_FILE}"
log "Deb: ${DEB_FILE}"

# AppImage / portable AppDir
export DIST_ROOT="${REPO_ROOT}/dist"
# Reuse existing AppImage builder against staged naming
APP_OUT="${OUT}/installers"
APPDIR="${APP_OUT}/rigor-wallet.AppDir"
rm -rf "${APPDIR}"
mkdir -p "${APPDIR}/usr/bin" "${APPDIR}/usr/share/applications" "${APPDIR}/usr/share/icons/hicolor/512x512/apps"
cp -a "${OUT}/bin/rigor-wallet" "${APPDIR}/usr/bin/rigor-wallet"
cp -a "${OUT}/bin/rigord" "${APPDIR}/usr/bin/rigord"
cp -a "${OUT}/bin/rigor-miner" "${APPDIR}/usr/bin/rigor-miner"
cp -a "${DEB_ROOT}/usr/share/applications/rigor-wallet.desktop" "${APPDIR}/rigor-wallet.desktop"
cp -a "${ICON_SRC}" "${APPDIR}/rigor-wallet.png"
cp -a "${ICON_SRC}" "${APPDIR}/usr/share/icons/hicolor/512x512/apps/rigor-wallet.png"
ln -sfn rigor-wallet.png "${APPDIR}/.DirIcon"
cat > "${APPDIR}/AppRun" <<'EOF'
#!/usr/bin/env bash
HERE="$(dirname "$(readlink -f "$0")")"
export PATH="${HERE}/usr/bin:${PATH}"
exec "${HERE}/usr/bin/rigor-wallet" "$@"
EOF
chmod 755 "${APPDIR}/AppRun"

APPIMAGE="${APP_OUT}/rigor-wallet.AppImage"
if command -v appimagetool >/dev/null 2>&1; then
  ARCH=x86_64 appimagetool "${APPDIR}" "${APPIMAGE}"
  log "AppImage: ${APPIMAGE}"
elif [[ -x "${REPO_ROOT}/.tools/appimagetool-x86_64.AppImage" ]]; then
  ARCH=x86_64 "${REPO_ROOT}/.tools/appimagetool-x86_64.AppImage" "${APPDIR}" "${APPIMAGE}"
  log "AppImage: ${APPIMAGE}"
else
  TAR_APP="${APP_OUT}/rigor-wallet-x86_64.AppDir.tar.gz"
  tar -C "${APP_OUT}" -czf "${TAR_APP}" rigor-wallet.AppDir
  log "appimagetool missing — shipping AppDir tarball only (no fake 263-byte AppImage)"
  rm -f "${APPIMAGE}"
fi
# Always keep the AppDir tarball for operators without fuse/appimagetool
if [[ ! -f "${APP_OUT}/rigor-wallet-x86_64.AppDir.tar.gz" ]]; then
  tar -C "${APP_OUT}" -czf "${APP_OUT}/rigor-wallet-x86_64.AppDir.tar.gz" rigor-wallet.AppDir
fi
if [[ -f "${APPIMAGE}" ]] && [[ "$(stat -c '%s' "${APPIMAGE}")" -lt 1048576 ]]; then
  log "ERROR: AppImage is a stub ($(stat -c '%s' "${APPIMAGE}") bytes). Refusing to ship it."
  rm -f "${APPIMAGE}"
  exit 1
fi

# Manifest
MANIFEST_OUT="${OUT}/installers/MANIFEST.json" VERSION="${VERSION}" python3 - <<'PY'
import hashlib, json, os
from pathlib import Path
root = Path(os.environ["MANIFEST_OUT"]).parent
files = {}
for p in sorted(root.rglob("*")):
    if p.is_file() and p.name != "MANIFEST.json":
        h = hashlib.sha256(p.read_bytes()).hexdigest()
        files[str(p.relative_to(root))] = {"sha256": h, "bytes": p.stat().st_size}
manifest = {
    "task": "TASK-ALPHAPLUS-RELEASE-IDENTITY-MIGRATION-001",
    "version": os.environ["VERSION"],
    "platform": "linux-amd64",
    "product_names": ["rigor-wallet", "rigord", "rigor-miner"],
    "files": files,
}
Path(os.environ["MANIFEST_OUT"]).write_text(json.dumps(manifest, indent=2) + "\n")
print("MANIFEST:", os.environ["MANIFEST_OUT"])
PY

# First-alpha user layout: stripped tar/deb/AppImage under release/linux/
REL="${REPO_ROOT}/release/linux"
mkdir -p "${REL}"
cp -a "${TAR}" "${REL}/rigor-alpha-linux.tar.gz"
cp -a "${DEB_FILE}" "${REL}/rigor-wallet.deb"
if [[ -f "${APPIMAGE}" ]]; then
  cp -a "${APPIMAGE}" "${REL}/rigor-wallet.AppImage"
else
  rm -f "${REL}/rigor-wallet.AppImage"
fi
if [[ -f "${APP_OUT}/rigor-wallet-x86_64.AppDir.tar.gz" ]]; then
  cp -a "${APP_OUT}/rigor-wallet-x86_64.AppDir.tar.gz" "${REL}/rigor-wallet-x86_64.AppDir.tar.gz"
fi
log "Staged first-alpha names under ${REL}"

# AlphaPlus dist names: overwrite any unstripped debug tarballs
AP="${REPO_ROOT}/dist/alphaplus/linux"
mkdir -p "${AP}"
cp -a "${TAR}" "${AP}/rigor-v0.1.1-alphaplus-linux.tar.gz"
cp -a "${TAR}" "${AP}/rigor-wallet-0.1.1-alphaplus.tar.gz"
if [[ -f "${DEB_FILE}" ]]; then
  cp -a "${DEB_FILE}" "${AP}/rigor-v0.1.1-alphaplus-linux.deb"
fi
core_tmp="$(mktemp -d)"
mkdir -p "${core_tmp}/bin"
cp -a "${OUT}/bin/rigord" "${OUT}/bin/rigor-cli" "${core_tmp}/bin/"
tar -C "${core_tmp}" -czf "${AP}/rigor-core-0.1.1-alphaplus.tar.gz" bin
rm -rf "${core_tmp}"
(
  cd "${AP}"
  sha256sum \
    rigor-v0.1.1-alphaplus-linux.tar.gz \
    rigor-wallet-0.1.1-alphaplus.tar.gz \
    rigor-core-0.1.1-alphaplus.tar.gz \
    > SHA256SUMS
  if [[ -f rigor-v0.1.1-alphaplus-linux.deb ]]; then
    sha256sum rigor-v0.1.1-alphaplus-linux.deb >> SHA256SUMS
  fi
)
log "Staged stripped AlphaPlus tarballs under ${AP}"

log "DONE — AlphaPlus Linux package under ${OUT}"
ls -la "${OUT}/bin" "${OUT}/installers" "${REL}"

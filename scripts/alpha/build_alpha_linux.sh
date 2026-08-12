#!/usr/bin/env bash
# TASK-MAINNET-PRODUCT-ALPHA-001 — Linux alpha package (deb + AppImage/portable)
# No consensus / RFC / RES model changes.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
CORE="${REPO_ROOT}/core"
SRC="${CORE}/src"
OUT="${REPO_ROOT}/dist/alpha/linux"
VERSION="${RIGOR_ALPHA_VERSION:-0.1.0-alpha.1}"
ARCH=amd64

log() { printf '[alpha-linux] %s\n' "$*"; }

need() {
  local p="$1"
  if [[ ! -x "$p" ]]; then
    log "ERROR: missing executable: $p"
    log "Build first: cd core && ./configure && make -j\$(nproc) rigord rigor-cli rigor-wallet rigor-qt"
    log "Also: cd cpuminer && make"
    exit 1
  fi
}

need "${SRC}/rigord"
need "${SRC}/rigor-cli"
need "${SRC}/qt/rigor-qt"
need "${REPO_ROOT}/cpuminer/minerd"

rm -rf "${OUT}"
mkdir -p "${OUT}/bin" "${OUT}/config" "${OUT}/installers" "${OUT}/share"

# Product names required by alpha brief (strip for distributable size)
cp -a "${SRC}/qt/rigor-qt" "${OUT}/bin/rigor-wallet"
cp -a "${SRC}/rigord" "${OUT}/bin/rigord"
cp -a "${SRC}/rigor-cli" "${OUT}/bin/rigor-cli"
cp -a "${REPO_ROOT}/cpuminer/minerd" "${OUT}/bin/rigor-miner"
cp -a "${SRC}/qt/rigor-qt" "${OUT}/bin/rigor-qt"
ln -sfn rigor-miner "${OUT}/bin/minerd"
for b in rigor-wallet rigor-qt rigord rigor-cli rigor-miner; do
  strip --strip-unneeded "${OUT}/bin/${b}" 2>/dev/null || true
done

cat > "${OUT}/config/rigor-alpha.conf" <<'EOF'
# RIGOR Product Alpha — local / regtest-style template
# For local alpha stack use contrib/alpha/start_local_stack.sh
server=1
txindex=1
listen=1
fallbackfee=0.0001
EOF

cat > "${OUT}/README-ALPHA.txt" <<EOF
RIGOR Product Alpha ${VERSION} (Linux amd64)
============================================

Binaries (this directory):
  bin/rigor-wallet   GUI wallet (rigor-qt)
  bin/rigord         Full node
  bin/rigor-cli      CLI
  bin/rigor-miner    Solo GBT miner (cpuminer/minerd)

Quick start (local alpha stack):
  contrib/alpha/start_local_stack.sh

No consensus parameters changed in this packaging step.
EOF

# Portable tarball
TAR="${OUT}/installers/rigor-alpha-${VERSION}-linux-${ARCH}.tar.gz"
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
Comment=RIGOR Product Alpha Wallet
Exec=/opt/rigor/bin/rigor-wallet
Icon=rigor-wallet
Terminal=false
Type=Application
Categories=Finance;Network;
EOF

cat > "${DEB_ROOT}/DEBIAN/control" <<EOF
Package: rigor-alpha
Version: ${VERSION}
Section: net
Priority: optional
Architecture: ${ARCH}
Maintainer: RIGOR Protocol <ops@rigorchain.com>
Description: RIGOR Product Alpha (node, wallet, miner)
 Locally distributable alpha package. Not a signed mainnet release.
 Depends on system Qt/Boost libraries matching the build host.
EOF

DEB_FILE="${OUT}/installers/rigor-alpha_${VERSION}_${ARCH}.deb"
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
  cat > "${APPIMAGE}" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
SELF="$(readlink -f "$0")"
DIR="$(dirname "$SELF")"
APPDIR="${DIR}/rigor-wallet.AppDir"
if [[ ! -x "${APPDIR}/AppRun" ]]; then
  tar -C "${DIR}" -xzf "${DIR}/rigor-wallet-x86_64.AppDir.tar.gz"
fi
exec "${APPDIR}/AppRun" "$@"
EOF
  chmod 755 "${APPIMAGE}"
  log "Portable AppImage launcher (appimagetool missing): ${APPIMAGE}"
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
    "task": "TASK-MAINNET-PRODUCT-ALPHA-001",
    "version": os.environ["VERSION"],
    "platform": "linux-amd64",
    "product_names": ["rigor-wallet", "rigord", "rigor-miner"],
    "files": files,
}
Path(os.environ["MANIFEST_OUT"]).write_text(json.dumps(manifest, indent=2) + "\n")
print("MANIFEST:", os.environ["MANIFEST_OUT"])
PY

log "DONE — alpha Linux package under ${OUT}"
ls -la "${OUT}/bin" "${OUT}/installers"

#!/usr/bin/env bash
# Build a real Rigor Wallet .dmg on GitHub-hosted macOS (not WSL).
# Wallet + node + CLI only. Does not ship a miner.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
CORE="${ROOT}/core"
VERSION="${RIGOR_RELEASE_VERSION:-0.2}"
ARCH="$(uname -m)"
case "${ARCH}" in
  arm64) TARGET="macos-arm64" ;;
  x86_64) TARGET="macos-x86_64" ;;
  *) echo "FAIL: unsupported arch ${ARCH}" >&2; exit 1 ;;
esac
OUT="${ROOT}/dist/release/v${VERSION}/${TARGET}"
mkdir -p "${OUT}"

echo "MACOS_WALLET_DMG=START"
echo "TARGET=${TARGET}"
echo "ARCH=${ARCH}"
echo "GIT_COMMIT=$(git -C "${ROOT}" rev-parse HEAD 2>/dev/null || echo unknown)"

# macOS 15 system Python is PEP 668 managed. macdeployqtplus needs ds_store
# and mac_alias. Use a venv; do not pip --user or --break-system-packages.
VENV="${RIGOR_MACOS_DMG_VENV:-${ROOT}/.venv-macos-dmg}"
if [[ ! -x "${VENV}/bin/python3" ]]; then
  python3 -m venv "${VENV}"
  "${VENV}/bin/pip" install --upgrade pip
  "${VENV}/bin/pip" install ds_store mac_alias
fi
export PATH="${VENV}/bin:${PATH}"
export PYTHON="${VENV}/bin/python3"
"${PYTHON}" -c "import ds_store, mac_alias"

QT5_PREFIX="$(brew --prefix qt@5 2>/dev/null || true)"
SODIUM_PREFIX="$(brew --prefix libsodium 2>/dev/null || true)"
BOOST_PREFIX="$(brew --prefix boost 2>/dev/null || true)"
if [[ -n "${QT5_PREFIX}" ]]; then
  export PATH="${QT5_PREFIX}/bin:${PATH}"
  export PKG_CONFIG_PATH="${QT5_PREFIX}/lib/pkgconfig${PKG_CONFIG_PATH:+:${PKG_CONFIG_PATH}}"
  echo "QT5_PREFIX=${QT5_PREFIX}"
  echo "QT5_VERSION=$(brew list --versions qt@5 2>/dev/null || true)"
fi
if [[ -n "${SODIUM_PREFIX}" ]]; then
  export PKG_CONFIG_PATH="${SODIUM_PREFIX}/lib/pkgconfig${PKG_CONFIG_PATH:+:${PKG_CONFIG_PATH}}"
fi
if [[ -n "${BOOST_PREFIX}" ]]; then
  export PKG_CONFIG_PATH="${BOOST_PREFIX}/lib/pkgconfig${PKG_CONFIG_PATH:+:${PKG_CONFIG_PATH}}"
fi

need() { command -v "$1" >/dev/null 2>&1 || { echo "FAIL: missing $1" >&2; exit 1; }; }
need g++
need make
need pkg-config
need python3
pkg-config --exists libsodium || { echo "FAIL: libsodium.pc missing"; exit 1; }

cd "${CORE}"

# Fresh GitHub checkouts omit these dummy Makefiles because `Makefile` is
# gitignored. autoreconf / AC_CONFIG_LINKS requires them.
mkdir -p src/qt/test src/test
if [[ ! -f src/qt/Makefile ]]; then
  cat > src/qt/Makefile <<'EOF'
.PHONY: FORCE
all: FORCE
	$(MAKE) -C .. bitcoin_qt test_bitcoin_qt
clean: FORCE
	$(MAKE) -C .. bitcoin_qt_clean test_bitcoin_qt_clean
check: FORCE
	$(MAKE) -C .. test_bitcoin_qt_check
bitcoin-qt bitcoin-qt.exe rigor-qt rigor-qt.exe: FORCE
	$(MAKE) -C .. bitcoin_qt
apk: FORCE
	$(MAKE) -C .. bitcoin_qt_apk
EOF
fi
if [[ ! -f src/qt/test/Makefile ]]; then
  cat > src/qt/test/Makefile <<'EOF'
all:
	$(MAKE) -C ../../ test_bitcoin_qt
clean:
	$(MAKE) -C ../../ test_bitcoin_qt_clean
check:
	$(MAKE) -C ../../ test_bitcoin_qt_check
EOF
fi
if [[ ! -f src/test/Makefile ]]; then
  cat > src/test/Makefile <<'EOF'
all:
	$(MAKE) -C .. bitcoin_test
clean:
	$(MAKE) -C .. bitcoin_test_clean
check:
	$(MAKE) -C .. bitcoin_test_check
EOF
fi

if [[ ! -x ./configure ]]; then
  bash ./autogen.sh
fi
qt_args=()
if [[ -n "${QT5_PREFIX}" ]]; then
  export QTDIR="${QT5_PREFIX}"
  qt_args+=(--with-qt-bindir="${QT5_PREFIX}/bin")
  trans=""
  for cand in \
    "${QT5_PREFIX}/share/qt/translations" \
    "${QT5_PREFIX}/share/qt5/translations" \
    "${QT5_PREFIX}/lib/qt5/translations"
  do
    if [[ -d "${cand}" ]]; then
      trans="${cand}"
      break
    fi
  done
  if [[ -z "${trans}" ]]; then
    trans="$(find "${QT5_PREFIX}" -type d -name translations 2>/dev/null | head -n1 || true)"
  fi
  if [[ -n "${trans}" ]]; then
    qt_args+=(--with-qt-translationdir="${trans}")
    echo "QT_TRANSLATIONS=${trans}"
  fi
fi
prod_flag=()
if ./configure --help 2>/dev/null | grep -q enable-production-mainnet; then
  prod_flag=(--enable-production-mainnet)
fi
./configure \
  --without-bdb \
  --with-sqlite=yes \
  --with-gui=qt5 \
  "${qt_args[@]}" \
  "${prod_flag[@]}" \
  --disable-bench \
  --disable-tests \
  --disable-gui-tests \
  --disable-fuzz-binary \
  --disable-man \
  PYTHON="${PYTHON}" \
  CXXFLAGS="-O2 -DRIGOR_PRODUCTION_MAINNET"
make -j"$(sysctl -n hw.ncpu)"
make deploy PYTHON="${PYTHON}"

dmg_built="$(ls -1 "${CORE}"/*.dmg 2>/dev/null | head -n1 || true)"
[[ -n "${dmg_built}" && -f "${dmg_built}" ]] || {
  echo "FAIL: make deploy produced no dmg" >&2
  ls -la "${CORE}" | head
  exit 1
}

official="rigor-v${VERSION}-${TARGET}.dmg"
cp -a "${dmg_built}" "${OUT}/${official}"
echo "PACKAGED=${OUT}/${official}"
echo "MACOS_WALLET_DMG=PASS"

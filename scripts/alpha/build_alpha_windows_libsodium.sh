#!/usr/bin/env bash
# Cross-install static libsodium into core/depends/<HOST> (not a depends package).
# Required by core/configure.ac PKG_CHECK_MODULES([SODIUM], [libsodium >= 1.0.18]).
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
HOST="${HOST:-x86_64-w64-mingw32}"
PREFIX="${REPO_ROOT}/core/depends/${HOST}"
WORKDIR="${REPO_ROOT}/.runtime/alpha-local-stack/libsodium-mingw"
VER="${LIBSODIUM_VERSION:-1.0.19}"

log() { printf '[win-sodium] %s\n' "$*"; }

if [[ -f "${PREFIX}/lib/pkgconfig/libsodium.pc" ]]; then
  PKG_CONFIG_PATH="${PREFIX}/lib/pkgconfig" pkg-config --modversion libsodium
  log "Already installed under ${PREFIX}"
  exit 0
fi

mkdir -p "${WORKDIR}"
cd "${WORKDIR}"
TARBALL="libsodium-${VER}.tar.gz"
if [[ ! -f "${TARBALL}" ]]; then
  curl -fsSL -o "${TARBALL}" \
    "https://github.com/jedisct1/libsodium/releases/download/${VER}-RELEASE/libsodium-${VER}.tar.gz" \
    || curl -fsSL -o "${TARBALL}" \
    "https://download.libsodium.org/libsodium/releases/libsodium-${VER}-stable.tar.gz"
fi
rm -rf libsodium-src
mkdir libsodium-src
tar -xzf "${TARBALL}" -C libsodium-src --strip-components=1
cd libsodium-src
./configure --host="${HOST}" --prefix="${PREFIX}" --disable-shared --enable-static
make -j"$(nproc)"
make install
PKG_CONFIG_PATH="${PREFIX}/lib/pkgconfig" pkg-config --modversion libsodium
log "Installed libsodium → ${PREFIX}"

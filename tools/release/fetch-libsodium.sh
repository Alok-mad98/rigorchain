#!/usr/bin/env bash
# Build the pinned libsodium (tools/release/libsodium.lock) as a static library.
#
#   tools/release/fetch-libsodium.sh --prefix DIR [--host TRIPLET]
#
# Fails closed on any SHA-256 mismatch. Prints the pkg-config path to use:
#   export PKG_CONFIG_PATH="DIR/lib/pkgconfig"
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
# shellcheck source=libsodium.lock
source "${ROOT}/tools/release/libsodium.lock"

PREFIX=""
HOST=""
while [[ $# -gt 0 ]]; do
  case "$1" in
    --prefix) PREFIX="$2"; shift 2 ;;
    --host) HOST="$2"; shift 2 ;;
    *) echo "usage: $0 --prefix DIR [--host TRIPLET]" >&2; exit 2 ;;
  esac
done
[[ -n "${PREFIX}" ]] || { echo "FAIL: --prefix required" >&2; exit 2; }

log() { printf '[libsodium] %s\n' "$*"; }

sha256_of() {
  if command -v sha256sum >/dev/null 2>&1; then sha256sum "$1" | cut -d' ' -f1
  else shasum -a 256 "$1" | cut -d' ' -f1; fi
}

PC="${PREFIX}/lib/pkgconfig/libsodium.pc"
if [[ -f "${PC}" ]]; then
  have="$(PKG_CONFIG_PATH="${PREFIX}/lib/pkgconfig" pkg-config --modversion libsodium)"
  if [[ "${have}" == "${LIBSODIUM_VERSION}" && ! -e "${PREFIX}/lib/libsodium.so" && ! -e "${PREFIX}/lib/libsodium.dylib" ]]; then
    log "pinned ${have} already installed (static) under ${PREFIX}"
    exit 0
  fi
  echo "FAIL: ${PREFIX} holds libsodium ${have} or a shared build; use an empty prefix" >&2
  exit 1
fi

WORK="$(mktemp -d)"
trap 'rm -rf "${WORK}"' EXIT
TARBALL="${WORK}/libsodium-${LIBSODIUM_VERSION}.tar.gz"

got=""
for url in ${LIBSODIUM_URLS}; do
  log "fetch ${url}"
  if curl -fsSL --retry 3 -o "${TARBALL}" "${url}"; then
    got="$(sha256_of "${TARBALL}")"
    [[ "${got}" == "${LIBSODIUM_SHA256}" ]] && break
    log "sha256 mismatch from ${url}: ${got}"
  fi
  got=""
done
[[ -n "${got}" ]] || { echo "FAIL: no source matched LIBSODIUM_SHA256=${LIBSODIUM_SHA256}" >&2; exit 1; }
log "sha256 OK ${got}"

mkdir "${WORK}/src"
tar -xzf "${TARBALL}" -C "${WORK}/src" --strip-components=1
cd "${WORK}/src"
cfg=(--prefix="${PREFIX}" --disable-shared --enable-static --with-pic --disable-dependency-tracking)
[[ -n "${HOST}" ]] && cfg+=(--host="${HOST}")
./configure "${cfg[@]}" >/dev/null
jobs="$( (command -v nproc >/dev/null && nproc) || sysctl -n hw.ncpu 2>/dev/null || echo 2)"
make -j"${jobs}" >/dev/null
make install >/dev/null
rm -f "${PREFIX}"/lib/libsodium.la

have="$(PKG_CONFIG_PATH="${PREFIX}/lib/pkgconfig" pkg-config --modversion libsodium)"
[[ "${have}" == "${LIBSODIUM_VERSION}" ]] || { echo "FAIL: installed ${have}, expected ${LIBSODIUM_VERSION}" >&2; exit 1; }
log "installed static libsodium ${have} -> ${PREFIX}"
echo "PKG_CONFIG_PATH=${PREFIX}/lib/pkgconfig"

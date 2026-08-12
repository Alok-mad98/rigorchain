#!/usr/bin/env bash
# Cross-install static libcurl (Schannel) into core/depends/<HOST> for rigor-miner PE.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
HOST="${HOST:-x86_64-w64-mingw32}"
PREFIX="${REPO_ROOT}/core/depends/${HOST}"
WORKDIR="${REPO_ROOT}/.runtime/alpha-local-stack/curl-mingw"
VER="${CURL_VERSION:-8.7.1}"

log() { printf '[win-curl] %s\n' "$*"; }

if [[ -f "${PREFIX}/lib/libcurl.a" && -f "${PREFIX}/include/curl/curl.h" ]]; then
  log "Already installed under ${PREFIX}"
  exit 0
fi

mkdir -p "${WORKDIR}"
cd "${WORKDIR}"
TARBALL="curl-${VER}.tar.gz"
if [[ ! -f "${TARBALL}" ]]; then
  curl -fsSL -o "${TARBALL}" "https://curl.se/download/curl-${VER}.tar.gz"
fi
rm -rf curl-src
mkdir curl-src
tar -xzf "${TARBALL}" -C curl-src --strip-components=1
cd curl-src
unset CONFIG_SITE
./configure --host="${HOST}" --prefix="${PREFIX}" \
  --disable-shared --enable-static \
  --with-schannel \
  --without-libpsl --without-nghttp2 --without-libidn2 --without-libssh2 \
  --without-brotli --without-zstd --without-zlib \
  --disable-ldap --disable-ldaps --disable-rtsp --disable-dict --disable-telnet \
  --disable-tftp --disable-pop3 --disable-imap --disable-smb --disable-smtp \
  --disable-gopher --disable-mqtt --disable-manual --disable-docs
make -j"$(nproc)"
make install
log "Installed libcurl → ${PREFIX}"

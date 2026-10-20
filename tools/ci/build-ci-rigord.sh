#!/usr/bin/env bash
# CI infrastructure: produce in-tree core/src/rigord for Genesis Release Guardian.
#
# Hosted Actions checkouts do not contain gitignored rigord. Stage DATABASE_SECURITY
# then fail-closes with missing-binary. This script builds the daemon so Guardian
# can run its own integrity check unchanged.
#
# Does not modify Guardian stages, Consensus Kernel, or skip stale/missing checks.
# If this script fails, the workflow must not invoke Guardian with a fake PASS.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
CORE="${ROOT}/core"
BINARY="${CORE}/src/rigord"
CONFIG_H="${CORE}/src/config/rigor-config.h"

echo "CI_RIGORD_BUILD=START"
echo "tree=${ROOT}"

if [[ ! -x "${CORE}/autogen.sh" ]]; then
  echo "CI_RIGORD_BUILD=FAIL"
  echo "FAIL: missing ${CORE}/autogen.sh"
  exit 1
fi

need() {
  local p="$1"
  command -v "${p}" >/dev/null 2>&1 || {
    echo "CI_RIGORD_BUILD=FAIL"
    echo "FAIL: ${p} is not on PATH"
    exit 1
  }
}

need g++
need make
need python3
need pkg-config
need autoreconf
SODIUM_PREFIX="${ROOT}/.runtime/libsodium-pinned-ci"
bash "${ROOT}/tools/release/fetch-libsodium.sh" --prefix "${SODIUM_PREFIX}" || {
  echo "CI_RIGORD_BUILD=FAIL"
  echo "FAIL: pinned libsodium build failed (tools/release/libsodium.lock)"
  exit 1
}
export PKG_CONFIG_PATH="${SODIUM_PREFIX}/lib/pkgconfig${PKG_CONFIG_PATH:+:${PKG_CONFIG_PATH}}"

echo "COMPILER=$(command -v g++)"
echo "COMPILER_VERSION=$(g++ --version | head -n1)"
echo "NPROC=$(nproc)"
echo "ARCH=$(uname -m)"

cd "${CORE}"
echo "== autogen =="
bash ./autogen.sh

echo "== configure (daemon + sqlite wallet, no GUI; not production-mainnet) =="
# Do not pass --enable-production-mainnet: DATABASE_SECURITY functional tests
# use isolated datadirs and need development networks.
# CXXFLAGS=-O2 skips -g to keep hosted link memory in range.
./configure \
  --enable-static-libsodium \
  --without-gui \
  --with-sqlite=yes \
  --disable-bench \
  --disable-fuzz-binary \
  --disable-man \
  --disable-usdt \
  CXXFLAGS="-O2"

echo "== make src/rigord =="
make -j"$(nproc)" src/rigord

if [[ ! -x "${BINARY}" ]]; then
  echo "CI_RIGORD_BUILD=FAIL"
  echo "FAIL: make reported success but ${BINARY} is missing"
  exit 1
fi

if [[ ! -f "${CONFIG_H}" ]] || ! grep -q 'USE_SQLITE 1' "${CONFIG_H}"; then
  echo "CI_RIGORD_BUILD=FAIL"
  echo "FAIL: USE_SQLITE is not 1 in ${CONFIG_H}"
  exit 1
fi
if grep -q '^#define USE_BDB' "${CONFIG_H}"; then
  echo "CI_RIGORD_BUILD=FAIL"
  echo "FAIL: USE_BDB must not be enabled"
  exit 1
fi

echo "CI_RIGORD_PATH=${BINARY}"
echo "CI_RIGORD_MTIME=$(date -u -r "${BINARY}" '+%Y-%m-%dT%H:%M:%SZ')"
echo "CI_RIGORD_SIZE=$(stat -c '%s' "${BINARY}")"
echo "USE_SQLITE=1"
file "${BINARY}" || true
"${BINARY}" --version | head -n 6 || true
echo "CI_RIGORD_BUILD=PASS"
echo "NOTE: Guardian Stage DATABASE_SECURITY still runs its own BINARY_INTEGRITY check"

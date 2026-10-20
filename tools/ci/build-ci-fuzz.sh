#!/usr/bin/env bash
# CI: build core/src/test/fuzz/fuzz with libFuzzer + ASan + UBSan against pinned libsodium.
# Fails closed; never reports success without the binary.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
CORE="${ROOT}/core"
BINARY="${CORE}/src/test/fuzz/fuzz"

for p in clang clang++ make pkg-config autoreconf python3; do
  command -v "${p}" >/dev/null 2>&1 || { echo "CI_FUZZ_BUILD=FAIL missing ${p}"; exit 1; }
done

SODIUM_PREFIX="${ROOT}/.runtime/libsodium-pinned-ci"
bash "${ROOT}/tools/release/fetch-libsodium.sh" --prefix "${SODIUM_PREFIX}" || {
  echo "CI_FUZZ_BUILD=FAIL pinned libsodium build failed (tools/release/libsodium.lock)"
  exit 1
}
export PKG_CONFIG_PATH="${SODIUM_PREFIX}/lib/pkgconfig${PKG_CONFIG_PATH:+:${PKG_CONFIG_PATH}}"

cd "${CORE}"
bash ./autogen.sh
./configure \
  --enable-fuzz \
  --with-sanitizers=fuzzer,address,undefined \
  --enable-static-libsodium \
  --without-gui \
  --without-miniupnpc \
  --without-natpmp \
  --disable-zmq \
  CC=clang CXX=clang++
make -j"$(nproc)"

[[ -x "${BINARY}" ]] || { echo "CI_FUZZ_BUILD=FAIL ${BINARY} missing"; exit 1; }
echo "CI_FUZZ_BINARY=${BINARY}"
echo "CI_FUZZ_BUILD=PASS"

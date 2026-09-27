#!/usr/bin/env bash
# Compile production CSHA256 (portable Transform) into a tiny CLI for the
# differential gate. Does not require a full bitcoind make.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="${1:-$ROOT/formal_verification/extracted/cpp_hash_cli}"
SRC="$ROOT/core/src"

if ! command -v g++ >/dev/null && ! command -v c++ >/dev/null; then
  echo "FAIL: g++/c++ is not on PATH" >&2
  exit 1
fi
CXX="${CXX:-$(command -v g++ || command -v c++)}"

mkdir -p "$(dirname "$OUT")"
# No HAVE_CONFIG_H / USE_ASM / ENABLE_* portable C++ Transform only.
"$CXX" -O2 -std=c++17 -Wall -Wextra \
  -I"$SRC" \
  -o "$OUT" \
  "$ROOT/formal_verification/tools/cpp_hash_cli.cpp" \
  "$SRC/crypto/sha256.cpp"
echo "cpp hash cli: $OUT"

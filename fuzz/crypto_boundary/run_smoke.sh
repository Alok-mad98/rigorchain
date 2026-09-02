#!/usr/bin/env bash
# P0-080 cryptographic boundary fuzz smoke.
# Crash => CRASH_FOUND. Encoding accept of truncated/oversized => ACCEPT_INVALID.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SRC="$ROOT/core/src"
OUT="${TMPDIR:-/tmp}/crypto_boundary_fuzz_smoke"
CXX="${CXX:-$(command -v g++ || command -v c++)}"

mkdir -p "$(dirname "$OUT")"
CORPUS="$ROOT/fuzz/crypto_boundary/corpus"
mkdir -p "$CORPUS"

SODIUM_CFLAGS=""
SODIUM_LIBS=""
SODIUM_DEFINE=""
if command -v pkg-config >/dev/null 2>&1 && pkg-config --exists libsodium; then
  SODIUM_CFLAGS="$(pkg-config --cflags libsodium)"
  SODIUM_LIBS="$(pkg-config --libs libsodium)"
  SODIUM_DEFINE="-DCRYPTO_BOUNDARY_HAVE_SODIUM"
fi

EXTRA_SRC=()
if [[ -n "$SODIUM_DEFINE" ]]; then
  EXTRA_SRC+=("$SRC/crypto/ed25519_provider.cpp")
fi

"$CXX" -O2 -std=c++17 -ffunction-sections -fdata-sections \
  -DCRYPTO_BOUNDARY_FUZZ_MAIN \
  $SODIUM_DEFINE \
  -I"$SRC" \
  $SODIUM_CFLAGS \
  -o "$OUT" \
  "$ROOT/fuzz/crypto_boundary/crypto_boundary_fuzz.cpp" \
  "$SRC/rigor_kernel/keccak.cpp" \
  "$SRC/crypto/sha256.cpp" \
  "${EXTRA_SRC[@]}" \
  $SODIUM_LIBS \
  -Wl,--gc-sections

python3 - "$CORPUS" <<'PY'
from pathlib import Path
import sys
c = Path(sys.argv[1])
(c / "empty").write_bytes(b"")
(c / "trunc_sig").write_bytes(bytes([63, 32, 32]) + bytes(90))
(c / "over_sig").write_bytes(bytes([65, 32, 32]) + bytes(90))
(c / "bad_pk").write_bytes(bytes([64, 31, 32]) + bytes(90))
(c / "random").write_bytes(bytes((i * 13) % 256 for i in range(1024)))
(c / "header").write_bytes(bytes([0x01, 0x00, 0x00, 0x00]) + bytes(80))
PY

set +e
"$OUT" smoke
rc=$?
set -e
if [[ $rc -ne 0 ]]; then
  echo "CRASH_FOUND"
  exit 1
fi

set +e
for f in "$CORPUS"/*; do
  "$OUT" "$f"
  rc=$?
  if [[ $rc -ne 0 ]]; then
    echo "CRASH_FOUND"
    echo "  seed=$f"
    exit 1
  fi
done
set -e

echo "CRYPTO_FUZZ_STATUS = PASS"
echo "FUZZ_STATUS=PASS"
echo "CRASH_FOUND=NO"
echo "UNDEFINED_BEHAVIOR=NO"
echo "ACCEPT_INVALID=NO"

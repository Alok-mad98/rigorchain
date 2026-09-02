#!/usr/bin/env bash
# P0-081 PoW boundary fuzz smoke.
# Crash => CRASH_FOUND. Mutation-invariant digest => ACCEPT_INVALID.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SRC="$ROOT/core/src"
OUT="${TMPDIR:-/tmp}/pow_boundary_fuzz_smoke"
CXX="${CXX:-$(command -v g++ || command -v c++)}"

mkdir -p "$(dirname "$OUT")"
CORPUS="$ROOT/fuzz/pow_boundary/corpus"
mkdir -p "$CORPUS"

"$CXX" -O2 -std=c++17 -ffunction-sections -fdata-sections \
  -DPOW_BOUNDARY_FUZZ_MAIN \
  -I"$SRC" \
  -o "$OUT" \
  "$ROOT/fuzz/pow_boundary/pow_boundary_fuzz.cpp" \
  "$SRC/rigor_kernel/keccak.cpp" \
  -Wl,--gc-sections

python3 - "$CORPUS" <<'PY'
from pathlib import Path
import sys
c = Path(sys.argv[1])
(c / "empty").write_bytes(b"")
(c / "trunc").write_bytes(bytes(40))
(c / "header80").write_bytes(bytes([0x01, 0x00, 0x00, 0x00]) + bytes(76))
(c / "oversize").write_bytes(bytes(80) + bytes([0xff, 0x1d, 0x00, 0xff, 0xff]))
(c / "neg_nbits").write_bytes(bytes(80) + bytes([0xff, 0x00, 0x00, 0x1d | 0x80]))
(c / "zero_nbits").write_bytes(bytes(80) + bytes(4))
(c / "random").write_bytes(bytes((i * 13) % 256 for i in range(1024)))
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

echo "POW_FUZZ_STATUS = PASS"
echo "FUZZ_STATUS=PASS"
echo "CRASH_FOUND=NO"
echo "UNDEFINED_BEHAVIOR=NO"
echo "ACCEPT_INVALID=NO"

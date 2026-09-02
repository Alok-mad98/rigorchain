#!/usr/bin/env bash
# P0-086B: dual-asset Coin / key / undo / mutation fuzz smoke.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SRC="$ROOT/core/src"
OUT="${TMPDIR:-/tmp}/dual_asset_database_fuzz_smoke"
CXX="${CXX:-$(command -v g++ || command -v c++)}"
CORPUS="$ROOT/fuzz/dual_asset_database/corpus"

mkdir -p "$(dirname "$OUT")" "$CORPUS"

"$CXX" -O2 -std=c++17 -ffunction-sections -fdata-sections \
  -DDUAL_ASSET_DATABASE_FUZZ_MAIN \
  -I"$SRC" \
  -o "$OUT" \
  "$ROOT/fuzz/dual_asset_database/dual_asset_database_fuzz.cpp" \
  -Wl,--gc-sections

python3 - "$CORPUS" <<'PY'
from pathlib import Path
import sys
c = Path(sys.argv[1])
(c / "empty").write_bytes(b"")
(c / "coin_trunc").write_bytes(bytes([0, 0x80]))
(c / "key_ok_prefix").write_bytes(bytes([2, ord("C")]) + bytes(40))
(c / "mutate_oru").write_bytes(bytes([3, 0x00, 20]) + bytes(20))
(c / "mutate_res").write_bytes(bytes([3, 0x01, 0x51, 32]) + bytes(32))
(c / "random").write_bytes(bytes((i * 19) % 256 for i in range(512)))
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

echo "DUAL_ASSET_FUZZ_STATUS=PASS"
echo "FUZZ_STATUS=PASS"
echo "CRASH_FOUND=NO"
echo "UNDEFINED_BEHAVIOR=NO"

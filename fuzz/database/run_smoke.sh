#!/usr/bin/env bash
# P0-086: Coin / key / undo decoder fuzz smoke.
# Crash => CRASH_FOUND. Does not claim a coverage campaign.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SRC="$ROOT/core/src"
OUT="${TMPDIR:-/tmp}/database_codec_fuzz_smoke"
CXX="${CXX:-$(command -v g++ || command -v c++)}"
CORPUS="$ROOT/fuzz/database/corpus"

mkdir -p "$(dirname "$OUT")" "$CORPUS"

"$CXX" -O2 -std=c++17 -ffunction-sections -fdata-sections \
  -DDATABASE_CODEC_FUZZ_MAIN \
  -I"$SRC" \
  -o "$OUT" \
  "$ROOT/fuzz/database/database_codec_fuzz.cpp" \
  -Wl,--gc-sections

python3 - "$CORPUS" <<'PY'
from pathlib import Path
import sys
c = Path(sys.argv[1])
(c / "empty").write_bytes(b"")
(c / "coin_trunc").write_bytes(bytes([0, 0x80]))
(c / "undo_ff").write_bytes(bytes([1]) + bytes([0xFF] * 32))
(c / "key_wrong_type").write_bytes(bytes([2, 0x63]) + bytes(40))
(c / "key_ok_prefix").write_bytes(bytes([2, ord("C")]) + bytes(40))
(c / "overlay_ops").write_bytes(bytes([3]) + bytes((i * 13) % 256 for i in range(256)))
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

echo "DATABASE_FUZZ_STATUS=PASS"
echo "FUZZ_STATUS=PASS"
echo "CRASH_FOUND=NO"
echo "UNDEFINED_BEHAVIOR=NO"

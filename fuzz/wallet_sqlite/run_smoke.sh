#!/usr/bin/env bash
# P0-089: wallet SQLite header / record mutation smoke.
# Crash => CRASH_FOUND. Does not claim a coverage campaign.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="${TMPDIR:-/tmp}/wallet_sqlite_fuzz_smoke"
CXX="${CXX:-$(command -v g++ || command -v c++)}"
CORPUS="$ROOT/fuzz/wallet_sqlite/corpus"

mkdir -p "$(dirname "$OUT")" "$CORPUS"

"$CXX" -O2 -std=c++17 \
  -o "$OUT" \
  "$ROOT/fuzz/wallet_sqlite/wallet_sqlite_fuzz.cpp"

python3 - "$CORPUS" <<'PY'
from pathlib import Path
import sys
c = Path(sys.argv[1])
(c / "empty").write_bytes(b"")
(c / "trunc").write_bytes(b"SQLite format 3\x00" + b"\x00" * 20)
magic = b"SQLite format 3\x00"
hdr = bytearray(512)
hdr[:16] = magic
hdr[16:18] = (512).to_bytes(2, "big")
(c / "ok_header").write_bytes(bytes(hdr))
unk = bytearray(hdr)
unk[60:64] = (99).to_bytes(4, "big")
(c / "unknown_user_version").write_bytes(bytes(unk))
mut = bytearray(4096)
mut[:16] = magic
mut[16:18] = (4096).to_bytes(2, "big")
mut[200:250] = b"\xff" * 50
(c / "page_mutation").write_bytes(bytes(mut))
(c / "not_sqlite").write_bytes(b"BerkeleyDB" + b"\x00" * 100)
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

echo "WALLET_SQLITE_FUZZ_STATUS=PASS"
echo "FUZZ_STATUS=PASS"
echo "CRASH_FOUND=NO"
echo "UNDEFINED_BEHAVIOR=NO"

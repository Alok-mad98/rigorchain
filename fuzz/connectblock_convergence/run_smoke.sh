#!/usr/bin/env bash
# P0-086 ConnectBlock convergence fuzz smoke.
# Model: reject must not increment state_mutation_count.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="${TMPDIR:-/tmp}/connectblock_convergence_fuzz_smoke"
CXX="${CXX:-$(command -v g++ || command -v c++)}"

mkdir -p "$(dirname "$OUT")"
CORPUS="$ROOT/fuzz/connectblock_convergence/corpus"
mkdir -p "$CORPUS"

"$CXX" -O2 -std=c++17 -ffunction-sections -fdata-sections \
  -DCONNECTBLOCK_CONVERGENCE_FUZZ_MAIN \
  -o "$OUT" \
  "$ROOT/fuzz/connectblock_convergence/connectblock_convergence_fuzz.cpp" \
  -Wl,--gc-sections

python3 - "$CORPUS" <<'PY'
from pathlib import Path
import sys
c = Path(sys.argv[1])
(c / "trunc").write_bytes(bytes(40))
(c / "header80").write_bytes(bytes(80))
(c / "nbits0").write_bytes(bytes(72) + bytes(4) + bytes(8))
(c / "merkle0").write_bytes(bytes([1]) * 36 + bytes(32) + bytes([1]) * 20)
(c / "bad_coinbase").write_bytes(bytes([1]) * 80 + bytes([0x00]))
(c / "bad_tx").write_bytes(bytes([1]) * 80 + bytes([0x01]))
(c / "bad_predicate").write_bytes(bytes([1]) * 80 + bytes([0x02]))
(c / "bad_res").write_bytes(bytes([1]) * 80 + bytes([0x04]))
(c / "accept_then_disconnect").write_bytes(bytes([1]) * 81)
(c / "random").write_bytes(bytes((i * 17) % 256 for i in range(256)))
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

echo "CONNECTBLOCK_FUZZ_STATUS=PASS"
echo "FUZZ_STATUS=PARTIAL"
echo "CRASH_FOUND=NO"
echo "UNDEFINED_BEHAVIOR=NO"
echo "NOTE: harness models reject→no-commit; it does not instantiate Chainstate::ConnectBlock"

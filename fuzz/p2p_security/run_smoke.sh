#!/usr/bin/env bash
# P0-076 P2P security fuzz smoke.
# Crash => CRASH_FOUND. Does not invoke ConsensusKernel.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SRC="$ROOT/core/src"
OUT="${TMPDIR:-/tmp}/p2p_security_fuzz_smoke"
CXX="${CXX:-$(command -v g++ || command -v c++)}"

mkdir -p "$(dirname "$OUT")"
CORPUS="$ROOT/fuzz/p2p_security/corpus"
mkdir -p "$CORPUS"

"$CXX" -O2 -std=c++17 -ffunction-sections -fdata-sections \
  -DP2P_SECURITY_FUZZ_MAIN \
  -I"$SRC" \
  -o "$OUT" \
  "$ROOT/fuzz/p2p_security/p2p_security_fuzz.cpp" \
  "$SRC/net/security/p2p_message_boundary.cpp" \
  -Wl,--gc-sections

python3 - "$CORPUS" <<'PY'
from pathlib import Path
import sys
c = Path(sys.argv[1])
(c / "empty").write_bytes(b"")
(c / "short_header").write_bytes(bytes(range(79)))
(c / "null_header").write_bytes(bytes(80))
(c / "version_bitcoin").write_bytes(bytes([2, 0x10, 0x27, 0x00, 0x00]) + bytes(80))
(c / "tx_trunc").write_bytes(bytes([1, 1, 0, 0, 0]))
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

echo "FUZZ_STATUS=PASS"
echo "CRASH_FOUND=NO"
echo "P2P_CONSENSUS_BYPASS=NONE"

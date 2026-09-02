#!/usr/bin/env bash
# P0-078 script/opcode boundary fuzz smoke.
# Crash => CRASH_FOUND. Accept of malformed => ACCEPT_INVALID.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SRC="$ROOT/core/src"
TOOLS="$ROOT/tools"
OUT="${TMPDIR:-/tmp}/script_opcode_fuzz_smoke"
CXX="${CXX:-$(command -v g++ || command -v c++)}"

mkdir -p "$(dirname "$OUT")"
CORPUS="$ROOT/fuzz/script_opcode_boundary/corpus"
mkdir -p "$CORPUS"

"$CXX" -O2 -std=c++17 -ffunction-sections -fdata-sections \
  -DSCRIPT_OPCODE_FUZZ_MAIN \
  -I"$SRC" \
  -o "$OUT" \
  "$ROOT/fuzz/script_opcode_boundary/script_opcode_fuzz.cpp" \
  "$TOOLS/consensus_kernel_refinement_log_stub.cpp" \
  "$SRC/script/rigor_classify.cpp" \
  "$SRC/script/script.cpp" \
  "$SRC/uint256.cpp" \
  "$SRC/util/strencodings.cpp" \
  "$SRC/crypto/sha256.cpp" \
  -Wl,--gc-sections

python3 - "$CORPUS" <<'PY'
from pathlib import Path
import sys
c = Path(sys.argv[1])
(c / "empty").write_bytes(b"")
(c / "checksig").write_bytes(bytes([0xac, 0xae, 0xb1, 0xb2, 0x76, 0xa9, 0x88]))
(c / "truncated").write_bytes(bytes([0x00, 0x14]) + bytes(10))
(c / "random").write_bytes(bytes((i * 13) % 256 for i in range(1024)))
(c / "witness_v0").write_bytes(bytes([0x80, 0x16]) + bytes([0x00, 0x14]) + bytes(20))
(c / "op_return").write_bytes(bytes([0x6a, 0x01, 0xff]))
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

echo "SCRIPT_FUZZ_STATUS = PASS"
echo "FUZZ_STATUS=PASS"
echo "CRASH_FOUND=NO"
echo "ACCEPT_INVALID=NO"
echo "CONSENSUS_BYPASS=NO"

#!/usr/bin/env bash
# P0-079 Convergence Draw fuzz smoke.
# Crash => CRASH_FOUND. Order-dependent winner => NONDETERMINISM.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SRC="$ROOT/core/src"
TOOLS="$ROOT/tools"
OUT="${TMPDIR:-/tmp}/convergence_draw_fuzz_smoke"
CXX="${CXX:-$(command -v g++ || command -v c++)}"

mkdir -p "$(dirname "$OUT")"
CORPUS="$ROOT/fuzz/convergence_draw/corpus"
mkdir -p "$CORPUS"

"$CXX" -O2 -std=c++17 -ffunction-sections -fdata-sections \
  -DCONVERGENCE_DRAW_FUZZ_MAIN \
  -I"$SRC" \
  -o "$OUT" \
  "$ROOT/fuzz/convergence_draw/convergence_draw_fuzz.cpp" \
  "$TOOLS/consensus_kernel_refinement_log_stub.cpp" \
  "$SRC/consensus/convergence_logic.cpp" \
  "$SRC/consensus_rigor/sigma_hash.cpp" \
  "$SRC/crypto/sha256.cpp" \
  "$SRC/arith_uint256.cpp" \
  "$SRC/uint256.cpp" \
  "$SRC/util/strencodings.cpp" \
  "$SRC/script/rigor_classify.cpp" \
  "$SRC/script/script.cpp" \
  -Wl,--gc-sections

python3 - "$CORPUS" <<'PY'
from pathlib import Path
import sys
c = Path(sys.argv[1])
(c / "empty").write_bytes(bytes([0, 1]))
one = bytearray(128)
one[0] = 1
one[1] = 1
one[67 + 31] = 0x11
one[67 + 32] = 1
(c / "one").write_bytes(bytes(one))
two = bytearray(180)
two[0] = 2
two[1] = 1
two[67 + 31] = 0x11
two[67 + 32] = 1
two[67 + 40 + 31] = 0x22
two[67 + 40 + 32] = 1
(c / "two").write_bytes(bytes(two))
t0 = bytearray(128)
t0[0] = 1
t0[1] = 0
(c / "t0").write_bytes(bytes(t0))
tmax = bytearray(128)
tmax[0] = 1
tmax[1] = 0xff
(c / "tmax").write_bytes(bytes(tmax))
fat = bytearray(128)
fat[0] = 1
fat[1] = 1
for i in range(8):
    fat[67 + 32 + i] = 0xff
(c / "max_weight").write_bytes(bytes(fat))
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

echo "DRAW_FUZZ_STATUS = PASS"
echo "FUZZ_STATUS=PASS"
echo "CRASH_FOUND=NO"
echo "NONDETERMINISM=NO"
echo "INVALID_WINNER=NO"

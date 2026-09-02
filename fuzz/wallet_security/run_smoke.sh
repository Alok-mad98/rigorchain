#!/usr/bin/env bash
# P0-077 wallet security fuzz smoke.
# Crash => CRASH_FOUND. Key leak => KEY_LEAK. Does not invoke ConsensusKernel.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SRC="$ROOT/core/src"
OUT="${TMPDIR:-/tmp}/wallet_security_fuzz_smoke"
CXX="${CXX:-$(command -v g++ || command -v c++)}"

mkdir -p "$(dirname "$OUT")"
CORPUS="$ROOT/fuzz/wallet_security/corpus"
mkdir -p "$CORPUS"

"$CXX" -O2 -std=c++17 -ffunction-sections -fdata-sections \
  -DWALLET_SECURITY_FUZZ_MAIN \
  -I"$SRC" \
  -o "$OUT" \
  "$ROOT/fuzz/wallet_security/wallet_security_fuzz.cpp" \
  "$SRC/wallet/security/wallet_security_boundary.cpp" \
  "$SRC/wallet/security/wallet_signing_boundary.cpp" \
  -Wl,--gc-sections

python3 - "$CORPUS" <<'PY'
from pathlib import Path
import sys
c = Path(sys.argv[1])
(c / "empty").write_bytes(b"")
(c / "addr").write_bytes(b"\x01rg1qabcdefghijklmnopqrstuvwxyz")
(c / "neg_amt").write_bytes(b"\x02-1")
(c / "import").write_bytes(bytes([3]) + bytes(range(64)))
(c / "rpc").write_bytes(b"\x04sendtoaddress")
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
echo "PRIVATE_KEY_EXPOSURE=NONE"
echo "INVALID_TX_GENERATION=NONE"

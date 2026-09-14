#!/usr/bin/env bash
# S2 predicate-boundary differential fuzz smoke.
# Divergence between production VerifyRigorPredicate and the P058 boundary
# => BOUNDARY_DIVERGENCE. Crash => CRASH_FOUND.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SRC="$ROOT/core/src"
OUT="${TMPDIR:-/tmp}/predicate_boundary_fuzz_smoke"
CXX="${CXX:-$(command -v g++ || command -v c++)}"

mkdir -p "$(dirname "$OUT")"
CORPUS="$ROOT/fuzz/predicate_boundary_differential/corpus"
mkdir -p "$CORPUS"

"$CXX" -O2 -std=c++17 -ffunction-sections -fdata-sections \
  -DPREDICATE_BOUNDARY_FUZZ_MAIN \
  -I"$SRC" \
  -o "$OUT" \
  "$ROOT/fuzz/predicate_boundary_differential/predicate_boundary_fuzz.cpp" \
  "$SRC/test/predicate_boundary_ed25519_stub.cpp" \
  "$SRC/script/predicate_verify.cpp" \
  "$SRC/script/script_error.cpp" \
  "$SRC/consensus/trace/consensus_trace.cpp" \
  "$SRC/script/rigor_classify.cpp" \
  "$SRC/script/script.cpp" \
  "$SRC/uint256.cpp" \
  "$SRC/util/strencodings.cpp" \
  "$SRC/crypto/sha256.cpp" \
  "$SRC/crypto/ripemd160.cpp" \
  "$SRC/crypto/sha512.cpp" \
  "$SRC/crypto/hmac_sha512.cpp" \
  "$SRC/hash.cpp" \
  -Wl,--gc-sections

python3 - "$CORPUS" <<'PY'
from pathlib import Path
import sys
c = Path(sys.argv[1])
# One seed per script shape the decoder can emit, plus degenerate inputs.
(c / "empty").write_bytes(b"")
(c / "one_byte").write_bytes(b"\x00")
(c / "v0_keyhash").write_bytes(bytes([0, 0, 0]) + bytes(60))
(c / "v1_res").write_bytes(bytes([1, 0, 0]) + bytes(60))
(c / "v1_fuzz_program").write_bytes(bytes([2, 2, 1]) + bytes((i * 7) % 256 for i in range(60)))
(c / "res_mint").write_bytes(bytes([3, 0, 0]) + bytes(60))
(c / "arbitrary_version").write_bytes(bytes([4, 3, 3]) + bytes((i * 31) % 256 for i in range(60)))
(c / "raw_script").write_bytes(bytes([5, 3, 3]) + bytes((i * 13) % 256 for i in range(60)))
(c / "empty_spk").write_bytes(bytes([6, 0, 0]) + bytes(60))
(c / "oversize_spk").write_bytes(bytes([7, 3, 3]) + bytes((i * 19) % 256 for i in range(80)))
(c / "truncated_mint").write_bytes(bytes([8, 0, 0]) + bytes(60))
(c / "oversize_witness").write_bytes(bytes([9, 0, 0]) + bytes(60))
(c / "all_ones").write_bytes(bytes([0xff] * 64))
PY

set +e
"$OUT" --selftest
self_rc=$?
set -e
if [[ $self_rc -ne 1 ]]; then
  echo "FAIL: ACCEPTANCE_SET_GREW detector did not fire (rc=$self_rc)"
  exit 1
fi

FORCE_OUT="${TMPDIR:-/tmp}/predicate_boundary_fuzz_force_accept"
"$CXX" -O2 -std=c++17 -ffunction-sections -fdata-sections \
  -DPREDICATE_BOUNDARY_FUZZ_MAIN \
  -DPREDICATE_FUZZ_FORCE_BOUNDARY_ACCEPT \
  -I"$SRC" \
  -o "$FORCE_OUT" \
  "$ROOT/fuzz/predicate_boundary_differential/predicate_boundary_fuzz.cpp" \
  "$SRC/test/predicate_boundary_ed25519_stub.cpp" \
  "$SRC/script/predicate_verify.cpp" \
  "$SRC/script/script_error.cpp" \
  "$SRC/consensus/trace/consensus_trace.cpp" \
  "$SRC/script/rigor_classify.cpp" \
  "$SRC/script/script.cpp" \
  "$SRC/uint256.cpp" \
  "$SRC/util/strencodings.cpp" \
  "$SRC/crypto/sha256.cpp" \
  "$SRC/crypto/ripemd160.cpp" \
  "$SRC/crypto/sha512.cpp" \
  "$SRC/crypto/hmac_sha512.cpp" \
  "$SRC/hash.cpp" \
  -Wl,--gc-sections
set +e
force_err="$("$FORCE_OUT" "$CORPUS/empty" 2>&1)"
force_rc=$?
set -e
if [[ $force_rc -eq 0 ]]; then
  echo "FAIL: force-accept build did not trip ACCEPTANCE_SET_GREW"
  exit 1
fi
if ! grep -q "ACCEPTANCE_SET_GREW NEW_ACCEPT=TRUE LEGACY_ACCEPT=FALSE" <<<"$force_err"; then
  echo "FAIL: force-accept build exited $force_rc without ACCEPTANCE_SET_GREW"
  echo "$force_err"
  exit 1
fi

set +e
"$OUT" smoke
rc=$?
set -e
if [[ $rc -ne 0 ]]; then
  echo "BOUNDARY_DIVERGENCE=YES"
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

echo "PREDICATE_BOUNDARY_FUZZ_STATUS = PASS"
echo "FUZZ_STATUS=PASS"
echo "CRASH_FOUND=NO"
echo "BOUNDARY_DIVERGENCE=NO"
echo "ACCEPTANCE_SET_GREW=NO"
echo "ACCEPTANCE_MONOTONICITY=PASS"
echo "PREDICATE_BOUNDARY_VS_PRODUCTION=SAME-FUNCTION"

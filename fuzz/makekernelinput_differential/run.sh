#!/usr/bin/env bash
# Out-of-domain MakeKernelInput vs CoqMakeCryptoAccept differential.
# Random CScript, not the 9216-state abstract gate domain.
#
# Usage (from repo root, WSL):
#   bash fuzz/makekernelinput_differential/run.sh
#   MAKEKERNELINPUT_FUZZ_ITERS=1000000 bash fuzz/makekernelinput_differential/run.sh
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SRC="$ROOT/core/src"
OUT="${TMPDIR:-/tmp}/makekernelinput_fuzz"
CXX="${CXX:-$(command -v g++ || command -v c++)}"
ITERS="${MAKEKERNELINPUT_FUZZ_ITERS:-200000}"

"$CXX" -O2 -std=c++17 -ffunction-sections -fdata-sections \
  -DMAKEKERNELINPUT_FUZZ_MAIN \
  -I"$SRC" \
  -o "$OUT" \
  "$ROOT/fuzz/makekernelinput_differential/makekernelinput_fuzz.cpp" \
  "$SRC/test/predicate_boundary_ed25519_stub.cpp" \
  "$SRC/script/predicate_verify.cpp" \
  "$SRC/script/rigor_classify.cpp" \
  "$SRC/script/script.cpp" \
  "$SRC/uint256.cpp" \
  "$SRC/util/strencodings.cpp" \
  "$SRC/crypto/sha256.cpp" \
  "$SRC/crypto/ripemd160.cpp" \
  "$SRC/crypto/sha512.cpp" \
  "$SRC/crypto/hmac_sha512.cpp" \
  "$SRC/hash.cpp" \
  "$SRC/primitives/transaction.cpp" \
  -Wl,--gc-sections

echo "== makekernelinput out-of-domain differential iters=$ITERS =="
"$OUT" smoke "$ITERS"
echo "MAKEKERNELINPUT_COQ_DIVERGENCE=NO"
echo "EMPTY_TAG_PREIMAGE_NONEMPTY=NO"
echo "NON_NATIVE_V1_CARRIES_RES_TAG=NO"
echo "FORGED_ZERO_SIGHASH_FIELDS=NO"

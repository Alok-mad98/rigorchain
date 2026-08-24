#!/usr/bin/env bash
# Compile production ComputeSigmaHash + ComputeConvergenceRootFromEligibilityTxids
# into a tiny CLI for the differential gate. Does not require a full bitcoind make.
#
# Links:
#   core/src/consensus_rigor/sigma_hash.cpp
#   core/src/consensus/convergence_root_eligibility.cpp
#   core/src/crypto/sha256.cpp   (portable Transform; no USE_ASM / SIMD)
#
# Does not compile ComputeConvergenceRootFromConvergenceSet (view-dependent).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="${1:-$ROOT/formal_verification/extracted/cpp_sigma_root_cli}"
SRC="$ROOT/core/src"

if ! command -v g++ >/dev/null && ! command -v c++ >/dev/null; then
  echo "FAIL: g++/c++ is not on PATH" >&2
  exit 1
fi
CXX="${CXX:-$(command -v g++ || command -v c++)}"

mkdir -p "$(dirname "$OUT")"
# No HAVE_CONFIG_H / USE_ASM / ENABLE_* : portable C++ Transform only.
"$CXX" -O2 -std=c++17 -Wall -Wextra \
  -I"$SRC" \
  -o "$OUT" \
  "$ROOT/formal_verification/tools/cpp_sigma_root_cli.cpp" \
  "$SRC/consensus_rigor/sigma_hash.cpp" \
  "$SRC/consensus/convergence_root_eligibility.cpp" \
  "$SRC/crypto/sha256.cpp"
echo "cpp sigma/root cli: $OUT"

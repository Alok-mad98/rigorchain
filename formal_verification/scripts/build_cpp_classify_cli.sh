#!/usr/bin/env bash
# Compile production AssetTypeFromScriptPubKey into a tiny CLI for the
# differential gate. Does not require a full bitcoind make.
#
# Links:
#   core/src/script/rigor_classify.cpp   (script-as-asset classifier + parsers)
#   core/src/script/script.cpp           (CScript::IsWitnessProgram / IsPushOnly / GetScriptOp)
#   core/src/util/strencodings.cpp       (HexStr used by CScriptWitness::ToString)
#
# Does not link predicate_verify.cpp (SignatureHash / Ed25519).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="${1:-$ROOT/formal_verification/extracted/cpp_classify_cli}"
SRC="$ROOT/core/src"

if ! command -v g++ >/dev/null && ! command -v c++ >/dev/null; then
  echo "FAIL: g++/c++ is not on PATH" >&2
  exit 1
fi
CXX="${CXX:-$(command -v g++ || command -v c++)}"

mkdir -p "$(dirname "$OUT")"
"$CXX" -O2 -std=c++17 -Wall -Wextra \
  -I"$SRC" \
  -o "$OUT" \
  "$ROOT/formal_verification/tools/cpp_classify_cli.cpp" \
  "$SRC/script/rigor_classify.cpp" \
  "$SRC/script/script.cpp" \
  "$SRC/util/strencodings.cpp"
echo "cpp classify cli: $OUT"

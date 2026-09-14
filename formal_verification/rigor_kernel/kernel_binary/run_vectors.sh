#!/usr/bin/env bash
# Execute golden vectors against rigor-kernel-checker.
# Test harness only. Does not prove binary semantic equivalence.
set -euo pipefail

DIR="$(cd "$(dirname "$0")" && pwd)"
BIN="$DIR/rigor-kernel-checker"
VEC="$DIR/test_vectors"

fail() { echo "FAIL: $*" >&2; exit 1; }

[[ -x "$BIN" ]] || fail "rigor-kernel-checker missing; run build.sh"

required=(
  utxo_valid.json
  utxo_invalid.json
  utxo_over_max.json
  oru_conservation.json
  oru_invalid.json
  res_schedule.json
  res_invalid.json
  script_classification.json
  pow_target.json
  pow_invalid.json
  asert_negative_time.json
  asert_invalid.json
)

pass=0
invalid_seen=0
for name in "${required[@]}"; do
  f="$VEC/$name"
  [[ -f "$f" ]] || fail "missing vector $name"
  got="$("$BIN" "$f" | tr -d '\r')"
  expect="$(python3 -c "import json; print(json.load(open('$f'))['expect'])")"
  echo "vector=$name result=$got expect=$expect"
  [[ "$got" == "$expect" ]] || fail "$name: got $got want $expect"
  if [[ "$expect" == "INVALID" ]]; then
    invalid_seen=$((invalid_seen + 1))
  fi
  pass=$((pass + 1))
done

if [[ "$invalid_seen" -lt 5 ]]; then
  fail "golden suite must include negative (INVALID) vectors; got $invalid_seen"
fi

echo "BINARY_VECTOR_TESTS_PASS=TRUE"
echo "GOLDEN_VECTORS_RUN=$pass"
echo "GOLDEN_INVALID_VECTORS=$invalid_seen"
echo "BINARY_SEMANTIC_EQUIVALENCE=false"

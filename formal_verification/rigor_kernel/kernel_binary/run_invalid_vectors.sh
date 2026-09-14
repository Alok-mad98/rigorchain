#!/usr/bin/env bash
# Negative golden vectors INVALID-001 .. INVALID-005.
# Proves only: the binary harness detects obvious corruption.
# Does NOT prove binary semantic equivalence.
set -euo pipefail

DIR="$(cd "$(dirname "$0")" && pwd)"
BIN="$DIR/rigor-kernel-checker"
VEC="$DIR/vectors/invalid"

fail() { echo "FAIL: $*" >&2; exit 1; }

[[ -x "$BIN" ]] || fail "rigor-kernel-checker missing; run build.sh"

required=(
  INVALID-001.json
  INVALID-002.json
  INVALID-003.json
  INVALID-004.json
  INVALID-005.json
)

pass=0
report_lines=()
for name in "${required[@]}"; do
  f="$VEC/$name"
  [[ -f "$f" ]] || fail "missing negative vector $name"
  got="$("$BIN" "$f" | tr -d '\r')"
  id="$(python3 -c "import json; print(json.load(open('$f'))['id'])")"
  expect="$(python3 -c "import json; print(json.load(open('$f'))['expect'])")"
  reason="$(python3 -c "import json; print(json.load(open('$f')).get('failure_reason',''))")"
  echo "vector=$id result=$got expect=$expect"
  [[ "$expect" == "INVALID" ]] || fail "$id: expect must be INVALID"
  [[ "$got" == "INVALID" ]] || fail "$id: got $got want INVALID"
  report_lines+=("$id|$got|$expect|$reason")
  pass=$((pass + 1))
done

echo "NEGATIVE_VECTOR_TEST_COMPLETE=TRUE"
echo "NEGATIVE_VECTORS_RUN=$pass"
echo "BINARY_SEMANTIC_EQUIVALENCE=false"

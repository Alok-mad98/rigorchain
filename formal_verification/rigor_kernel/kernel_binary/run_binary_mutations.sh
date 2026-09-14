#!/usr/bin/env bash
# Binary-harness mutations B1–B3.
# These tests do NOT prove binary correctness.
# They only prove the test harness can detect obvious implementation
# corruption of the generated executable artifact.
set -euo pipefail

DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$DIR/.." && pwd)"
EXT="$ROOT/extraction/ExtractedKernel.ml"
MAIN="$DIR/main.ml"

fail() { echo "FAIL: $*" >&2; exit 1; }

[[ -f "$EXT" ]] || fail "ExtractedKernel.ml missing; run extraction/build.sh"
[[ -f "$MAIN" ]] || fail "main.ml missing"
[[ -x "$DIR/rigor-kernel-checker" ]] || fail "build the clean binary first"

EXPECT_FAIL_COUNT=0
MUTATION_PASS_COUNT=0

restore_sources() {
  if [[ -f "$EXT.bak-bmut" ]]; then mv -f "$EXT.bak-bmut" "$EXT"; fi
  if [[ -f "$MAIN.bak-bmut" ]]; then mv -f "$MAIN.bak-bmut" "$MAIN"; fi
}

rebuild() {
  ( cd "$DIR" && ./build.sh ) >/dev/null
}

run_vectors_rc() {
  local rc=0
  ( cd "$DIR" && ./run_vectors.sh ) >/dev/null 2>&1 || rc=$?
  return "$rc"
}

run_bmut() {
  local id="$1"
  local desc="$2"
  shift 2
  # remaining args unused; caller patches before calling

  echo "== Binary mutation $id  $desc =="
  local rc=0
  rebuild
  run_vectors_rc || rc=$?
  restore_sources
  rebuild

  if [[ "$rc" -eq 0 ]]; then
    echo "mutation_passed $id"
    MUTATION_PASS_COUNT=$((MUTATION_PASS_COUNT + 1))
    return 1
  fi
  echo "EXPECTED_FAILURE id=$id artifact=rigor-kernel-checker"
  EXPECT_FAIL_COUNT=$((EXPECT_FAIL_COUNT + 1))
  return 0
}

trap restore_sources EXIT

# --- B1: force validator always return true ---
cp "$MAIN" "$MAIN.bak-bmut"
python3 - "$MAIN" <<'PY'
import pathlib, sys
p = pathlib.Path(sys.argv[1])
text = p.read_text()
old = "  if decide j then print_endline \"VALID\" else print_endline \"INVALID\""
new = "  let _ = decide j in print_endline \"VALID\""
if old not in text:
    sys.stderr.write("B1 pattern not found in main.ml\n")
    sys.exit(2)
p.write_text(text.replace(old, new, 1))
PY
run_bmut B1 "force validator always return true" || true

# --- B2: integer overflow / money-range disabled ---
cp "$EXT" "$EXT.bak-bmut"
python3 - "$EXT" <<'PY'
import pathlib, sys
p = pathlib.Path(sys.argv[1])
text = p.read_text()
old = "let money_range_b v =\n  (&&) (Z.leb 0 v) (Z.leb v mAX_MONEY)"
new = "let money_range_b _ =\n  true"
if old not in text:
    sys.stderr.write("B2 pattern not found in ExtractedKernel.ml\n")
    sys.exit(2)
p.write_text(text.replace(old, new, 1))
PY
run_bmut B2 "integer overflow mutation (money_range_b always true)" || true

# --- B3: ASERT extreme negative time / sign erasure ---
cp "$EXT" "$EXT.bak-bmut"
python3 - "$EXT" <<'PY'
import pathlib, sys
p = pathlib.Path(sys.argv[1])
text = p.read_text()
old = "let exp_q_raw te tau =\n  Z.quot (Z.mul te aSERT_RADIX) tau"
new = "let exp_q_raw te tau =\n  Z.quot (Z.mul (Z.opp te) aSERT_RADIX) tau"
if old not in text:
    sys.stderr.write("B3 pattern not found in ExtractedKernel.ml\n")
    sys.exit(2)
p.write_text(text.replace(old, new, 1))
PY
run_bmut B3 "ASERT extreme negative time mutation (sign erasure)" || true

trap - EXIT
restore_sources
rebuild

echo "---- binary mutation summary ----"
echo "EXPECTED_FAILURES=$EXPECT_FAIL_COUNT"
echo "MUTATION_PASSES=$MUTATION_PASS_COUNT"

if [[ "$MUTATION_PASS_COUNT" -ne 0 ]]; then
  fail "binary mutation_passed is forbidden"
fi
if [[ "$EXPECT_FAIL_COUNT" -ne 3 ]]; then
  fail "expected 3 binary EXPECTED_FAILURE lines, got $EXPECT_FAIL_COUNT"
fi

echo "BINARY_MUTATION_TEST_COMPLETE=TRUE"
echo "BINARY_HARNESS_DETECTS_CORRUPTION=TRUE"
echo "BINARY_SEMANTIC_EQUIVALENCE=false"

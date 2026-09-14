#!/usr/bin/env bash
# Coq compile → extract (faithful Z/nat) → compile OCaml → run vectors.
# Does not claim compiler correctness or full OCaml semantic equivalence.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

fail() { echo "FAIL: $*" >&2; exit 1; }

echo "== Coq compile =="
coq_makefile -f _CoqProject.kernel-critical -o Makefile
make -j"$(nproc 2>/dev/null || echo 2)"
[[ -f Kernel/ExtractionBoundary.vo ]] || fail "ExtractionBoundary.vo missing"

echo "== extract to extracted/ (inductive Z/nat; no ExtrOcamlZInt) =="
(
  cd extracted
  rm -f ExtractedKernel.ml ExtractedKernel.mli
  coqc -R .. RigorKernel Extract.v
)
[[ -f extracted/ExtractedKernel.ml ]] || fail "ExtractedKernel.ml not produced"
if grep -qE '^From Coq Require Import ExtrOcamlZInt' extracted/Extract.v; then
  fail "faithful extract must not import ExtrOcamlZInt"
fi
echo "KERNEL_EXTRACTION=PASS"
echo "EXTRACTION_Z_MAPPING=inductive"

echo "== compile extracted artifact =="
(
  cd extracted
  ocamlopt -alert -deprecated -c ExtractedKernel.mli ExtractedKernel.ml
  ocamlopt -alert -deprecated ExtractedKernel.cmx test_driver.ml \
    -o extracted-kernel-checker
)
[[ -x extracted/extracted-kernel-checker ]] || fail "checker not built"
echo "EXTRACTED_ARTIFACT_BUILT=TRUE"

BIN="$ROOT/extracted/extracted-kernel-checker"
run_vec() {
  local f="$1"
  local expect="$2"
  [[ -f "$f" ]] || fail "missing $f"
  local got
  got="$("$BIN" "$f" | tr -d '\r')"
  echo "vector=$(basename "$f") result=$got expect=$expect"
  [[ "$got" == "$expect" ]] || fail "$(basename "$f"): got $got want $expect"
}

echo "== positive vectors =="
run_vec kernel_binary/test_vectors/utxo_valid.json VALID
run_vec kernel_binary/test_vectors/oru_conservation.json VALID
run_vec kernel_binary/test_vectors/res_schedule.json VALID
run_vec kernel_binary/test_vectors/script_classification.json VALID
run_vec kernel_binary/test_vectors/pow_target.json VALID
run_vec kernel_binary/test_vectors/asert_negative_time.json VALID

echo "== negative vectors INVALID-001..005 =="
run_vec kernel_binary/vectors/invalid/INVALID-001.json INVALID
run_vec kernel_binary/vectors/invalid/INVALID-002.json INVALID
run_vec kernel_binary/vectors/invalid/INVALID-003.json INVALID
run_vec kernel_binary/vectors/invalid/INVALID-004.json INVALID
run_vec kernel_binary/vectors/invalid/INVALID-005.json INVALID

echo "== kernel_binary_vectors V1–V6 (must be INVALID) =="
run_vec kernel_binary_vectors/V1.json INVALID
run_vec kernel_binary_vectors/V2.json INVALID
run_vec kernel_binary_vectors/V3.json INVALID
run_vec kernel_binary_vectors/V4.json INVALID
run_vec kernel_binary_vectors/V5.json INVALID
run_vec kernel_binary_vectors/V6.json INVALID
run_vec kernel_binary_vectors/V7.json INVALID
run_vec kernel_binary_vectors/V8.json INVALID
run_vec kernel_binary_vectors/V9.json INVALID
run_vec kernel_binary_vectors/V10.json INVALID
echo "KERNEL_BINARY_VECTORS_NEGATIVE=PASS"

echo "EXTRACT_AND_TEST=PASS"
echo "NEGATIVE_VECTORS_ON_EXTRACTED=PASS"

echo "== differential verify =="
"$ROOT/scripts/diff_verify.sh"

echo "EXTRACTION_SOURCE_IDENTITY_PROVED=TRUE"
echo "EXTRACTION_FORMAL_PROOF=source_identity_only"
echo "EXTRACTION_EQUIVALENCE_NOT_CLAIMED=TRUE"
echo "PRODUCTION_REFINEMENT_NOT_CLAIMED=TRUE"
echo "BINARY_SEMANTIC_EQUIVALENCE=false"
echo "COMPILER_CORRECTNESS=false"

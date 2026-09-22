#!/usr/bin/env bash
# Independent-auditor one-shot replay of claimed kernel results.
# Writes nothing into the audited tree. All generated files go to
# --out-dir (default /tmp/rigor-kernel-audit-replay).
#
# Not claimed: C++ refinement, CompCert correctness, binary equivalence.
set -euo pipefail

DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$DIR/.." && pwd)"

fail() { echo "FAIL: $*" >&2; exit 1; }

OUTDIR="/tmp/rigor-kernel-audit-replay"
while [[ $# -gt 0 ]]; do
  case "$1" in
    --out-dir)
      [[ $# -ge 2 ]] || fail "--out-dir requires a path"
      OUTDIR="$2"
      shift 2
      ;;
    --out-dir=*)
      OUTDIR="${1#--out-dir=}"
      shift
      ;;
    -h|--help)
      echo "usage: independent_audit_replay.sh [--out-dir DIR]"
      exit 0
      ;;
    *)
      fail "unknown argument: $1"
      ;;
  esac
done

mkdir -p "$OUTDIR"
OUTDIR="$(cd "$OUTDIR" && pwd)"
WORK="$OUTDIR/work"
REPORT="$OUTDIR/IndependentAuditReplayReport.md"

EXPECT_COQC="8.15.0"
EXPECT_OCAML_MAJOR="4.13"

echo "======== independent audit replay ========"
echo "audited_tree=$ROOT"
echo "out_dir=$OUTDIR"
echo "EXTRACTION_FORMAL_PROOF=source_identity_only"
echo "EXTRACTION_EQUIVALENCE_NOT_CLAIMED=TRUE"
echo "PRODUCTION_REFINEMENT_NOT_CLAIMED=TRUE"
echo "COMPILER_CORRECTNESS=false"
echo "BINARY_SEMANTIC_EQUIVALENCE=false"

echo "== toolchain =="
command -v coqc >/dev/null || fail "coqc not on PATH (need Coq $EXPECT_COQC)"
command -v coq_makefile >/dev/null || fail "coq_makefile not on PATH"
command -v coqtop >/dev/null || fail "coqtop not on PATH"
command -v ocamlopt >/dev/null || fail "ocamlopt not on PATH"
command -v python3 >/dev/null || fail "python3 not on PATH"
command -v gcc >/dev/null || fail "gcc not on PATH (C-fragment gcc fallback)"
command -v make >/dev/null || fail "make not on PATH"

COQC_VER="$(coqc --version | head -n1)"
OCAML_VER="$(ocamlopt -version 2>/dev/null || ocamlopt -vnum)"
echo "coqc: $COQC_VER"
echo "ocamlopt: $OCAML_VER"
echo "python3: $(python3 --version 2>&1)"
echo "gcc: $(gcc --version | head -n1)"
echo "make: $(make --version | head -n1)"

echo "$COQC_VER" | grep -q "$EXPECT_COQC" \
  || fail "recorded Coq is $EXPECT_COQC; got: $COQC_VER"
echo "$OCAML_VER" | grep -q "$EXPECT_OCAML_MAJOR" \
  || fail "recorded OCaml is ${EXPECT_OCAML_MAJOR}.x; got: $OCAML_VER"

CCOMP_STATE="ABSENT"
if command -v ccomp >/dev/null 2>&1; then
  CCOMP_STATE="PRESENT"
  echo "ccomp: $(command -v ccomp) (optional; not required)"
else
  echo "ccomp: ABSENT (expected on this host; gcc fallback is the documented result)"
fi

echo "== copy cone to out-dir (audited tree stays unread-written) =="
rm -rf "$WORK"
mkdir -p "$WORK"
if command -v rsync >/dev/null 2>&1; then
  rsync -a \
    --exclude '_build' \
    --exclude '.git' \
    --exclude 'extracted/ExtractedKernel.ml' \
    --exclude 'extracted/ExtractedKernel.mli' \
    --exclude 'extracted/extracted-kernel-checker' \
    --exclude 'extracted/*.cmx' \
    --exclude 'extracted/*.cmi' \
    --exclude 'extracted/*.o' \
    --exclude 'extracted/diff_work' \
    --exclude 'verified_compiler/fragment-checker' \
    --exclude '*.vo' --exclude '*.glob' --exclude '*.vok' --exclude '*.vos' \
    --exclude 'Makefile' \
    "$ROOT/" "$WORK/"
else
  cp -a "$ROOT/." "$WORK/"
  rm -rf "$WORK/_build" \
    "$WORK/extracted/ExtractedKernel.ml" \
    "$WORK/extracted/extracted-kernel-checker" \
    "$WORK/verified_compiler/fragment-checker"
  find "$WORK" \( -name '*.vo' -o -name '*.glob' -o -name 'Makefile' \) -delete
fi

cd "$WORK"

echo "== Coq compile (in out-dir copy) =="
coq_makefile -f _CoqProject.kernel-critical -o Makefile
make -j"$(nproc 2>/dev/null || echo 2)"
[[ -f Kernel/KernelSoundness.vo ]] || fail "KernelSoundness.vo missing"
[[ -f Kernel/VerifiedCompilerBoundary.vo ]] || fail "VerifiedCompilerBoundary.vo missing"

echo "== theorem classification grep + Print Assumptions (whole cone) =="
ASSUME_V="$OUTDIR/print_assumptions.v"
python3 "$WORK/scripts/check_theorem_classification.py" \
  --md "$WORK/release/TheoremClassification.md" \
  --also "$WORK/evidence/TheoremClassification.md" \
  --emit "$ASSUME_V"
ASSUME_OUT="$OUTDIR/print_assumptions.out"
coqtop -R . RigorKernel -R certicoq/surface RigorSurface -batch -l "$ASSUME_V" > "$ASSUME_OUT" 2>&1 || {
  cat "$ASSUME_OUT"
  fail "Print Assumptions failed"
}
if grep -qE 'Axiom|Parameter' "$ASSUME_OUT"; then
  cat "$ASSUME_OUT"
  fail "Print Assumptions is not closed"
fi
CLOSED_COUNT="$(grep -c 'Closed under the global context' "$ASSUME_OUT" || true)"
EXPECTED_CLOSED="$(grep -c '^Print Assumptions' "$ASSUME_V" || true)"
[[ "$CLOSED_COUNT" -eq "$EXPECTED_CLOSED" ]] \
  || fail "expected $EXPECTED_CLOSED Closed theorems, got $CLOSED_COUNT"
echo "ALL_ASSUMPTIONS=CLOSED closed_count=$CLOSED_COUNT (whole cone)"

echo "== extract + vectors + differential verify (in out-dir copy) =="
export RIGOR_KERNEL_OUTDIR="$OUTDIR"
./scripts/extract_and_test.sh

echo "== hand-written C99 fragment rehearsal (in out-dir copy) =="
./scripts/verified_compiler_path.sh

echo "== SourceManifest (read-only, original tree) =="
"$ROOT/release/check_source_manifest.sh"

DIFF_LINE="$(grep -E '^Vectors:' "$OUTDIR/DiffVerifyReport.md" || true)"
{
  echo "# Independent Audit Replay Report"
  echo ""
  echo "One-shot replay of claimed kernel results."
  echo "Generated under \`--out-dir\`; the audited tree was not written."
  echo ""
  echo "**Not a C++ refinement. Not binary semantic equivalence.**"
  echo ""
  echo "\`audited_tree\`: $ROOT"
  echo "\`out_dir\`: $OUTDIR"
  echo "\`coqc\`: $COQC_VER"
  echo "\`ocamlopt\`: $OCAML_VER"
  echo "\`ccomp\`: $CCOMP_STATE"
  echo ""
  echo "$DIFF_LINE"
  echo ""
  echo "\`ALL_ASSUMPTIONS=CLOSED\` (closed_count=$CLOSED_COUNT, whole cone)"
  echo "\`EXTRACT_AND_TEST=PASS\`"
  echo "\`DIFF_VERIFY=PASS\`  (MATCH = implementation agreement only)"
  echo "\`VERIFIED_COMPILER=$CCOMP_STATE\`"
  echo "\`C_FRAGMENT_REHEARSAL=PASS\`"
  echo "\`SOURCE_MANIFEST_COMPLETE=TRUE\`"
  echo "\`REPRODUCIBLE_SOURCE_TREE=TRUE\`"
  echo "\`EXTRACTION_FORMAL_PROOF=source_identity_only\`"
  echo "\`EXTRACTION_EQUIVALENCE_NOT_CLAIMED=TRUE\`"
  echo "\`PRODUCTION_REFINEMENT_NOT_CLAIMED=TRUE\`"
  echo "\`COMPILER_CORRECTNESS=false\`"
  echo "\`BINARY_SEMANTIC_EQUIVALENCE=false\`"
  echo ""
  if [[ "$CCOMP_STATE" == "ABSENT" ]]; then
    echo "CompCert was absent. Documented result: gcc fallback,"
    echo "\`VERIFIED_COMPILER=ABSENT\`, \`C_FRAGMENT_REHEARSAL=PASS\`."
    echo ""
  fi
  echo "INDEPENDENT_AUDIT_REPLAY=PASS"
  echo ""
} > "$REPORT"

echo "wrote $REPORT"
echo "INDEPENDENT_AUDIT_REPLAY=PASS"
echo "======== done ========"

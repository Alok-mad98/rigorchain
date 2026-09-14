#!/usr/bin/env bash
# Hand-written C99 fragment rehearsal (not Coq→C, not a refinement).
#
# Intended future: CompCert-C / Clight from the checkers → ccomp.
# Current host: ccomp is typically absent; gcc is an UNTRUSTED fallback.
#
# This is not Coq→C extraction, not a CompCert refinement, and not
# binary semantic equivalence. Default kernel extract remains OCaml.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

fail() { echo "FAIL: $*" >&2; exit 1; }

SRC="$ROOT/verified_compiler/fragment_checker.c"
OUTDIR="${RIGOR_KERNEL_OUTDIR:-$ROOT/verified_compiler}"
mkdir -p "$OUTDIR"
BIN="$OUTDIR/fragment-checker"
REPORT_DIR="${RIGOR_KERNEL_OUTDIR:-$ROOT/evidence}"
mkdir -p "$REPORT_DIR"
REPORT="$REPORT_DIR/VerifiedCompilerPathReport.md"

[[ -f "$SRC" ]] || fail "missing $SRC"

echo "== hand-written C99 fragment rehearsal =="
COMPILER="gcc"
COMPILER_KIND="untrusted_host_cc"
VERIFIED_PRESENT="ABSENT"
if command -v ccomp >/dev/null 2>&1; then
  COMPILER="ccomp"
  COMPILER_KIND="compcert_ccomp"
  VERIFIED_PRESENT="PRESENT"
  echo "ccomp found: $(command -v ccomp)"
  ccomp -o "$BIN" "$SRC" || fail "ccomp failed"
else
  echo "ccomp not found; using untrusted host gcc (not a verified compiler)"
  gcc -std=c99 -Wall -Wextra -o "$BIN" "$SRC" || fail "gcc failed"
fi
[[ -x "$BIN" ]] || fail "fragment-checker not built"
echo "COMPILER=$COMPILER"
echo "VERIFIED_COMPILER=$VERIFIED_PRESENT"

run_case() {
  local id="$1"
  local expect="$2"
  shift 2
  local got
  got="$("$BIN" "$@" | tr -d '\r')"
  echo "vector=$id result=$got expect=$expect compiler=$COMPILER"
  [[ "$got" == "$expect" ]] || fail "$id: got $got want $expect"
}

echo "== negative vectors INVALID-001..005 =="
# INVALID-001: UTXO value MAX_MONEY+1
run_case INVALID-001 INVALID utxo 1000000000000000001
# INVALID-002: ORU oru_out > oru_in, DomORU
run_case INVALID-002 INVALID oru 7 10 0 0 1
# INVALID-003: RES mint > W(0)
run_case INVALID-003 INVALID res 40000001 0 1 1 1
# INVALID-004: ASERT te<0 but expected_q=0
run_case INVALID-004 INVALID asert -1000000 43200 0
# INVALID-005: PoW compact_t < 0
run_case INVALID-005 INVALID pow 1 100 -1

echo "== positive smoke (comparable fragment only) =="
run_case utxo_valid VALID utxo 1
run_case oru_conservation VALID oru 10 7 0 0 1
run_case res_schedule VALID res 1 0 1 1 1
run_case pow_target VALID pow 1 100 100
run_case asert_negative_time VALID asert -1 43200 -1

echo "== long long vs unbounded Z (documented gap; not a fail) =="
# te = -2^50, tau = 2^50, Coq Z.quot(te * 65536, tau) = -65536 (VALID).
# te * ASERT_RADIX overflows signed long long. The C fragment is not a Z oracle.
TE_OV="-1125899906842624"
TAU_OV="1125899906842624"
Q_Z="-65536"
GOT_OV="$("$BIN" asert "$TE_OV" "$TAU_OV" "$Q_Z" | tr -d '\r')"
echo "vector=asert_llong_overflow C_fragment=$GOT_OV coq_Z_would=VALID"
echo "note: te*ASERT_RADIX overflows signed long long; Coq Z does not."
echo "LONG_LONG_VS_Z=DOCUMENTED"
if [[ "$GOT_OV" == "VALID" ]]; then
  echo "note: this host wrapped to the Coq value; the overflow is still UB / wrap, not a proof"
fi

{
  echo "# Verified Compiler Path Report"
  echo ""
  echo "Hand-written C99 fragment rehearsal of \`verified_compiler/fragment_checker.c\`"
  echo "on INVALID-001..005 plus five positive smoke cases."
  echo ""
  echo "**Not a CompCert refinement of the Coq extract.**"
  echo "**Not binary semantic equivalence.**"
  echo ""
  echo "\`VERIFIED_COMPILER=$VERIFIED_PRESENT\`"
  echo "\`COMPILER=$COMPILER\` ($COMPILER_KIND)"
  echo "\`C_FRAGMENT_REHEARSAL=PASS\`"
  echo "\`COMPILER_CORRECTNESS=false\`"
  echo "\`BINARY_SEMANTIC_EQUIVALENCE=false\`"
  echo "\`EXTRACTION_FORMAL_PROOF=source_identity_only\`"
  echo "\`EXTRACTION_EQUIVALENCE_NOT_CLAIMED=TRUE\`"
  echo "\`PRODUCTION_REFINEMENT_NOT_CLAIMED=TRUE\`"
  echo ""
  echo "Default executable specification remains the faithful OCaml extract"
  echo "(\`scripts/extract_and_test.sh\`). This C file is a hand-written C99"
  echo "subset rehearsal. Coq rejects the same five vectors"
  echo "(\`Kernel/VerifiedCompilerBoundary.v\`, \`negative_vectors_rejected_in_coq\`)."
  echo ""
  echo "Known limit (live): \`asert_llong_overflow\` — C_fragment=$GOT_OV,"
  echo "Coq Z would be VALID. Fixed-width \`long long\` is not unbounded \`Z\`."
  echo ""
  echo "See \`release/VerifiedCompilerPath.md\`."
  echo ""
  echo "C_FRAGMENT_REHEARSAL=PASS"
  echo ""
} > "$REPORT"

echo "wrote $REPORT"
echo "VERIFIED_COMPILER=$VERIFIED_PRESENT"
echo "C_FRAGMENT_REHEARSAL=PASS"
echo "VERIFIED_COMPILER_FORCED_SWITCH=FALSE"
echo "COMPILER_CORRECTNESS=false"
echo "BINARY_SEMANTIC_EQUIVALENCE=false"
echo "EXTRACTION_FORMAL_PROOF=source_identity_only"
echo "EXTRACTION_EQUIVALENCE_NOT_CLAIMED=TRUE"
echo "PRODUCTION_REFINEMENT_NOT_CLAIMED=TRUE"

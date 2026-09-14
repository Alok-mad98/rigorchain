#!/usr/bin/env bash
# Differential verification: extracted Coq checker vs production-logic shadow.
# Comparable fragment only. Not a production-code refinement proof.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

fail() { echo "FAIL: $*" >&2; exit 1; }

BIN="$ROOT/extracted/extracted-kernel-checker"
if [[ ! -x "$BIN" ]]; then
  fail "extracted-kernel-checker missing; run scripts/extract_and_test.sh first"
fi

echo "== differential verify (extracted spec vs production-logic shadow) =="
python3 "$ROOT/scripts/diff_oracle.py"
echo "PRODUCTION_REFINEMENT_NOT_CLAIMED=TRUE"
echo "EXTRACTION_FORMAL_PROOF=source_identity_only"
echo "EXTRACTION_EQUIVALENCE_NOT_CLAIMED=TRUE"

#!/usr/bin/env bash
# Freeze artifact check — works from any directory.
#
# The relative paths formal_verification/coq/... are from the *repo root*.
# Running those ls commands after `cd formal_verification/coq` will print missing
# even when the files exist. Use this script instead:
#
#   bash formal_verification/scripts/check_freeze_artifacts.sh
#   bash /path/to/rigor-official-tree/formal_verification/scripts/check_freeze_artifacts.sh
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

fail=0
check() {
  local rel="$1"
  if [[ -e "$ROOT/$rel" ]]; then
    ls -l "$ROOT/$rel"
  else
    echo "missing: $rel"
    fail=1
  fi
}

echo "=== critical file check (repo root: $ROOT) ==="
check formal_verification/coq/CanonicalHashKernel.v
check formal_verification/coq/InvalidCvg.v
check formal_verification/coq/ScriptMintWitness.v
check formal_verification/coq/ResSchedule.v
check formal_verification/coq/Kernel/NegativeCoreHarness.v
echo "SKIP: lottery DrawBridgeProgress / GoldenHarness / CanonicalDrawEncoder / WindowExclusion / DrawCandidateRoot / ReplayReorgCore (Batch A)"
check formal_verification/scripts/run_differential_gate.sh
check formal_verification/scripts/check_golden_lock.py
check formal_verification/golden_lock.json
check docs/verification/FORMAL-VERIFICATION-STATUS.md
check docs/verification/FORMAL-VERIFICATION-FREEZE.md
echo "SKIP: CURRENT_STATE / GOLDEN-VECTOR-DISCREPANCIES / STATUS-2026-08-16-R2 live copies were archived (not lottery restore)"
check docs/verification/FV-RELEASE-CHECKLIST.md
check formal_verification/scripts/run_fv_release_checklist.sh

echo
echo "=== honesty-flag check ==="
echo "SKIP: DrawBridgeProgress.v honesty lock removed with lottery (Batch A)"
README_FV="$ROOT/formal_verification/README.md"
if ! grep -q 'equivalence_claimed = false' "$README_FV"; then
  echo "FAIL: equivalence_claimed is not false in $README_FV"
  fail=1
fi
if ! grep -q 'hash_kernel_shared_binary_with_cpp = false' "$README_FV"; then
  echo "FAIL: hash_kernel_shared_binary_with_cpp is not false in $README_FV"
  fail=1
fi
if ! grep -q 'refinement_proved = false' "$README_FV"; then
  echo "FAIL: refinement_proved is not false in $README_FV"
  fail=1
fi
grep -n 'equivalence_claimed = false\|hash_kernel_shared_binary_with_cpp = false\|refinement_proved = false' "$README_FV" | head -n 6

echo "SKIP: archived CURRENT_STATE / lottery-era 414/207 and 422/215 count pins (Batch A remaining shared_vectors=127)"
echo "Honesty flags required false in formal_verification/README.md (checked above)."

echo
if [[ "$fail" -ne 0 ]]; then
  echo "freeze artifact check: FAIL"
  exit 1
fi
echo "freeze artifact check: PASS"
echo "Note: paths are relative to the repo root, not to formal_verification/coq/."

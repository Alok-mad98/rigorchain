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
check formal_verification/coq/CanonicalDrawEncoder.v
check formal_verification/coq/DrawCandidateRoot.v
check formal_verification/coq/InvalidCvg.v
check formal_verification/coq/ScriptMintWitness.v
check formal_verification/coq/ReplayReorgCore/ScriptShape.v
check formal_verification/coq/ReplayReorgCore/ScriptShapeLift.v
check formal_verification/coq/WindowExclusion.v
check formal_verification/coq/DrawBridgeProgress.v
check formal_verification/coq/Phase5Equivalence/DrawBridgeProgress.v
check formal_verification/coq/Kernel/GoldenHarness.v
check formal_verification/coq/Kernel/NegativeCoreHarness.v
check formal_verification/coq/ReplayReorgCore/ReplayState.v
check formal_verification/coq/ReplayReorgCore/OpaqueBoundary.v
check formal_verification/coq/ReplayReorgCore/ReplayInvariants.v
check formal_verification/scripts/run_differential_gate.sh
check formal_verification/scripts/check_golden_lock.py
check formal_verification/golden_lock.json
check docs/verification/FORMAL-VERIFICATION-STATUS.md
check docs/verification/FORMAL-VERIFICATION-FREEZE.md
check docs/verification/FORMAL-VERIFICATION-STATUS-2026-08-16-R2.md
check docs/formal/RIGOR_FORMAL_VERIFICATION_CURRENT_STATE.md
check docs/verification/GOLDEN-VECTOR-DISCREPANCIES.md
check docs/verification/FV-RELEASE-CHECKLIST.md
check formal_verification/scripts/run_fv_release_checklist.sh

echo
echo "=== honesty-flag check ==="
LOCK="$ROOT/formal_verification/coq/DrawBridgeProgress.v"
if grep -q 'equivalence_claimed' "$LOCK" && grep -q 'hash_kernel_shared_binary_with_cpp' "$LOCK"; then
  grep -n 'equivalence_claimed\|hash_kernel_shared_binary_with_cpp' "$LOCK"
else
  echo "missing: honesty flags in DrawBridgeProgress.v"
  fail=1
fi
if ! grep -q 'equivalence_claimed = false' "$LOCK"; then
  echo "FAIL: equivalence_claimed is not false in $LOCK"
  fail=1
fi
if ! grep -q 'hash_kernel_shared_binary_with_cpp = false' "$LOCK"; then
  echo "FAIL: hash_kernel_shared_binary_with_cpp is not false in $LOCK"
  fail=1
fi
PHASE5="$ROOT/formal_verification/coq/Phase5Equivalence/DrawBridgeProgress.v"
if ! grep -q 'refinement_proved : bool := false' "$PHASE5"; then
  echo "FAIL: refinement_proved is not false in $PHASE5"
  fail=1
fi

CS="$ROOT/docs/formal/RIGOR_FORMAL_VERIFICATION_CURRENT_STATE.md"
# Production CURRENT_STATE dropped historical TASK-FV-* ids. Require the
# living sections and pinned counts instead of those retired labels.
if ! grep -q 'RC0 freeze declaration' "$CS"; then
  echo "FAIL: RC0 freeze declaration missing from CURRENT_STATE"
  fail=1
fi
if ! grep -q '414' "$CS" || ! grep -q '207' "$CS"; then
  echo "FAIL: RC0 freeze counts 414/207 missing from CURRENT_STATE"
  fail=1
fi
if ! grep -q '422' "$CS" || ! grep -q '215' "$CS"; then
  echo "FAIL: living densify counts 422/215 missing from CURRENT_STATE"
  fail=1
fi
if ! grep -q 'FV release checklist' "$CS"; then
  echo "FAIL: FV release checklist missing from CURRENT_STATE"
  fail=1
fi
for flag in equivalence_claimed hash_kernel_shared_binary_with_cpp refinement_proved; do
  if ! grep -q "${flag} = false" "$CS"; then
    echo "FAIL: ${flag} is not stated false in CURRENT_STATE"
    fail=1
  fi
done

echo
if [[ "$fail" -ne 0 ]]; then
  echo "freeze artifact check: FAIL"
  exit 1
fi
echo "freeze artifact check: PASS"
echo "Note: paths are relative to the repo root, not to formal_verification/coq/."

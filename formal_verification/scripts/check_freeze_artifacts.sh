#!/usr/bin/env bash
# Freeze artifact check — works from any directory.
#
# The relative paths formal_verification/coq/... are from the *repo root*.
# Running those ls commands after `cd formal_verification/coq` will print 缺失
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
    echo "缺失: $rel"
    fail=1
  fi
}

echo "=== 关键文件检查 (repo root: $ROOT) ==="
check formal_verification/coq/CanonicalHashKernel.v
check formal_verification/coq/CanonicalDrawEncoder.v
check formal_verification/coq/WindowExclusion.v
check formal_verification/coq/DrawBridgeProgress.v
check formal_verification/coq/Phase5Equivalence/DrawBridgeProgress.v
check formal_verification/coq/Kernel/GoldenHarness.v
check formal_verification/scripts/run_differential_gate.sh
check docs/verification/FORMAL-VERIFICATION-STATUS.md
check docs/verification/FORMAL-VERIFICATION-STATUS-2026-08-16-R2.md

echo
echo "=== 诚实声明检查 ==="
LOCK="$ROOT/formal_verification/coq/DrawBridgeProgress.v"
if grep -q 'equivalence_claimed' "$LOCK" && grep -q 'hash_kernel_shared_binary_with_cpp' "$LOCK"; then
  grep -n 'equivalence_claimed\|hash_kernel_shared_binary_with_cpp' "$LOCK"
else
  echo "缺失: honesty flags in DrawBridgeProgress.v"
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

echo
if [[ "$fail" -ne 0 ]]; then
  echo "freeze artifact check: FAIL"
  exit 1
fi
echo "freeze artifact check: PASS"
echo "Note: paths are relative to the repo root, not to formal_verification/coq/."

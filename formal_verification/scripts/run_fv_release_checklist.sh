#!/usr/bin/env bash
# FV release checklist runner — TASK-FV-OPS-REGRESSION-GUARD-001
#
# Default (from repo root or anywhere):
#   bash formal_verification/scripts/run_fv_release_checklist.sh
#
# Runs freeze artifacts then the differential gate with SKIP_TEST_BITCOIN=1
# (gate step 2b already runs check_golden_lock.py).
#
# Environment:
#   FV_COQ=1           pass COQ=1 to the gate (Coq 8.15.0 harness rebuild)
#   FV_EXTRACT=1       pass EXTRACT=1 (re-extract OCaml hash kernel)
#   FV_TEST_BITCOIN=1  do not skip BOOST test_bitcoin
#
# This is not a refinement proof. Honesty flags must stay false.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

echo "======== FV release checklist ========"
echo "lock: formal_verification/golden_lock.json (living 422/215)"
echo "docs: docs/verification/FV-RELEASE-CHECKLIST.md"
echo

echo "== freeze artifacts =="
bash "$ROOT/formal_verification/scripts/check_freeze_artifacts.sh"
echo

export SKIP_TEST_BITCOIN="${SKIP_TEST_BITCOIN:-1}"
if [[ "${FV_TEST_BITCOIN:-0}" == "1" ]]; then
  export SKIP_TEST_BITCOIN=0
fi
if [[ "${FV_COQ:-0}" == "1" ]]; then
  export COQ=1
fi
if [[ "${FV_EXTRACT:-0}" == "1" ]]; then
  export EXTRACT=1
fi

echo "== differential gate (SKIP_TEST_BITCOIN=${SKIP_TEST_BITCOIN} COQ=${COQ:-0} EXTRACT=${EXTRACT:-0}) =="
bash "$ROOT/formal_verification/scripts/run_differential_gate.sh"

echo
echo "======== FV release checklist: PASS ========"
echo "Honest status: not a refinement proof."
echo "  equivalence_claimed=false  hash_kernel_shared_binary_with_cpp=false  refinement_proved=false"
echo "See docs/verification/FV-RELEASE-CHECKLIST.md"

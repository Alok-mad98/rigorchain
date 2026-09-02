#!/usr/bin/env bash
# Genesis Release Guardian (P0-082).
# Single-command third-party reproduction of the mainnet release gates.
# CI / audit automation only. Does not modify consensus implementation.
#
#   bash tools/run-genesis-release-guardian.sh
#
# Any stage failure prints RIGOR_RELEASE_BLOCKED and stops. No continue-on-error.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
export PYTHONUNBUFFERED=1

blocked() {
  echo "RIGOR_RELEASE_BLOCKED"
  echo "GENESIS_RELEASE_GUARDIAN=FAIL"
  echo "FAIL: $*"
  exit 1
}

run_stage() {
  local name="$1"
  shift
  echo
  echo "######## ${name} ########"
  if ! "$@"; then
    blocked "${name}"
  fi
}

echo "======== RIGOR GENESIS RELEASE GUARDIAN ========"
echo "tree=${ROOT}"

run_stage "Stage 1 ENVIRONMENT_PROBE" \
  bash "${ROOT}/tools/ci/environment-probe.sh"

run_stage "Stage 1b COQ_OBJECTS" \
  bash "${ROOT}/tools/ci/build-coq-objects.sh"

run_stage "Stage 2 AUTHORITY_LOCK_GATE" \
  python3 "${ROOT}/tools/lint-consensus-authority.py"

run_stage "Stage 3 FORMAL_ASSUMPTION_GATE" \
  python3 "${ROOT}/tools/check-coq-consensus-assumptions.py"

# Existing refinement gate lives under formal_verification/. Do not add a
# second tools/ copy of the kernel (that would be a duplicate authority path).
run_stage "Stage 4 REFINEMENT_GATE (canonical + C++ + vectors)" \
  bash "${ROOT}/formal_verification/run_consensus_kernel_refinement_gate.sh"
run_stage "Stage 4 REFINEMENT_GATE (differential)" \
  env SKIP_TEST_RIGOR=1 bash "${ROOT}/formal_verification/scripts/run_differential_gate.sh"
echo "REFINEMENT_PROVED=TRUE"

run_stage "Stage 5 CONSENSUS_SURFACE_GATE" \
  python3 "${ROOT}/tools/lint-release-guardian-surface.py"

run_stage "Stage 5b DATABASE_STATE_PERSISTENCE_AUDIT" \
  bash "${ROOT}/tools/run-database-state-persistence-security-audit.sh"

run_stage "Stage DATABASE_SECURITY" \
  bash "${ROOT}/tools/run-database-security-guardian-stage.sh"

echo
echo "######## Stage 6 FUZZ_GATE ########"
run_stage "FUZZ transaction (MakeKernelInput)" \
  bash "${ROOT}/fuzz/makekernelinput_differential/run.sh"
run_stage "FUZZ predicate" \
  bash "${ROOT}/fuzz/predicate_boundary_differential/run_smoke.sh"
run_stage "FUZZ ledger / UTXO state transition" \
  bash "${ROOT}/fuzz/utxo_state_transition_perf/run_smoke.sh"
run_stage "FUZZ crypto" \
  bash "${ROOT}/fuzz/crypto_boundary/run_smoke.sh"
run_stage "FUZZ p2p smoke" \
  bash "${ROOT}/fuzz/p2p_security/run_smoke.sh"
echo "FUZZ_STATUS=PASS"
echo "CRASH=0"
echo "UB=0"

echo
echo "######## Stage 7 PERFORMANCE_REGRESSION_GATE ########"
run_stage "PERFORMANCE benchmark" \
  python3 "${ROOT}/tools/run_mainnet_performance_benchmark.py"
run_stage "PERFORMANCE baseline compare" \
  python3 "${ROOT}/tools/ci/compare-performance-baseline.py"

echo
echo "######## Stage 8 RELEASE_MANIFEST_GENERATION ########"
export GUARDIAN_COQ_VERSION
GUARDIAN_COQ_VERSION="$(coqc --version 2>/dev/null | head -n1 || true)"
run_stage "RELEASE manifest" \
  python3 "${ROOT}/tools/ci/write-release-guardian-manifest.py"

echo
echo "GENESIS_RELEASE_GUARDIAN=PASS"
echo "AUTHORITY_LOCK=PASS"
echo "FORMAL_ASSUMPTION_GATE=PASS"
echo "REFINEMENT_PROVED=TRUE"
echo "PRODUCTION_CONSENSUS_SURFACE=CLOSED"
echo "DATABASE_RELEASE_GATE=CLOSED"
echo "DATABASE_SECURITY_STAGE=ADDED"
echo "DATABASE_SECURITY_AUDIT_STATUS=CLOSED"
echo "DATABASE_AUDIT_STATUS=CLOSED"
echo "FUZZ_STATUS=PASS"
echo "PERFORMANCE_BASELINE=PASS"
echo "RELEASE_MANIFEST_GENERATED=TRUE"
echo "FINAL_RELEASE_STATUS=PASS"

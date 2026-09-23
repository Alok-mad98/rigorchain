#!/usr/bin/env bash
# Copyright (c) 2026 The RigorChain developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
#
# Pre-mainnet functional security runner (solo mining; no pool).
#
# Lottery window / draw suites were removed in Batch B. Remaining:
#   1. Solo-mining path  — feature_res_solo_security.py
#   2. ORU coinbase maturity — feature_res_maturity.py
# Body mint: feature_res_destroy_cost_mint.py (default runner, not this script).
set -euo pipefail

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
  sed -n '2,16p' "$0" | sed 's/^# \?//'
  exit 0
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CORE_DIR="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
cd "${CORE_DIR}"
if [[ ! -f test/functional/feature_res_solo_security.py ]]; then
  echo "error: missing feature_res_solo_security.py" >&2
  exit 1
fi
if [[ ! -f test/functional/feature_res_maturity.py ]]; then
  echo "error: missing feature_res_maturity.py" >&2
  exit 1
fi

section() {
  echo
  echo "======== $* ========"
}

run_py() {
  local label="$1"
  shift
  section "$label"
  echo "cwd=${CORE_DIR}"
  echo "cmd: python3 $*"
  python3 "$@"
  echo "PASS: $label"
}

run_py "1/2 Solo-mining security (no pool)" \
  test/functional/feature_res_solo_security.py --descriptors "$@"

run_py "2/2 ORU coinbase maturity" \
  test/functional/feature_res_maturity.py --descriptors "$@"

SECURITY_UNIT_SUITES="res_orum_tests,pow_tests,sighash_tests,script_asset_ownership_freeze_tests,res_proof_math_tests,convergence_root_tests"

section "Optional unit: ${SECURITY_UNIT_SUITES}"
if [[ "${SKIP_UNIT:-0}" == "1" ]]; then
  echo "skipped (SKIP_UNIT=1)"
elif [[ -n "${TEST_RIGOR:-${TEST_BITCOIN:-}}" && -x "${TEST_RIGOR:-${TEST_BITCOIN}}" ]]; then
  UNIT_BIN="${TEST_RIGOR:-${TEST_BITCOIN}}"
  "${UNIT_BIN}" --run_test="${SECURITY_UNIT_SUITES}"
  echo "PASS: ${SECURITY_UNIT_SUITES}"
elif [[ -x "${CORE_DIR}/src/test/test_rigor" ]]; then
  "${CORE_DIR}/src/test/test_rigor" --run_test="${SECURITY_UNIT_SUITES}"
  echo "PASS: ${SECURITY_UNIT_SUITES}"
else
  echo "skipped (no test_rigor)"
fi

echo
echo "======== pre-mainnet functional suites: PASS ========"
echo "  solo-mining security"
echo "  ORU maturity"
echo "Lottery window / draw suites: removed (Batch B)."

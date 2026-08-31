#!/usr/bin/env bash
# Copyright (c) 2026 The RigorChain developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
#
# Primary pre-mainnet functional security runner (solo mining; no pool).
#
# Runs, in order, fail-fast on first failure:
#   1. Window / reorg / RES monopoly  — feature_res_window_reorg_res_attacks.py
#   2. Solo-mining path               — feature_res_solo_security.py
#   3. RES mint + ORU coinbase maturity — feature_res_maturity.py
#
# Usage (from anywhere; script cds to core/):
#   ./test/functional/res_security/run_pre_mainnet.sh
#   bash test/functional/res_security/run_pre_mainnet.sh   # extra args forwarded to each test
# Each suite already receives --descriptors; do not omit maturity.
#
# Required build (from core/): wallet-enabled rigord (or bitcoind) used by the
# functional framework. Typical: configure with wallet, then make.
# Chain is mockable devtest; no public network.
#
# Optional unit step (not a Coq rebuild; skip if binary missing):
#   SKIP_UNIT=1     do not run test_rigor
#   TEST_RIGOR=path  override Boost binary (default: core/src/test/test_rigor)
#
# Security unit suites (checklist §D). Comma list is the Boost --run_test filter.
#
# Formal verification is a separate gate:
#   bash formal_verification/scripts/run_differential_gate.sh
#
# Operator checklist: docs/security/PRE-MAINNET-CHECKLIST.md
set -euo pipefail

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
  sed -n '2,28p' "$0" | sed 's/^# \?//'
  exit 0
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# res_security/ -> functional/ -> test/ -> core/
CORE_DIR="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
cd "${CORE_DIR}"
if [[ ! -f test/functional/feature_res_window_reorg_res_attacks.py ]]; then
  echo "error: expected to run from core/; got ${CORE_DIR}" >&2
  exit 1
fi
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

run_py "1/3 Window / reorg / RES monopoly" \
  test/functional/feature_res_window_reorg_res_attacks.py --descriptors "$@"

run_py "2/3 Solo-mining security (no pool)" \
  test/functional/feature_res_solo_security.py --descriptors "$@"

run_py "3/3 RES mint + ORU coinbase maturity" \
  test/functional/feature_res_maturity.py --descriptors "$@"

# Keep in lockstep with docs/security/PRE-MAINNET-CHECKLIST.md §D.
SECURITY_UNIT_SUITES="res_orum_tests,pow_tests,sighash_tests,script_asset_ownership_freeze_tests,res_proof_math_tests,convergence_root_tests"

section "Optional unit: ${SECURITY_UNIT_SUITES}"
if [[ "${SKIP_UNIT:-0}" == "1" ]]; then
  echo "skipped (SKIP_UNIT=1). Checklist D: ./src/test/test_rigor --run_test=${SECURITY_UNIT_SUITES}"
elif [[ -n "${TEST_RIGOR:-${TEST_BITCOIN:-}}" && -x "${TEST_RIGOR:-${TEST_BITCOIN}}" ]]; then
  UNIT_BIN="${TEST_RIGOR:-${TEST_BITCOIN}}"
  echo "TEST_RIGOR=${UNIT_BIN}"
  "${UNIT_BIN}" --run_test="${SECURITY_UNIT_SUITES}"
  echo "PASS: ${SECURITY_UNIT_SUITES}"
elif [[ -x "${CORE_DIR}/src/test/test_rigor" ]]; then
  echo "auto-detected ${CORE_DIR}/src/test/test_rigor"
  "${CORE_DIR}/src/test/test_rigor" --run_test="${SECURITY_UNIT_SUITES}"
  echo "PASS: ${SECURITY_UNIT_SUITES}"
elif [[ -x "${CORE_DIR}/src/test/test_bitcoin" ]]; then
  echo "auto-detected leftover ${CORE_DIR}/src/test/test_bitcoin (rebuild as test_rigor)"
  "${CORE_DIR}/src/test/test_bitcoin" --run_test="${SECURITY_UNIT_SUITES}"
  echo "PASS: ${SECURITY_UNIT_SUITES}"
else
  echo "skipped (no test_rigor). Checklist D:"
  echo "  cd ${CORE_DIR} && ./src/test/test_rigor --run_test=${SECURITY_UNIT_SUITES}"
fi

echo
echo "======== pre-mainnet functional suites: PASS ========"
echo "  window / reorg / RES monopoly"
echo "  solo-mining security"
echo "  RES/ORU maturity"
echo "Pool / stratum / web: out of scope (not run)."
echo "FV gate (separate): bash formal_verification/scripts/run_differential_gate.sh"
echo "Checklist: docs/security/PRE-MAINNET-CHECKLIST.md"

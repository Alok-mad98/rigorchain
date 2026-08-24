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
#   SKIP_UNIT=1     do not run test_bitcoin
#   TEST_BITCOIN=path  override Boost binary (default: core/src/test/test_bitcoin)
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

section "Optional unit: res_orum_tests"
if [[ "${SKIP_UNIT:-0}" == "1" ]]; then
  echo "skipped (SKIP_UNIT=1). Checklist D: ./src/test/test_bitcoin --run_test=res_orum_tests"
elif [[ -n "${TEST_BITCOIN:-}" && -x "${TEST_BITCOIN}" ]]; then
  echo "TEST_BITCOIN=${TEST_BITCOIN}"
  "${TEST_BITCOIN}" --run_test=res_orum_tests
  echo "PASS: res_orum_tests"
elif [[ -x "${CORE_DIR}/src/test/test_bitcoin" ]]; then
  echo "auto-detected ${CORE_DIR}/src/test/test_bitcoin"
  "${CORE_DIR}/src/test/test_bitcoin" --run_test=res_orum_tests
  echo "PASS: res_orum_tests"
else
  echo "skipped (no test_bitcoin). Checklist D:"
  echo "  cd ${CORE_DIR} && ./src/test/test_bitcoin --run_test=res_orum_tests"
fi

echo
echo "======== pre-mainnet functional suites: PASS ========"
echo "  window / reorg / RES monopoly"
echo "  solo-mining security"
echo "  RES/ORU maturity"
echo "Pool / stratum / web: out of scope (not run)."
echo "FV gate (separate): bash formal_verification/scripts/run_differential_gate.sh"
echo "Checklist: docs/security/PRE-MAINNET-CHECKLIST.md"

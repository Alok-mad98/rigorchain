#!/usr/bin/env bash
# P0-091: Release Guardian Stage DATABASE_SECURITY.
# CI / audit automation only. Does not modify Consensus Kernel,
# Transaction Kernel, CoinsDB serialization, Coin key format, Scheme A CRC,
# wallet schema, or add ORU_DB / RES_DB / an asset column.
#
# Executes the production reindex reconstruction audit (P0-088) and the
# wallet SQLite audit (P0-089). Parses required tokens from their output.
# A BLOCKED or OPEN run that exits 0 is not a PASS.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
export PYTHONUNBUFFERED=1

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

require_line() {
  local file="$1"
  local token="$2"
  if ! grep -q "^${token}$" "$file"; then
    echo "FAIL: Stage DATABASE_SECURITY requires ${token}"
    echo "---- captured output (tail) ----"
    tail -n 80 "$file" || true
    echo "RIGOR_RELEASE_BLOCKED"
    exit 1
  fi
}

echo "======== Stage DATABASE_SECURITY ========"
echo "tree=${ROOT}"

echo
echo "== P0-088 production reindex reconstruction =="
set +e
bash "${ROOT}/tools/run-production-reindex-reconstruction-audit.sh" | tee "${TMP}/reindex.txt"
reindex_rc=${PIPESTATUS[0]}
set -e
require_line "${TMP}/reindex.txt" "PRODUCTION_FULL_REINDEX=PASS"
if [[ "${reindex_rc}" -ne 0 ]]; then
  echo "FAIL: P0-088 exited ${reindex_rc} after printing PRODUCTION_FULL_REINDEX=PASS"
  echo "RIGOR_RELEASE_BLOCKED"
  exit 1
fi

echo
echo "== P0-089 wallet SQLite audit =="
set +e
bash "${ROOT}/tools/run-wallet-sqlite-audit.sh" | tee "${TMP}/wallet.txt"
wallet_rc=${PIPESTATUS[0]}
set -e
require_line "${TMP}/wallet.txt" "WALLET_SQLITE_AUDIT_STATUS=CLOSED"
if [[ "${wallet_rc}" -ne 0 ]]; then
  echo "FAIL: P0-089 exited ${wallet_rc} after printing WALLET_SQLITE_AUDIT_STATUS=CLOSED"
  echo "RIGOR_RELEASE_BLOCKED"
  exit 1
fi

echo
echo "PRODUCTION_FULL_REINDEX=PASS"
echo "WALLET_SQLITE_AUDIT_STATUS=CLOSED"
echo "DATABASE_SECURITY_STAGE=ADDED"
echo "DATABASE_SECURITY_AUDIT_STATUS=CLOSED"
echo "DATABASE_AUDIT_STATUS=CLOSED"
echo "DATABASE_SECURITY_STAGE_RESULT=PASS"

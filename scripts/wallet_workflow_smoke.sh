#!/usr/bin/env bash
# Wallet workflow smoke helper — documents required RPC sequence.
# Requires a running rigord with wallet RPC and rigor-cli in PATH.
# Usage: RIGOR_CLI=rigor-cli ./scripts/wallet_workflow_smoke.sh
set -euo pipefail

CLI="${RIGOR_CLI:-rigor-cli}"
WNAME="${WALLET_NAME:-smoke_main}"
BACKUP="${BACKUP_PATH:-/tmp/rigor-wallet-smoke-backup.dat}"

echo "== createwallet (ignore error if exists) =="
${CLI} createwallet "${WNAME}" || true

echo "== receive ORU / RES =="
ORU_ADDR=$(${CLI} -rpcwallet="${WNAME}" getnewaddress "" "ORU")
RES_ADDR=$(${CLI} -rpcwallet="${WNAME}" getnewaddress "" "RES")
echo "ORU=${ORU_ADDR}"
echo "RES=${RES_ADDR}"

echo "== backupwallet =="
${CLI} -rpcwallet="${WNAME}" backupwallet "${BACKUP}"
ls -la "${BACKUP}"

echo "== RPCs present (help) =="
${CLI} help sendtoaddress >/dev/null
${CLI} help sendres >/dev/null
${CLI} help createconvergence >/dev/null
${CLI} help restorewallet >/dev/null

cat <<EOF
Smoke structural PASS.
Fund ${ORU_ADDR}, then manually:
  ${CLI} -rpcwallet=${WNAME} sendtoaddress <oru_peer> 0.01
  ${CLI} -rpcwallet=${WNAME} createconvergence 1.0
  ${CLI} -rpcwallet=${WNAME} sendres <res_peer> 0.01
Restore check:
  ${CLI} restorewallet smoke_restored ${BACKUP}
See docs/wallet/WALLET_USER_WORKFLOW_GUIDE.md
EOF

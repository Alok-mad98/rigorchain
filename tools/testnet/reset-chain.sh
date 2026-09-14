#!/usr/bin/env bash
# Wipe per-node datadirs. Does not copy a chain between nodes.
set -euo pipefail
# shellcheck source=common.sh
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"
bash "${TESTNET_TOOLS}/stop-all.sh"
rm -rf "${DATA_ROOT}/node-001" "${DATA_ROOT}/node-002" "${DATA_ROOT}/node-003"
rm -f "${DATA_ROOT}"/node-00*.stdout "${DATA_ROOT}"/node-00*.stderr "${DATA_ROOT}"/node-00*.pid
echo "RESET_CHAIN=PASS"
echo "Each next start begins at genesis in its own datadir."

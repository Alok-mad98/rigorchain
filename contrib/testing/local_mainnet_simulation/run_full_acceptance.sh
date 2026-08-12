#!/usr/bin/env bash
# Full local production acceptance: mesh → ORU → Convergence W=32 → RES mint → reorg → reports.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=common.sh
source "${SCRIPT_DIR}/common.sh"
lms_require_binaries
mkdir -p "${LMS_OUT}" "${LMS_DOCS}"
MAX_ROUNDS="${LMS_MAX_MINT_ROUNDS:-120}"
echo "==> TASK-LOCAL-MAINNET-SIMULATION-001 full acceptance"
echo "    runtime=${LMS_RUNTIME}"
echo "    out=${LMS_OUT}"
echo "    docs=${LMS_DOCS}"
python3 "${SCRIPT_DIR}/run_simulation.py" --full --wipe --max-mint-rounds "${MAX_ROUNDS}"
echo "==> Reports written under ${LMS_DOCS}"

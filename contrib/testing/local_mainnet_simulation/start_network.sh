#!/usr/bin/env bash
# Start Node-0 (miner+wallet), Node-1 (validator), Node-2 (observer).
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=common.sh
source "${SCRIPT_DIR}/common.sh"
lms_require_binaries
mkdir -p "${LMS_OUT}" "${LMS_RUNTIME}"
WIPE_FLAG=()
if [[ "${1:-}" == "--wipe" ]]; then
  WIPE_FLAG=(--wipe)
fi
exec python3 "${SCRIPT_DIR}/run_simulation.py" --start-only "${WIPE_FLAG[@]}"

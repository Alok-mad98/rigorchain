#!/usr/bin/env bash
# Verify wallet RES balances and chain convergence status (no manual mint).
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=common.sh
source "${SCRIPT_DIR}/common.sh"
lms_require_binaries
exec python3 "${SCRIPT_DIR}/run_simulation.py" --verify-res

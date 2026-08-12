#!/usr/bin/env bash
# Create a real Convergence transaction via wallet RPC createconvergence.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=common.sh
source "${SCRIPT_DIR}/common.sh"
lms_require_binaries
AMOUNT="${1:-1.0}"
exec python3 "${SCRIPT_DIR}/run_simulation.py" --create-convergence "${AMOUNT}"

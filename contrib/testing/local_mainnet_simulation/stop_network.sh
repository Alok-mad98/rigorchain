#!/usr/bin/env bash
# Stop the local mainnet simulation mesh.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=common.sh
source "${SCRIPT_DIR}/common.sh"
exec python3 "${SCRIPT_DIR}/run_simulation.py" --stop-only

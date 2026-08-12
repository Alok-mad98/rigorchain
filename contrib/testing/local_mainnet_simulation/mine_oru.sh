#!/usr/bin/env bash
# Mine ORU to Node-0 product_wallet via consensus generatetoaddress (maturity applies).
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=common.sh
source "${SCRIPT_DIR}/common.sh"
lms_require_binaries
N="${1:-101}"
exec python3 "${SCRIPT_DIR}/run_simulation.py" --mine-oru "${N}"

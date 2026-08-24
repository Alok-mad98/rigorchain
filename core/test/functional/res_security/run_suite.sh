#!/usr/bin/env bash
# Copyright (c) 2026 The RigorChain developers
# Run the pre-mainnet RES window / reorg / monopoly attack suite.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# res_security/ -> functional/ -> test/ -> core/
CORE_DIR="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
cd "${CORE_DIR}"
if [[ ! -f test/functional/feature_res_window_reorg_res_attacks.py ]]; then
  echo "error: expected to run from core/; got ${CORE_DIR}" >&2
  exit 1
fi
exec python3 test/functional/feature_res_window_reorg_res_attacks.py --descriptors "$@"

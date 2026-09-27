#!/usr/bin/env bash
# read-only RuntimeSupplyState export for Phase-5 / debug tooling.
# Usage:
#   runtimesupply-export.sh [blockhash] [rigor-cli args...]
# Example:
#   ./contrib/tools/runtimesupply-export.sh
#   ./contrib/tools/runtimesupply-export.sh <hash> -testnet -datadir=~/.rigor/testnet
#
# Equivalent RPC: rigor-cli getruntimesupply [blockhash]
#                 rigor-cli exportruntimesupply [blockhash]

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILT_CLI="${SCRIPT_DIR}/../../src/rigor-cli"

if [[ -x "${BUILT_CLI}" ]]; then
  RIGOR_CLI="${BUILT_CLI}"
elif command -v rigor-cli >/dev/null 2>&1; then
  RIGOR_CLI="$(command -v rigor-cli)"
else
  echo "runtimesupply-export.sh: rigor-cli not found." >&2
  echo "  Expected build artifact: ${BUILT_CLI}" >&2
  echo "  Run 'make -j\$(nproc)' in ${SCRIPT_DIR}/../.. or add rigor-cli to PATH." >&2
  exit 127
fi

BLOCKHASH="${1:-}"
shift || true

CLI=("${RIGOR_CLI}" "$@")
if [[ -n "${BLOCKHASH}" ]]; then
  exec "${CLI[@]}" exportruntimesupply "${BLOCKHASH}"
else
  exec "${CLI[@]}" exportruntimesupply
fi

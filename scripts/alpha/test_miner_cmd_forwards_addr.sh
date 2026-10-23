#!/bin/sh
# The Windows launcher must pass a payout address through to --coinbase-addr.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
CMD="$ROOT/installer/windows/rigor-miner.cmd"
grep -q -- '--coinbase-addr=' "$CMD"
grep -q -- '%~1' "$CMD"
grep -q 'COINBASE_ADDR=%~1' "$CMD"
grep -q -- '--no-getwork' "$CMD"
grep -q '127.0.0.1:8761' "$CMD"
if grep -q 'load-rpc-auth' "$CMD"; then
  echo "launcher still calls load-rpc-auth" >&2
  exit 1
fi
if grep -q 'start-miner-dashboard' "$CMD"; then
  echo "launcher still starts the dashboard" >&2
  exit 1
fi
echo MINER_CMD_FORWARD_OK

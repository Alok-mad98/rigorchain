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
grep -q -- '-a rgrpow01' "$CMD"
grep -q -- '-q' "$CMD"
grep -q 'rg1qCHANGE_ME' "$CMD"
grep -q 'rg1qYOURADDRESS' "$CMD"
grep -q 'MINER_THREADS' "$CMD"
grep -q 'rpc-auth.conf' "$CMD"
if grep -q 'load-rpc-auth' "$CMD"; then
  echo "launcher still calls load-rpc-auth" >&2
  exit 1
fi
if grep -q 'start-miner-dashboard' "$CMD"; then
  echo "launcher still starts the dashboard" >&2
  exit 1
fi
if grep -q '18761' "$CMD"; then
  echo "launcher mentions testnet RPC 18761" >&2
  exit 1
fi
if grep -qE 'solomining=1|-solomining=1' "$CMD"; then
  echo "launcher forces solomining" >&2
  exit 1
fi
# README: add `set MINER_THREADS=2` above the start command. The start
# command itself must read MINER_THREADS, not an earlier cached THREAD_ARG.
if grep -n 'rigor-miner.exe -a rgrpow01' "$CMD" | grep -q 'THREAD_ARG'; then
  echo "start command still uses precomputed THREAD_ARG; README override would miss" >&2
  exit 1
fi
SH="$ROOT/installer/linux/rigor-miner.sh"
grep -q -- '--coinbase-addr=' "$SH"
grep -q -- '-a rgrpow01' "$SH"
grep -q -- '--no-getwork' "$SH"
grep -q '127.0.0.1:8761' "$SH"
grep -q 'rg1qCHANGE_ME' "$SH"
grep -q 'rg1qYOURADDRESS' "$SH"
grep -q 'MINER_THREADS' "$SH"
if grep -q 'stratum+tcp' "$SH"; then
  echo "linux launcher mentions stratum" >&2
  exit 1
fi
if grep -q '18761' "$SH"; then
  echo "linux launcher mentions testnet RPC 18761" >&2
  exit 1
fi
echo MINER_CMD_FORWARD_OK

#!/usr/bin/env bash
# Start node-002 (miner). Joins via seednode/addnode to 127.0.0.1:18760.
set -euo pipefail
# shellcheck source=common.sh
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"
start_node node-002
if ! rpc node-002 -rpcwallet=miner getwalletinfo >/dev/null 2>&1; then
  if ! rpc node-002 loadwallet miner >/dev/null 2>&1; then
    rpc node-002 createwallet miner >/dev/null
  fi
fi
echo "MINER_RPC=127.0.0.1:18771"
echo "MINER_WALLET=miner"
echo "HINT=rigor-cli -testnet -datadir=$(node_datadir node-002) -conf=$(node_conf node-002) -rpcwallet=miner generatetoaddress 1 \$(… getnewaddress)"
rpc node-002 getpeerinfo | python3 -c "import json,sys; print('PEER_COUNT=%d' % len(json.load(sys.stdin)))"

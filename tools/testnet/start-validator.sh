#!/usr/bin/env bash
# Start node-003 (validator). Joins via seednode/addnode to 127.0.0.1:18760.
set -euo pipefail
# shellcheck source=common.sh
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"
start_node node-003
echo "VALIDATOR_RPC=127.0.0.1:18781"
rpc node-003 getblockcount
rpc node-003 getpeerinfo | python3 -c "import json,sys; print('PEER_COUNT=%d' % len(json.load(sys.stdin)))"

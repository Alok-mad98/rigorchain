#!/usr/bin/env bash
# Start node-001 (bootstrap). Does not mine.
# From a built tree: ./tools/testnet/start-bootstrap.sh
set -euo pipefail
# shellcheck source=common.sh
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"
start_node node-001
echo "BOOTSTRAP_P2P=127.0.0.1:18760"
echo "BOOTSTRAP_RPC=127.0.0.1:18761"
rpc node-001 getblockcount
rpc node-001 getpeerinfo | python3 -c "import json,sys; print('PEER_COUNT=%d' % len(json.load(sys.stdin)))"

#!/usr/bin/env python3
"""Three-node private testnet: start, seed join, mine, sync, restart."""
from __future__ import annotations

import json
import sys
import time

from rpc_util import (
    reset_chain,
    rpc,
    start_stack,
    stop_stack,
    wait_peers,
    wait_tips_equal,
    sh,
    TOOLS,
)


def mine_one() -> str:
    try:
        rpc("node-002", "loadwallet", "miner")
    except RuntimeError as exc:
        text = str(exc).lower()
        if "already loaded" in text or "duplicate" in text:
            pass
        elif "not found" in text:
            rpc("node-002", "createwallet", "miner")
        else:
            # wallet may already be loaded by start-miner.sh
            pass
    addr = rpc("node-002", "getnewaddress", wallet="miner")
    hashes = rpc("node-002", "generatetoaddress", 1, addr, 1000000, wallet="miner")
    if not hashes:
        raise RuntimeError("generatetoaddress returned no block")
    return hashes[0]


def main() -> int:
    reset_chain()
    try:
        start_stack()
        p1 = wait_peers("node-001", 1)
        p2 = wait_peers("node-002", 1)
        p3 = wait_peers("node-003", 1)
        print(f"PEERS node-001={p1} node-002={p2} node-003={p3}")

        block_hash = mine_one()
        print(f"MINED={block_hash}")
        tips = wait_tips_equal()
        if tips["node-003"]["hash"] != block_hash:
            raise RuntimeError(f"validator tip {tips['node-003']['hash']} != mined {block_hash}")
        if tips["node-001"]["height"] < 1:
            raise RuntimeError("bootstrap height still 0")
        print("SYNC=" + json.dumps(tips))

        sh("bash", str(TOOLS / "stop-all.sh"))
        time.sleep(2)
        start_stack()
        recovered = wait_tips_equal()
        if recovered["node-003"]["hash"] != block_hash:
            raise RuntimeError(f"restart lost tip: {recovered}")
        print("RESTART=" + json.dumps(recovered))
        print("MULTI_NODE_SYNC=PASS")
        return 0
    finally:
        stop_stack()


if __name__ == "__main__":
    sys.exit(main())

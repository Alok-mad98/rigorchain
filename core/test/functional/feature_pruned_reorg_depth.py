#!/usr/bin/env python3
# Copyright (c) 2026 The RIGOR developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""A pruned node must survive a reorg of MAX_SUPPORTED_REORG_DEPTH.

Connecting a block on RIGOR costs more than that block's own data: the convergence draw
enumerates F over CONVERGENCE_DRAW_WINDOW blocks, so ConnectBlock reads the block and undo data
of the window ending at the parent. A reorg of depth D therefore touches data down to
tip - D - W, and MIN_BLOCKS_TO_KEEP has to cover the sum.

Two things are pinned here:

  liveness  the reorg completes from local data plus a redownload of the replacement branch.
  verdicts  an ancestor the node cannot read is a local storage fact, not a fault of the block
            being connected. ConnectBlock must never turn it into BLOCK_CONSENSUS: that sets
            BLOCK_FAILED_VALID for the life of the datadir and scores the serving peer 100.

-fastprune gives one block per blk file, so pruning is exact to the height and the boundary is
actually exercised instead of being hidden by 32 MiB file granularity.
"""

from test_framework.blocktools import (
    CONVERGENCE_DRAW_WINDOW,
    DEFAULT_COINBASE_SCRIPT_PUBKEY,
    MAX_SUPPORTED_REORG_DEPTH,
    MIN_BLOCKS_TO_KEEP,
    add_witness_commitment,
    create_block,
    create_coinbase,
)
from test_framework.messages import CTxOut
from test_framework.script import (
    CScript,
    RIGOR_MARKER_RETURN,
)
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    assert_raises_rpc_error,
)

# -fastprune caps a blk file at 64 KiB, so any block above that lands in a file of its own.
FASTPRUNE_MAX_BLOCKFILE_SIZE = 0x10000
BLOCK_PAD_BYTES = FASTPRUNE_MAX_BLOCKFILE_SIZE + 4096

# Enough history that pruning to tip-MIN_BLOCKS_TO_KEEP leaves something pruned below the depth
# the reorg needs, with room to spare on both sides.
CHAIN_HEIGHT = MIN_BLOCKS_TO_KEEP + MAX_SUPPORTED_REORG_DEPTH + 64
# Longer than the branch it replaces, so the pruned node has to follow it.
REPLACEMENT_BRANCH = MAX_SUPPORTED_REORG_DEPTH + 8

PAD_SCRIPT = CScript([RIGOR_MARKER_RETURN, bytes(BLOCK_PAD_BYTES)])


def mine_padded_blocks(node, n):
    """Mine n blocks that each exceed the -fastprune block file size."""
    best = node.getblock(node.getbestblockhash())
    height = int(best["height"]) + 1
    ntime = int(best["time"])
    prev = int(best["hash"], 16)

    for _ in range(n):
        ntime += 1
        coinbase = create_coinbase(height, script_pubkey=DEFAULT_COINBASE_SCRIPT_PUBKEY)
        coinbase.vout.append(CTxOut(0, PAD_SCRIPT))
        coinbase.rehash()
        block = create_block(hashprev=prev, ntime=ntime, coinbase=coinbase)
        add_witness_commitment(block)
        block.solve()
        node.submitblock(block.serialize().hex())
        prev = block.sha256
        height += 1


class PrunedReorgDepthTest(BitcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 2
        self.extra_args = [
            [],
            ["-fastprune", "-prune=1"],
        ]

    def assert_pruned(self, node, height):
        assert_raises_rpc_error(-1, "Block not available (pruned data)",
                                node.getblock, node.getblockhash(height))

    def run_test(self):
        full, pruned = self.nodes

        self.log.info(f"Build a chain of {CHAIN_HEIGHT} blocks, one blk file per block on the pruned node")
        mine_padded_blocks(full, CHAIN_HEIGHT)
        self.sync_blocks(timeout=300)
        assert_equal(pruned.getblockcount(), CHAIN_HEIGHT)

        # The RPC clamps the request to tip - MIN_BLOCKS_TO_KEEP, which is the policy boundary
        # this test is about; asking for the tip is the way to land exactly on it.
        tip_height = pruned.getblockcount()
        keep_from = tip_height - MIN_BLOCKS_TO_KEEP
        self.log.info(f"Prune the pruned node as deep as policy allows (through height {keep_from})")
        pruned.pruneblockchain(tip_height)

        self.assert_pruned(pruned, keep_from)
        oldest_needed = tip_height - MAX_SUPPORTED_REORG_DEPTH - CONVERGENCE_DRAW_WINDOW + 1
        pruned.getblock(pruned.getblockhash(oldest_needed))
        self.log.info(f"Oldest height a depth-{MAX_SUPPORTED_REORG_DEPTH} reorg reads is {oldest_needed}; still on disk")

        self.log.info(f"Replace the last {MAX_SUPPORTED_REORG_DEPTH} blocks with a longer branch")
        self.disconnect_nodes(0, 1)
        fork_height = tip_height - MAX_SUPPORTED_REORG_DEPTH
        full.invalidateblock(full.getblockhash(fork_height + 1))
        assert_equal(full.getblockcount(), fork_height)
        mine_padded_blocks(full, REPLACEMENT_BRANCH)
        goal_hash = full.getbestblockhash()
        goal_height = fork_height + REPLACEMENT_BRANCH

        # A missing ancestor used to surface as a consensus verdict here. Either message means the
        # node recorded local storage state as a fact about the chain or about its peer.
        with pruned.assert_debug_log(
            expected_msgs=[],
            unexpected_msgs=["failed-parent-convergence-set", "Misbehaving"],
            timeout=10,
        ):
            self.connect_nodes(0, 1)
            self.sync_blocks(timeout=300)

        assert_equal(pruned.getblockcount(), goal_height)
        assert_equal(pruned.getbestblockhash(), goal_hash)

        self.log.info("No block was marked invalid and the redownloaded branch is readable")
        for tip in pruned.getchaintips():
            assert tip["status"] in ("active", "valid-fork", "valid-headers", "headers-only"), tip
        pruned.getblock(goal_hash)
        assert_equal(pruned.getblock(goal_hash)["confirmations"], 1)


if __name__ == '__main__':
    PrunedReorgDepthTest().main()

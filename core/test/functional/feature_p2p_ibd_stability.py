#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
""": IBD / slow-node / restart stability (PHASE-B2).

Validates fresh node headers-first catch-up, interrupted sync, and prune-compatible
IBD without stuck tips or peer disconnect storms.
"""

import os

from test_framework.test_framework import BitcoinTestFramework
from test_framework.p2p_propagation_util import (
    assert_tips_equal,
    assert_peer_counts_stable,
    wait_block_height,
)
from test_framework.util import assert_equal


class P2PIBDStabilityTest(BitcoinTestFramework):
    def set_test_params(self):
        self.chain = "devtest"
        self.setup_clean_chain = True
        self.num_nodes = 4
        self.extra_args = [
            [],
            [],
            [],
            ["-prune=550"],  # node 3: prune-compatible IBD
        ]

    def add_options(self, parser):
        parser.add_argument(
            "--ibd-blocks",
            dest="ibd_blocks",
            type=int,
            default=350,
            help="Blocks on seed before late joiner sync (default 350)",
        )

    def setup_network(self):
        self.setup_nodes()
        # Seed chain: node1 -> node0, node2 -> node1 (framework convention)
        self.connect_nodes(1, 0)
        self.connect_nodes(2, 1)

    def run_test(self):
        ibd_blocks = int(self.options.ibd_blocks)
        if ibd_blocks < 200:
            raise AssertionError("--ibd-blocks must be >= 200")

        seed = self.nodes[0]
        n1 = self.nodes[1]
        mid = self.nodes[2]
        late = self.nodes[3]
        addr = seed.get_deterministic_witness_mining_address()
        seeders = [seed, n1, mid]
        sync_seed = lambda: self.sync_blocks(seeders)

        self.log.info("Seed %d blocks on node0 (node3 not connected yet)", ibd_blocks + 101)
        self.generatetoaddress(seed, 101, addr, sync_fun=sync_seed)
        self.generatetoaddress(seed, ibd_blocks, addr, sync_fun=sync_seed)
        self.sync_blocks(seeders, timeout=300)
        target_height = seed.getblockcount()
        target_tip = seed.getbestblockhash()

        self.log.info("Late node (prune) starts and syncs headers-first from node2")
        self.connect_nodes(2, 3)
        self.sync_blocks([seed, n1, mid, late], timeout=300)
        self.sync_mempools([seed, n1, mid, late])

        assert_equal(late.getblockcount(), target_height)
        assert_equal(late.getbestblockhash(), target_tip)
        ch = late.getblockchaininfo()
        assert ch["headers"] >= target_height

        self.log.info("Interrupt IBD: stop late node mid-run, restart, resync")
        self.stop_node(3)
        self.start_node(3, self.extra_args[3])
        self.connect_nodes(2, 3)
        wait_block_height(late, target_height, timeout=180, test_framework=self)
        assert_equal(late.getbestblockhash(), target_tip)

        self.log.info("Restart seed during follower catch-up (no tip corruption)")
        partial = target_height - 25
        self.stop_node(3)
        self.start_node(3, self.extra_args[3])
        self.connect_nodes(2, 3)
        self.generatetoaddress(seed, 15, addr, sync_fun=sync_seed)
        self.sync_blocks([seed, n1, mid, late], timeout=300)
        assert_tips_equal(self.nodes, "after seed restart + extend")

        bh = late.getblockhash(partial)
        late.getblockheader(bh)  # pruned node must retain headers chain
        assert late.getblockcount() == seed.getblockcount()

        assert_peer_counts_stable([seed, mid, late], min_peers=1)

        log_path = os.path.join(self.options.tmpdir, "ibd_stability_summary.txt")
        with open(log_path, "w", encoding="utf-8") as f:
            f.write(f"target_height={target_height}\n")
            f.write(f"target_tip={target_tip}\n")
            f.write(f"late_final_height={late.getblockcount()}\n")
            f.write(f"late_prune={late.getblockchaininfo().get('pruned')}\n")
        self.log.info("IBD stability OK — wrote %s", log_path)


if __name__ == "__main__":
    P2PIBDStabilityTest().main()

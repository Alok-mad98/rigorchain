#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""TASK-B5-001: long-chain block propagation across 3–5 nodes (PHASE-B2).

Topology (5 nodes): 0 — 1 — 2 — 3 — 4
Node 0 mines sequentially; all nodes must converge on tip without disconnect storms
or runaway mempools. Samples propagation latency on the far end (node 4).

Default 1000 blocks. For faster CI: ``RIGOR_P2P_PROP_BLOCKS=200`` or
``python3 feature_p2p_1000_block_propagation.py --prop-blocks=200``
(do not pass ``--prop-blocks`` to ``test_runner.py`` — it forwards to create_cache.py).
"""

from __future__ import annotations

import csv
import os

# CI / test_runner cannot pass script-only flags to create_cache.py; use env instead.
_ENV_PROP_BLOCKS = os.environ.get("RIGOR_P2P_PROP_BLOCKS")
import statistics
import time

from test_framework.test_framework import BitcoinTestFramework
from test_framework.p2p_propagation_util import (
    assert_peer_counts_stable,
    assert_tips_equal,
    measure_propagation_latency,
    sample_network_metrics,
)


class P2P1000BlockPropagationTest(BitcoinTestFramework):
    def set_test_params(self):
        self.chain = "devtest"
        self.setup_clean_chain = True
        self.num_nodes = 5
        self.extra_args = [[], [], [], [], []]

    def add_options(self, parser):
        _default_blocks = (
            int(_ENV_PROP_BLOCKS) if _ENV_PROP_BLOCKS is not None else 1000
        )
        parser.add_argument(
            "--prop-blocks",
            dest="prop_blocks",
            type=int,
            default=_default_blocks,
            help="Blocks to mine after bootstrap (default 1000, or RIGOR_P2P_PROP_BLOCKS)",
        )
        parser.add_argument(
            "--prop-batch",
            dest="prop_batch",
            type=int,
            default=10,
            help="Mine batch size (default 10)",
        )
        parser.add_argument(
            "--prop-sample-every",
            dest="prop_sample_every",
            type=int,
            default=100,
            help="Record latency sample every N blocks (default 100)",
        )

    def setup_network(self):
        self.setup_nodes()
        # Linear chain: 0-1-2-3-4
        for i in range(self.num_nodes - 1):
            self.connect_nodes(i, i + 1)

    def run_test(self):
        n_blocks = int(self.options.prop_blocks)
        batch = max(1, int(self.options.prop_batch))
        sample_every = max(1, int(self.options.prop_sample_every))
        if n_blocks < 50:
            raise AssertionError("--prop-blocks must be >= 50")

        miner = self.nodes[0]
        tail = self.nodes[4]
        addr = miner.get_deterministic_witness_mining_address()

        self.log.info("Bootstrap: 101 blocks on miner, sync chain")
        self.generatetoaddress(miner, 101, addr)
        self.sync_all()
        assert_tips_equal(self.nodes, "post-bootstrap")

        metrics_path = os.path.join(self.options.tmpdir, "p2p_1000_propagation_metrics.tsv")
        latency_rows = []
        peer_rows = []

        self.log.info(
            "Mining %d blocks in batches of %d (5-node line topology)",
            n_blocks,
            batch,
        )
        mined = 0
        t_start = time.time()
        while mined < n_blocks:
            chunk = min(batch, n_blocks - mined)
            hashes = self.generatetoaddress(miner, chunk, addr, sync_fun=lambda: None)
            last_hash = hashes[-1]

            if (mined + chunk) % sample_every == 0 or mined + chunk >= n_blocks:
                lats = measure_propagation_latency(miner, [tail], last_hash, timeout=90)
                latency_rows.append((mined + chunk, last_hash, lats[0]))
                for i, n in enumerate(self.nodes):
                    peer_rows.append((mined + chunk, i, len(n.getpeerinfo())))

            self.sync_all()
            assert_tips_equal(self.nodes, f"height {mined + chunk}")
            assert_peer_counts_stable(self.nodes, min_peers=1)

            mi_tail = tail.getmempoolinfo()
            assert mi_tail["size"] < 5000, "unexpected mempool growth during block-only propagation"

            mined += chunk
            if mined % 200 == 0 or mined >= n_blocks:
                self.log.info("Progress: %d / %d blocks", mined, n_blocks)

        elapsed = time.time() - t_start
        heights = [n.getblockcount() for n in self.nodes]
        assert len(set(heights)) == 1

        with open(metrics_path, "w", encoding="utf-8", newline="") as f:
            w = csv.writer(f, delimiter="\t")
            w.writerow(["section", "field", "value"])
            w.writerow(["summary", "blocks_mined", mined])
            w.writerow(["summary", "elapsed_sec", f"{elapsed:.3f}"])
            w.writerow(["summary", "final_height", heights[0]])
            if latency_rows:
                samples = [r[2] for r in latency_rows]
                w.writerow(["summary", "latency_samples", len(samples)])
                w.writerow(["summary", "latency_mean_sec", f"{statistics.mean(samples):.4f}"])
                w.writerow(["summary", "latency_max_sec", f"{max(samples):.4f}"])
            w.writerow([])
            w.writerow(["height", "blockhash", "tail_latency_sec"])
            for row in latency_rows:
                w.writerow(row)
            w.writerow([])
            w.writerow(["height", "node", "peer_count"])
            for row in peer_rows:
                w.writerow(row)
            w.writerow([])
            w.writerow(["node", "metric", "value"])
            for i, n in enumerate(self.nodes):
                for k, v in sample_network_metrics(n).items():
                    w.writerow([i, k, v])

        self.log.info("Wrote metrics: %s", metrics_path)
        self.log.info(
            "1000-block propagation OK: height=%d elapsed=%.1fs",
            heights[0],
            elapsed,
        )


if __name__ == "__main__":
    P2P1000BlockPropagationTest().main()

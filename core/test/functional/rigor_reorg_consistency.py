#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Multi-node fork + reorg: identical Rigor consensus-derived fields on active best chain.

After the longer branch wins, every block hash on the common prefix that remains valid on
both nodes must report the same winner, target, RES minted bit, and candidate digest inputs.
"""

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal


def _block_draw_commitment_subset(block_obj):
    # Wire-stable getblock JSON keys (RIGOR-branded; match src/rpc/blockchain.cpp).
    return {
        "rigor_winner": block_obj["rigor_winner"],
        "res_probability_target": block_obj["res_probability_target"],
        "rigor_res_minted": block_obj["rigor_res_minted"],
    }


def _candidate_digest_map(convergenceresult_obj):
    out = {}
    for c in convergenceresult_obj["candidates"]:
        out[c["txid"]] = {
            "h": c["h"],
            "draw_eligible": c["draw_eligible"],
            "convergence_amount": c["convergence_amount"],
        }
    return out


class RigorReorgConsistencyTest(BitcoinTestFramework):
    def set_test_params(self):
        self.chain = "devtest"
        self.setup_clean_chain = True
        self.num_nodes = 2
        self.extra_args = [[], []]

    def run_test(self):
        n0, n1 = self.nodes[0], self.nodes[1]
        addr0 = n0.get_deterministic_witness_mining_address()
        addr1 = n1.get_deterministic_witness_mining_address()

        self.log.info("Common chain on node0, sync to node1")
        self.generatetoaddress(n0, 120, addr0)
        self.sync_all()

        self.log.info("Disconnect and build competing tips")
        self.disconnect_nodes(0, 1)
        self.wait_until(lambda: len(n0.getpeerinfo()) == 0 and len(n1.getpeerinfo()) == 0)

        # While partitioned, do not call sync_all/sync_blocks.
        self.generatetoaddress(n0, 4, addr0, sync_fun=lambda: None)
        self.generatetoaddress(n1, 8, addr1, sync_fun=lambda: None)

        self.log.info("Reconnect; longer fork (node1) should become best for both")
        self.connect_nodes(0, 1)
        self.sync_all()

        assert_equal(n0.getblockcount(), n1.getblockcount())
        tip = n0.getbestblockhash()
        assert_equal(tip, n1.getbestblockhash())

        self.log.info("Compare Rigor draw-commitment fields and convergenceresult digests on last 25 blocks (both nodes)")
        height = n0.getblockcount()
        for h in range(max(0, height - 24), height + 1):
            bh = n0.getblockhash(h)
            assert_equal(bh, n1.getblockhash(h))
            b0 = n0.getblock(bh, 1)
            b1 = n1.getblock(bh, 1)
            assert_equal(_block_draw_commitment_subset(b0), _block_draw_commitment_subset(b1))

            fr0 = n0.getconvergenceresult(bh)
            fr1 = n1.getconvergenceresult(bh)
            assert_equal(fr0["draw_winner_txid"], fr1["draw_winner_txid"])
            # SECURITY-001-A8: the σ seed is the block's own hash, so agreeing on the block is
            # already agreeing on the seed — assert the node reports it that way.
            assert_equal(fr0["sigma_seed"], bh)
            assert_equal(fr1["sigma_seed"], bh)
            assert_equal(fr0["payout_height"], fr1["payout_height"])
            t0 = fr0.get("res_probability_target", fr0.get("orbis_draw_target"))
            t1 = fr1.get("res_probability_target", fr1.get("orbis_draw_target"))
            assert_equal(t0, t1)
            assert_equal(_candidate_digest_map(fr0), _candidate_digest_map(fr1))

        self.log.info("Reorg Rigor draw-commitment field consistency passed")


if __name__ == "__main__":
    RigorReorgConsistencyTest().main()

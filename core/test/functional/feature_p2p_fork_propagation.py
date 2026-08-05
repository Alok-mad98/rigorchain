#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""TASK-B5-001: fork races and reorg propagation (PHASE-B2).

Scenarios on devtest (2–3 nodes):
  * 1-block race
  * 3-block race
  * delayed block arrival (slow peer)
  * slow peer catch-up after partition
  * mempool survival across invalidate/reorg (ORU fee semantics)
"""

from decimal import Decimal

from test_framework.authproxy import JSONRPCException
from test_framework.blocktools import COINBASE_MATURITY
from test_framework.test_framework import BitcoinTestFramework
from test_framework.p2p_propagation_util import assert_tips_equal, assert_peer_counts_stable
from test_framework.util import assert_equal

from feature_dual_pool_mempool import build_dual_pool_hex


class P2PForkPropagationTest(BitcoinTestFramework):
    def set_test_params(self):
        self.chain = "devtest"
        self.setup_clean_chain = True
        self.num_nodes = 3
        self.extra_args = [[], [], []]

    def mine_bootstrap(self, n, addr, count=120):
        self.generatetoaddress(n, count, addr)

    def scenario_1_block_race(self):
        self.log.info("=== 1-block race ===")
        n0, n1, n2 = self.nodes
        a0 = n0.get_deterministic_witness_mining_address()
        a1 = n1.get_deterministic_witness_mining_address()
        self.mine_bootstrap(n0, a0)
        self.sync_all()

        self.disconnect_nodes(0, 1)
        self.disconnect_nodes(1, 2)
        self.wait_until(lambda: len(n1.getpeerinfo()) == 0)

        self.generatetoaddress(n0, 1, a0, sync_fun=lambda: None)
        self.generatetoaddress(n1, 2, a1, sync_fun=lambda: None)

        self.connect_nodes(0, 1)
        self.connect_nodes(1, 2)
        self.sync_all()
        assert_tips_equal([n0, n1, n2], "1-block race")
        assert n1.getblockcount() == n0.getblockcount()

    def scenario_3_block_race(self):
        self.log.info("=== 3-block race ===")
        n0, n1, n2 = self.nodes
        a0 = n0.get_deterministic_witness_mining_address()
        a1 = n1.get_deterministic_witness_mining_address()
        self.mine_bootstrap(n0, a0)
        self.sync_all()

        self.disconnect_nodes(0, 1)
        self.disconnect_nodes(1, 2)
        self.wait_until(lambda: len(n1.getpeerinfo()) == 0)

        self.generatetoaddress(n0, 3, a0, sync_fun=lambda: None)
        self.generatetoaddress(n1, 6, a1, sync_fun=lambda: None)

        self.connect_nodes(0, 1)
        self.connect_nodes(1, 2)
        self.sync_all()
        assert_tips_equal([n0, n1, n2], "3-block race")
        assert_equal(n0.getblockcount(), n1.getblockcount())

    def scenario_delayed_arrival(self):
        self.log.info("=== delayed block arrival (slow tail) ===")
        n0, n1, n2 = self.nodes
        a0 = n0.get_deterministic_witness_mining_address()
        self.mine_bootstrap(n0, a0)
        self.sync_all()

        self.disconnect_nodes(1, 2)
        self.wait_until(lambda: len(n2.getpeerinfo()) <= 1)

        hashes = self.generatetoaddress(n0, 5, a0, sync_fun=lambda: None)
        self.sync_blocks([n0, n1])
        h_before = n2.getblockcount()

        self.connect_nodes(1, 2)
        self.sync_all()
        assert_tips_equal([n0, n1, n2], "delayed arrival")
        assert n2.getblockcount() > h_before
        assert_equal(n2.getbestblockhash(), hashes[-1])

    def scenario_slow_peer_catchup(self):
        self.log.info("=== slow peer catch-up ===")
        n0, n1, n2 = self.nodes
        a0 = n0.get_deterministic_witness_mining_address()
        self.mine_bootstrap(n0, a0)
        self.sync_all()

        self.disconnect_nodes(1, 2)
        self.wait_until(lambda: len(n2.getpeerinfo()) == 0)
        self.generatetoaddress(n0, 80, a0, sync_fun=lambda: None)
        self.sync_blocks([n0, n1])
        assert n2.getblockcount() < n0.getblockcount() - 50

        self.connect_nodes(1, 2)
        self.sync_all()
        assert_tips_equal([n0, n1, n2], "slow catch-up")

    def scenario_mempool_reorg_recovery(self):
        self.log.info("=== dual-pool mempool across invalidate/reorg ===")
        node = self.nodes[0]
        try:
            node.getwalletinfo()
        except JSONRPCException:
            self.log.info("no wallet on node0 — skip (see feature_dual_pool_reorg.py)")
            return
        self.oru_fund_addr = node.getnewaddress("", "bech32")
        self.generatetoaddress(node, COINBASE_MATURITY + 10, self.oru_fund_addr)
        br = node.getnewaddress("", "bech32m")
        self.stop_node(0)
        self.start_node(
            0,
            list(self.extra_args[0])
            + [
                "-reswalletbootstrap",
                f"-miningresfeeaddr={br}",
                "-minrelaytxfee=0",
                "-blockmintxfee=0",
                "-reindex-chainstate",
            ],
        )
        self.generatetoaddress(node, 120, self.oru_fund_addr)
        hex_tx = build_dual_pool_hex(
            node,
            bootstrap_res_addr=br,
            oru_fund_addr=self.oru_fund_addr,
            res_spend=Decimal("1.00"),
            oru_fee=Decimal("0.00005"),
        )
        txid = node.sendrawtransaction(hex_tx)
        blocks = self.generatetoaddress(node, 2, self.oru_fund_addr)
        node.invalidateblock(blocks[0])
        assert_equal(set(node.getrawmempool()), {txid})
        ent = node.getmempoolentry(txid)
        assert_equal(Decimal(str(ent["fees"]["base"])), Decimal("0.00005"))

    def run_test(self):
        self.scenario_1_block_race()
        self.scenario_3_block_race()
        self.scenario_delayed_arrival()
        self.scenario_slow_peer_catchup()
        self.scenario_mempool_reorg_recovery()
        assert_peer_counts_stable(self.nodes, min_peers=0, label="post-scenarios ")
        self.log.info("Fork propagation scenarios passed")


if __name__ == "__main__":
    P2PForkPropagationTest().main()

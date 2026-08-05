#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""PHASE-P8: dual-pool tx survives invalidateblock resurrection with ORU fee semantics."""

from decimal import Decimal

from test_framework.blocktools import COINBASE_MATURITY
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal

from feature_dual_pool_mempool import (
    build_dual_pool_hex,
    find_bootstrap_res_utxo,
)


class DualPoolReorgTest(BitcoinTestFramework):
    def add_options(self, parser):
        self.add_wallet_options(parser)

    def set_test_params(self):
        self.chain = "devtest"
        self.setup_clean_chain = True
        self.num_nodes = 1
        self.extra_args = [[]]
        self.wallet_names = ["default_wallet"]
        self._requires_wallet = True

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def bootstrap_res_wallet(self):
        node = self.nodes[0]
        self.oru_fund_addr = node.getnewaddress("", "bech32")
        self.generatetoaddress(node, COINBASE_MATURITY + 10, self.oru_fund_addr)

        orbis_fee = node.getnewaddress("", "bech32m")
        self.bootstrap_res_fee_addr = orbis_fee
        self.stop_node(0)
        self.start_node(
            0,
            list(self.extra_args[0])
            + [
                "-reswalletbootstrap",
                f"-miningresfeeaddr={orbis_fee}",
                "-minrelaytxfee=0",
                "-blockmintxfee=0",
                "-reindex-chainstate",
            ],
        )
        self.generatetoaddress(node, 120, self.oru_fund_addr)

    def run_test(self):
        self.bootstrap_res_wallet()
        node = self.nodes[0]
        fund = self.oru_fund_addr
        br = self.bootstrap_res_fee_addr

        hex_tx = build_dual_pool_hex(
            node,
            bootstrap_res_addr=br,
            oru_fund_addr=fund,
            res_spend=Decimal("1.00"),
            oru_fee=Decimal("0.00005"),
        )
        txid = node.sendrawtransaction(hex_tx)
        assert_equal(node.getrawmempool(), [txid])

        blocks = self.generatetoaddress(node, 2, fund)
        assert_equal(node.getrawmempool(), [])
        tip_before = node.getbestblockhash()

        self.log.info("invalidate shorter branch: dual-pool tx returns to mempool")
        node.invalidateblock(blocks[0])
        assert_equal(set(node.getrawmempool()), {txid})

        ent = node.getmempoolentry(txid)
        assert_equal(Decimal(str(ent["fees"]["base"])), Decimal("0.00005"))
        res_u = find_bootstrap_res_utxo(node, br, Decimal("1"))
        assert_equal(res_u["address"], br)

        self.log.info("re-mine on competing tip: tx confirms again")
        # Invalidateblock marks prior headers BLOCK_FAILED_VALID. Rebuilt templates that replay the
        # same coinbase payout script + height can collide with cached-invalid headers —
        # see feature_rigor_reorg_winner_lock (use a fresh generatetoaddress payout after reorg).
        fund_rebuild = node.getnewaddress("", "bech32")
        for _ in range(12):
            if not node.getrawmempool():
                break
            self.generatetoaddress(node, 1, fund_rebuild)
            fund_rebuild = node.getnewaddress("", "bech32")
        assert_equal(node.getrawmempool(), [])
        assert tip_before != node.getbestblockhash()


if __name__ == "__main__":
    DualPoolReorgTest().main()

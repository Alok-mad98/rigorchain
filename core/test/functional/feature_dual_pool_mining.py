#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""PHASE-P8: block assembly orders independent dual-pool txs by ORU feerate (not RES moved)."""

from decimal import Decimal

from test_framework.blocktools import COINBASE_MATURITY
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal

from feature_dual_pool_mempool import build_dual_pool_hex


class DualPoolMiningTest(BitcoinTestFramework):
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

        self.log.info("submit low-ORU-fee dual tx then high-ORU-fee dual tx")
        used = []
        hex_lo = build_dual_pool_hex(
            node,
            bootstrap_res_addr=br,
            oru_fund_addr=fund,
            res_spend=Decimal("0.50"),
            oru_fee=Decimal("0.000001"),
            excluded=used,
        )
        for vin in node.decoderawtransaction(hex_lo)["vin"]:
            used.append((vin["txid"], vin["vout"]))
        hex_hi = build_dual_pool_hex(
            node,
            bootstrap_res_addr=br,
            oru_fund_addr=fund,
            res_spend=Decimal("0.50"),
            oru_fee=Decimal("0.02"),
            excluded=used,
        )
        tx_lo = node.sendrawtransaction(hex_lo)
        tx_hi = node.sendrawtransaction(hex_hi)
        assert_equal(set(node.getrawmempool()), {tx_lo, tx_hi})

        block_hashes = self.generatetoaddress(node, 1, fund)
        block = node.getblock(block_hashes[0], 1)
        txids = block["tx"]
        cb = txids[0]
        assert tx_lo != cb and tx_hi != cb

        # Higher ORU fee must be prioritized earlier than the tiny-fee sibling (ancestor score order).
        i_hi = txids.index(tx_hi)
        i_lo = txids.index(tx_lo)
        assert i_hi < i_lo, (i_hi, i_lo, txids)


if __name__ == "__main__":
    DualPoolMiningTest().main()

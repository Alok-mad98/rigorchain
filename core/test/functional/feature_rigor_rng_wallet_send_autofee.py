#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""RES wallet: sendtoaddress(asset=RES) with wallet fee estimation (no manual fee_rate).

Exercises DummySignTx / coin selection for witness v1 RES: the transaction must
enter the mempool without an explicit fee_rate, then credit the recipient after a block.
"""

from decimal import Decimal

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, assert_greater_than_or_equal


class RigorRngWalletSendAutofeeTest(BitcoinTestFramework):
    def set_test_params(self):
        self.chain = "devtest"
        self.setup_clean_chain = True
        self.num_nodes = 1
        self.extra_args = [[]]
        self.wallet_names = ["default_wallet"]
        self._requires_wallet = True

    def add_options(self, parser):
        self.add_wallet_options(parser)

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def run_test(self):
        node = self.nodes[0]
        res_fee_addr = node.getnewaddress("", "bech32m")
        info = node.getaddressinfo(res_fee_addr)
        assert info.get("iswitness") and info.get("witness_version") == 1

        self.stop_node(0)
        extra = list(self.extra_args[0]) + [
            "-reswalletbootstrap",
            f"-miningresfeeaddr={res_fee_addr}",
            "-minrelaytxfee=0",
            "-blockmintxfee=0",
        ]
        self.start_node(0, extra)

        mining_addr = node.get_deterministic_witness_mining_address()
        self.generatetoaddress(node, 120, mining_addr)

        recv = node.getnewaddress("", "bech32m")
        self.log.info("sendtoaddress 0.01 RES -> %s (wallet-estimated fee, no fee_rate)", recv)
        txid = node.sendtoaddress(
            recv,
            "0.01",
            "",
            "",
            False,
            False,
            None,
            "unset",
            None,
            None,
            False,
            "RES",
        )
        assert len(txid) == 64
        assert txid in node.getrawmempool()

        self.generatetoaddress(node, 1, mining_addr)
        outs = node.listunspent(1, 9999999, [recv])
        assert len(outs) >= 1
        assert_equal(outs[0]["address"], recv)
        assert_greater_than_or_equal(Decimal(str(outs[0]["amount"])), Decimal("0.009"))


if __name__ == "__main__":
    RigorRngWalletSendAutofeeTest().main()

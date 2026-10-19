#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""A node with a loaded wallet must start with -reindex / -reindex-chainstate.

Regression for R2A-M01: the startup resubmit ran the height-invalid mint sweep
while the wallet had no processed tip yet and aborted in GetLastBlockHeight().
The abort depends on the validation-interface queue lagging behind the genesis
connect, so it only shows up reliably under CPU contention; the deterministic
regression is abandon_height_invalid_mints_without_tip_is_noop in
wallet_converge_classification_tests.
"""
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal


class WalletReindexStartupTest(BitcoinTestFramework):
    def add_options(self, parser):
        self.add_wallet_options(parser)

    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 1

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def run_test(self):
        node = self.nodes[0]
        addr = node.getnewaddress("", "bech32")
        self.generatetoaddress(node, 110, addr)
        node.sendtoaddress(node.getnewaddress("", "bech32"), 1)
        height = node.getblockcount()
        balance = node.getbalance()

        for args in (["-reindex"], ["-reindex-chainstate"], ["-reindex"], ["-reindex-chainstate"]):
            self.log.info(f"Restart with {args}")
            self.restart_node(0, extra_args=args)
            self.wait_until(lambda: node.getblockcount() == height)
            node.syncwithvalidationinterfacequeue()
            assert_equal(node.getbalance(), balance)

        self.log.info("Wallet still signs and spends after the reindex cycles")
        txid = node.sendtoaddress(node.getnewaddress("", "bech32"), 1)
        self.generatetoaddress(node, 1, addr)
        assert_equal(node.gettransaction(txid)["confirmations"], 1)


if __name__ == '__main__':
    WalletReindexStartupTest().main()

#!/usr/bin/env python3
# Copyright (c) 2026 The RIGOR developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Two-node determinism: the same Ed25519-signed ORU spend is accepted identically.

Node A creates and signs a wallet transaction. Node B validates the same bytes
into its mempool and both nodes mine/confirm it with equal txid and hex.
"""

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, assert_raises_rpc_error


class Ed25519CrossNodeTest(BitcoinTestFramework):
    def add_options(self, parser):
        self.add_wallet_options(parser, legacy=False)

    def set_test_params(self):
        self.num_nodes = 2
        self.setup_clean_chain = True

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def run_test(self):
        node_a = self.nodes[0]
        node_b = self.nodes[1]

        mine_addr = node_a.getnewaddress(address_type="bech32")
        self.generatetoaddress(node_a, 110, mine_addr)
        self.sync_all()

        dest = node_b.getnewaddress(address_type="bech32")
        txid = node_a.sendtoaddress(dest, 1)
        raw = node_a.gettransaction(txid)["hex"]
        self.sync_mempools()

        self.log.info("Both nodes must hold the same mempool txid after wallet Ed25519 spend")
        assert_equal(sorted(node_a.getrawmempool()), [txid])
        assert_equal(sorted(node_b.getrawmempool()), [txid])
        assert_equal(node_a.getrawtransaction(txid), node_b.getrawtransaction(txid))
        assert_equal(node_a.getrawtransaction(txid), raw)

        self.generatetoaddress(node_a, 1, mine_addr)
        self.sync_all()
        # Confirmed txs are not in the mempool; without -txindex, getrawtransaction
        # needs the including block hash (see rigor_block_builder.py).
        blockhash = node_a.getbestblockhash()
        hex_a = node_a.getrawtransaction(txid, False, blockhash)
        hex_b = node_b.getrawtransaction(txid, False, blockhash)
        assert_equal(hex_a, hex_b)
        assert_equal(hex_a, raw)
        assert_equal(node_a.gettransaction(txid)["confirmations"], 1)
        assert_equal(node_b.gettransaction(txid)["confirmations"], 1)

        self.log.info("createmultisig must reject secp256k1 compressed pubkeys")
        secp_hex = "03789ed0bb717d88f7d321a368d905e7430207ebbd82bd342cf11ae157a7ace5fd"
        secp_hex2 = "03dbc6764b8884a92e871274b87583e6d5c2a58819473e17e107ef3f6aa5a61626"
        assert_raises_rpc_error(-8, "Ed25519 ORU pubkeys", node_a.createmultisig, 2, [secp_hex, secp_hex2])


if __name__ == "__main__":
    Ed25519CrossNodeTest().main()

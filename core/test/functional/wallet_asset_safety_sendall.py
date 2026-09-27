#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
""": sendall is ORU-only; RES cannot be swept or mixed.

Notes (regtest):
- Coinbase maturity is 100. Mining 101 blocks to a wallet yields only ~1 mature UTXO;
  the next generate() can mature another 1000 ORU coinbase into trusted — do not assert
  trusted==0 after that generate.
- RIGOR FEE FINAL LOCK: fee_i >= 0; size = wire bytes (see docs/audit/RIGOR_FEE_FINAL_LOCK_ALIGNMENT.md).
"""

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    assert_greater_than,
    assert_raises_rpc_error,
)


class WalletAssetSafetySendallTest(BitcoinTestFramework):
    def add_options(self, parser):
        self.add_wallet_options(parser)

    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def run_test(self):
        n = self.nodes[0]
        # Miner wallet holds coinbases; source wallet only receives a single mature ORU payment
        # so sendall can empty it without later coinbase maturity re-filling trusted.
        miner = n.get_wallet_rpc(self.default_wallet_name)
        self.generatetoaddress(n, 101, miner.getnewaddress(address_type="bech32"))

        n.createwallet("source")
        n.createwallet("sink")
        source = n.get_wallet_rpc("source")
        sink = n.get_wallet_rpc("sink")

        fund_addr = source.getnewaddress(address_type="bech32")
        miner.sendtoaddress(fund_addr, 25)
        self.generatetoaddress(n, 1, miner.getnewaddress(address_type="bech32"))
        assert_greater_than(source.getbalances()["oru"]["trusted"], 0)

        # ConstructTransaction rejects RES destinations first (RPC -8).
        res_addr = source.getnewaddress(address_type="bech32m")
        assert_raises_rpc_error(
            -8,
            "Address type does not match",
            source.sendall,
            [res_addr],
        )

        sweep_to = sink.getnewaddress(address_type="bech32")
        receipt = source.sendall([sweep_to])
        assert_equal(receipt["complete"], True)
        # Before confirmation: mature ORU should already be spent (trusted == 0).
        assert_equal(source.getbalances()["oru"]["trusted"], 0)

        self.generatetoaddress(n, 1, miner.getnewaddress(address_type="bech32"))
        assert_equal(source.getbalances()["oru"]["trusted"], 0)
        assert_greater_than(sink.getbalances()["oru"]["trusted"], 0)

        res_bal = source.getbalances()["res"]
        assert "trusted" in res_bal
        assert "immature" in res_bal

        oru_addr = source.getnewaddress(address_type="bech32")
        assert_raises_rpc_error(-5, "", source.dumpresprivkey, oru_addr)
        assert_raises_rpc_error(-5, "", source.dumporuprivkey, res_addr)


if __name__ == "__main__":
    WalletAssetSafetySendallTest().main()

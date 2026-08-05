#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Lightweight ORU vs RES amount precision isolation (no full wallet_send).

- ORU: send 1.00000001 (8 decimals) via default sendtoaddress; verify recipient UTXO amount.
- RES: send 1.00 (2 decimals) with asset=RES on devtest bootstrap; verify recipient UTXO.
- RES: 1.001 must be rejected at RPC (ParseFixedPoint with 2 decimals).
"""

from decimal import Decimal

from test_framework.blocktools import COINBASE_MATURITY
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, assert_raises_rpc_error


def sendtoaddress_with_asset(node, addr, amount, asset):
    """sendtoaddress with explicit asset (positional index 11)."""
    return node.sendtoaddress(
        addr,
        amount,
        "",
        "",
        False,
        False,
        None,
        "unset",
        None,
        None,
        False,
        asset,
    )


class PrecisionIsolationTest(BitcoinTestFramework):
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
        mining_addr = node.get_deterministic_witness_mining_address()
        self.generatetoaddress(node, COINBASE_MATURITY + 5, mining_addr)

        self.log.info("ORU: send 1.00000001 to new bech32 address")
        oru_recv = node.getnewaddress("", "bech32")
        tx_m = node.sendtoaddress(oru_recv, "1.00000001")
        assert len(tx_m) == 64
        self.generatetoaddress(node, 1, mining_addr)
        oru_outs = node.listunspent(1, 9999999, [oru_recv])
        assert len(oru_outs) >= 1
        assert_equal(Decimal(str(oru_outs[0]["amount"])), Decimal("1.00000001"))

        self.log.info("RES: 1.001 must be rejected before bootstrap (RPC Invalid amount)")
        any_res_recv = node.getnewaddress("", "bech32m")
        assert_raises_rpc_error(
            -3,
            "Invalid amount",
            sendtoaddress_with_asset,
            node,
            any_res_recv,
            "1.001",
            "RES",
        )

        self.log.info("RES: restart with bootstrap, send 1.00 RES (bech32m)")
        res_fee = node.getnewaddress("", "bech32m")
        self.stop_node(0)
        # Clean re-read chain state after parameter change (avoids rare VerifyDB/undo issues on restart).
        self.start_node(
            0,
            list(self.extra_args[0])
            + [
                "-reswalletbootstrap",
                f"-miningresfeeaddr={res_fee}",
                "-minrelaytxfee=0",
                "-blockmintxfee=0",
                "-reindex-chainstate",
            ],
        )
        self.generatetoaddress(node, 120, mining_addr)

        res_recv = node.getnewaddress("", "bech32m")
        tx_r = sendtoaddress_with_asset(node, res_recv, "1.00", "RES")
        assert len(tx_r) == 64
        self.generatetoaddress(node, 1, mining_addr)
        res_outs = node.listunspent(1, 9999999, [res_recv])
        assert len(res_outs) >= 1
        assert_equal(Decimal(str(res_outs[0]["amount"])), Decimal("1.00"))


if __name__ == "__main__":
    PrecisionIsolationTest().main()

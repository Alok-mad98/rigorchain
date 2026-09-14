#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""RFC-020 wallet mint RPC: createconvergence + getconvergencestatus + res_mint_unit_cost."""

from decimal import Decimal

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, assert_greater_than, assert_raises_rpc_error


class RigorConvergenceRpcStatusHistoryTest(BitcoinTestFramework):
    def set_test_params(self):
        self.chain = "devtest"
        self.setup_clean_chain = True
        self.num_nodes = 1
        self.extra_args = [["-minrelaytxfee=0", "-blockmintxfee=0"]]
        self.wallet_names = ["default_wallet"]
        self._requires_wallet = True

    def add_options(self, parser):
        self.add_wallet_options(parser)

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def assert_getconvergencestatus_consistency(self):
        node = self.nodes[0]
        s = node.getconvergencestatus()
        assert "res_total" in s
        assert "tip_height" in s
        assert "last_res_height" in s
        assert "issuance_authorized" in s
        assert "window_progress" not in s
        assert "last_winner_txid" not in s
        assert_equal(s["tip_height"], node.getblockcount())
        assert Decimal(str(s["res_total"])) >= Decimal("0")
        if s["issuance_authorized"]:
            assert_greater_than(int(s["destroy_cost"]), 0)
            assert "epoch" not in s
            assert "rate" not in s
            assert_greater_than(int(s["min_atom_burn"]), 0)
            cost = node.res_mint_unit_cost()
            assert_equal(int(s["min_atom_burn"]), int(cost["min_atom_burn"]))
            assert_equal(int(s["min_whole_res_burn"]), int(cost["min_whole_res_burn"]))
            assert_greater_than(int(s["min_whole_res_burn"]), int(s["min_atom_burn"]))

        bi = node.getblockchaininfo()
        assert "cumulative_convergence" in bi
        assert Decimal(str(bi["cumulative_convergence"])) >= Decimal("0")

        hdr = node.getblockheader(node.getbestblockhash(), verbose=True)
        assert "cumulative_convergence" in hdr

        bf = node.getblockconvergence(node.getbestblockhash())
        assert_equal(bf["hash"], node.getbestblockhash())
        assert "cumulative_convergence" in bf
        assert "winner_txid" not in bf

    def run_test(self):
        node = self.nodes[0]
        mining_addr = node.getnewaddress()

        self.log.info("Mine coinbase maturity")
        self.generatetoaddress(node, 120, mining_addr)
        node.syncwithvalidationinterfacequeue()

        self.log.info("Test #1: getconvergencestatus / res_mint_unit_cost")
        self.assert_getconvergencestatus_consistency()
        cost = node.res_mint_unit_cost()
        assert_equal(cost["authorized"], True)
        assert "getdrawclosurestatus" not in node.help()
        assert "getconvergeinfo" not in node.help()
        assert "getconvergenceresult" not in node.help()
        assert "getconvergencestats" not in node.help()

        self.log.info("Test #1b: createconvergence burns ORU and writes res_out")
        minted = node.createconvergence("1.0")
        assert_equal(len(minted["txid"]), 64)
        assert_equal(minted["burn_oru"], 100_000_000)
        assert_greater_than(int(minted["res_out"]), 0)
        node.syncwithvalidationinterfacequeue()
        assert minted["txid"] in node.getrawmempool()
        me = node.getmempoolentry(minted["txid"])
        assert_equal(me["convergence"]["is_convergence"], True)
        assert_equal(me["convergence"]["meets_min_convergence"], True)
        assert Decimal(str(me["convergence"]["convergence_amount"])) >= Decimal("1")
        self.generatetoaddress(node, 1, mining_addr)

        self.log.info("Test #1c: createconvergence verbose=true adds fee_reason")
        v = node.createconvergence("1.0", None, None, False, None, None, "unset", False, None, True)
        assert "fee_reason" in v
        assert_equal(len(v["txid"]), 64)
        node.syncwithvalidationinterfacequeue()
        assert v["txid"] in node.getrawmempool()
        self.generatetoaddress(node, 1, mining_addr)

        self.log.info("Both mint txs confirmed")
        seen_txids = set()
        for depth in range(3):
            h = node.getblockhash(node.getblockcount() - depth)
            blk = node.getblock(h, 2)
            seen_txids.update(t["txid"] for t in blk["tx"])
        assert minted["txid"] in seen_txids
        assert v["txid"] in seen_txids

        self.assert_getconvergencestatus_consistency()


if __name__ == "__main__":
    RigorConvergenceRpcStatusHistoryTest().main()

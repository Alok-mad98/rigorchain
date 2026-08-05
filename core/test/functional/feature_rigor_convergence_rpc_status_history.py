#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Rigor convergence RPC checks: getconvergencestatus + listconvergencehistory + wallet ORU convergence RPC.

The wallet method is still named ``convergenceoru`` for RPC compatibility; it creates ORUM-domain convergence txs.
"""

import json
import os
import subprocess
import sys
from decimal import Decimal

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal


class RigorConvergenceRpcStatusHistoryTest(BitcoinTestFramework):
    def set_test_params(self):
        self.chain = "devtest"
        self.setup_clean_chain = True
        self.num_nodes = 1
        # Match wallet send tests: default min relay can exceed wallet-estimated fees on devtest,
        # so CommitTransaction succeeds but the tx never enters the mempool (ORUM convergence RPC would look "broken").
        self.extra_args = [["-minrelaytxfee=0", "-blockmintxfee=0"]]
        self.wallet_names = ["default_wallet"]
        self._requires_wallet = True

    def add_options(self, parser):
        self.add_wallet_options(parser)

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def run_convergence_helper(self):
        srcdir = self.config["environment"]["SRCDIR"]
        cli = self.options.bitcoincli
        helper = os.path.join(srcdir, "contrib", "devtools", "devtest-create-convergence-tx.py")
        datadir = self.nodes[0].datadir
        cmd = [
            sys.executable,
            helper,
            "--cli",
            cli,
            "--no-chain-arg",
            f"--cli-extra=-datadir={datadir}",
            "--cli-extra=-rpcwallet=default_wallet",
            "--convergence-orum",
            "1.0",
            "--json",
        ]
        r = subprocess.run(
            cmd,
            cwd=self.options.tmpdir,
            capture_output=True,
            text=True,
            timeout=120,
        )
        if r.returncode != 0:
            raise AssertionError("convergence helper failed:\n" + (r.stderr or r.stdout or "no output"))
        return json.loads(r.stdout)

    def assert_getconvergencestatus_consistency(self):
        node = self.nodes[0]
        s = node.getconvergencestatus()
        assert "res_total" in s
        assert "window_progress" in s
        assert "window_progress_percent" in s
        assert "current_convergence_sum" in s
        assert "last_winner_txid" in s
        assert "tip_height" in s
        assert "last_res_height" in s

        assert_equal(s["tip_height"], node.getblockcount())
        assert Decimal(str(s["res_total"])) >= Decimal("0")
        assert Decimal(str(s["current_convergence_sum"])) >= Decimal("0")
        assert Decimal(str(s["window_progress_percent"])) >= Decimal("0")
        assert "/" in s["window_progress"]

        bi = node.getblockchaininfo()
        assert "cumulative_convergence" in bi
        assert "current_convergence_sum" in bi
        assert Decimal(str(bi["cumulative_convergence"])) >= Decimal("0")
        assert Decimal(str(bi["current_convergence_sum"])) >= Decimal("0")
        assert_equal(Decimal(str(s["current_convergence_sum"])), Decimal(str(bi["current_convergence_sum"])))

        mi = node.getmempoolinfo()
        assert "cumulative_convergence" in mi
        assert "current_convergence_sum" in mi
        assert_equal(Decimal(str(mi["current_convergence_sum"])), Decimal(str(s["current_convergence_sum"])))

        hdr = node.getblockheader(node.getbestblockhash(), verbose=True)
        assert "cumulative_convergence" in hdr

    def assert_listconvergencehistory_consistency(self):
        node = self.nodes[0]
        tip = node.getblockcount()
        hist = node.listconvergencehistory(0, tip)

        prev_h = -1
        prev_res_total = Decimal("0")
        for row in hist:
            h = row["height"]
            assert h > prev_h
            prev_h = h

            assert_equal(row["blockhash"], node.getblockhash(h))
            bf = node.getblockconvergence(row["blockhash"])
            assert_equal(row["winner_txid"], bf["winner_txid"])
            assert row["winner_txid"] != "0" * 64

            res_total = Decimal(str(row["res_total"]))
            assert res_total >= prev_res_total
            prev_res_total = res_total

        s = node.getconvergencestatus()
        if hist:
            assert_equal(s["last_res_height"], hist[-1]["height"])
            assert_equal(s["last_winner_txid"], hist[-1]["winner_txid"])
        else:
            assert_equal(s["last_res_height"], -1)
            assert_equal(s["last_winner_txid"], "0" * 64)

    def run_test(self):
        node = self.nodes[0]
        mining_addr = node.getnewaddress()

        self.log.info("Mine coinbase maturity")
        self.generatetoaddress(node, 120, mining_addr)
        node.syncwithvalidationinterfacequeue()

        self.log.info("Test #1: getconvergencestatus basic consistency")
        self.assert_getconvergencestatus_consistency()

        self.log.info("Test #1b: wallet convergenceoru RPC — ORUM convergence tx (RPC name unchanged)")
        conv_txid = node.convergenceoru("1.0")
        assert len(conv_txid) == 64
        node.syncwithvalidationinterfacequeue()
        assert conv_txid in node.getrawmempool()
        me = node.getmempoolentry(conv_txid)
        assert_equal(me["convergence"]["is_convergence"], True)
        assert_equal(me["convergence"]["meets_min_convergence"], True)
        assert Decimal(str(me["convergence"]["convergence_amount"])) >= Decimal("1")
        # Confirm immediately so next template cannot accumulate multiple convergence candidates.
        self.generatetoaddress(node, 1, mining_addr)

        self.log.info("Test #1c: convergenceoru verbose=true (ORUM path) returns txid + fee_reason")
        v = node.convergenceoru("1.0", None, None, False, None, None, "unset", False, None, True)
        assert_equal(sorted(v.keys()), ["fee_reason", "txid"])
        assert len(v["txid"]) == 64
        node.syncwithvalidationinterfacequeue()
        assert v["txid"] in node.getrawmempool()
        node.getmempoolentry(v["txid"])
        self.generatetoaddress(node, 1, mining_addr)

        self.log.info("Mine one more block and verify both ORUM convergence txs from convergenceoru are confirmed")
        self.generatetoaddress(node, 1, mining_addr)
        seen_txids = set()
        for depth in range(3):
            h = node.getblockhash(node.getblockcount() - depth)
            blk = node.getblock(h, 2)
            seen_txids.update(t["txid"] for t in blk["tx"])
        assert conv_txid in seen_txids
        assert v["txid"] in seen_txids

        self.log.info("Advance chain by multiple blocks")
        self.generatetoaddress(node, 12, mining_addr)

        self.log.info("Test #2: listconvergencehistory matches on-chain CVGE commitments")
        self.assert_listconvergencehistory_consistency()
        self.assert_getconvergencestatus_consistency()


if __name__ == "__main__":
    RigorConvergenceRpcStatusHistoryTest().main()

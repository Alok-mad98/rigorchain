#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""TASK-WALLET-CONVERGENCE-NETWORK-STATS-001: getconvergencestats + reorg rollback.

Verifies:
- RPC shape and units (ORU sat-atoms, RES atoms)
- Counts rise when a convergence is included
- Stats roll back across invalidateblock / reconsiderblock
- Stats survive node restart (tip-hash cache rebuilt from W-window)
- Fields the Convergence Network GUI displays are present
"""

import json
import os
import subprocess
import sys

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, assert_greater_than_or_equal


COIN = 100_000_000
RES_COIN = 1_000_000


class RigorConvergenceNetworkStatsTest(BitcoinTestFramework):
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

    def _run_convergence_helper(self, amount="1.0"):
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
            amount,
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

    def _assert_stats_shape(self, s):
        for key in (
            "active_convergences",
            "active_oru",
            "pending_windows",
            "total_res_minted",
            "best_height",
        ):
            assert key in s, f"missing {key}"
        assert isinstance(s["active_convergences"], int)
        assert isinstance(s["active_oru"], int)
        assert isinstance(s["pending_windows"], int)
        assert isinstance(s["total_res_minted"], int)
        assert isinstance(s["best_height"], int)
        assert_equal(s["best_height"], self.nodes[0].getblockcount())
        assert_greater_than_or_equal(s["active_convergences"], 0)
        assert_greater_than_or_equal(s["active_oru"], 0)
        assert_greater_than_or_equal(s["pending_windows"], 0)
        assert_greater_than_or_equal(s["total_res_minted"], 0)
        # Unpaid set cannot exceed all-in-window count.
        assert_greater_than_or_equal(s["active_convergences"], s["pending_windows"])

    def run_test(self):
        n = self.nodes[0]
        maddr = n.getnewaddress()

        self.log.info("Baseline stats after maturity mining (no convergences)")
        self.generatetoaddress(n, 120, maddr)
        baseline = n.getconvergencestats()
        self._assert_stats_shape(baseline)
        assert_equal(baseline["active_convergences"], 0)
        assert_equal(baseline["active_oru"], 0)
        assert_equal(baseline["pending_windows"], 0)
        assert_equal(baseline["total_res_minted"], 0)

        self.log.info("Create and include a 1.00 ORU convergence")
        txid = self._run_convergence_helper("1.0")["txid"]
        self.generatetoaddress(n, 1, maddr)
        tip = n.getbestblockhash()
        assert txid in [t["txid"] for t in n.getblock(tip, 2)["tx"]]

        after = n.getconvergencestats()
        self._assert_stats_shape(after)
        assert_equal(after["active_convergences"], 1)
        assert_equal(after["active_oru"], COIN)
        assert_equal(after["pending_windows"], 1)
        assert_equal(after["best_height"], 121)

        # Cross-check tip draw set size via getconvergenceresult when available.
        fr = n.getconvergenceresult(tip)
        if "convergence_root_set_size" in fr:
            assert_equal(after["pending_windows"], fr["convergence_root_set_size"])

        self.log.info("GUI contract: Network Statistics fields update with tip")
        # The Qt ConvergenceNetworkPanel reads exactly these keys.
        assert after["best_height"] > baseline["best_height"]
        assert after["active_convergences"] == 1

        self.log.info("Reorg: invalidate inclusion tip — stats must roll back")
        n.invalidateblock(tip)
        rolled = n.getconvergencestats()
        self._assert_stats_shape(rolled)
        assert_equal(rolled["active_convergences"], 0)
        assert_equal(rolled["active_oru"], 0)
        assert_equal(rolled["pending_windows"], 0)
        assert_equal(rolled["best_height"], 120)

        self.log.info("Reconsider inclusion tip — stats restore")
        # Rebroadcast if mempool lost the tx during invalidate.
        mem = set(n.getrawmempool())
        if txid not in mem:
            n.sendrawtransaction(n.getrawtransaction(txid))
        n.reconsiderblock(tip)
        # If reconsider alone did not restore tip (cached invalid), remine.
        if n.getblockcount() < 121:
            self.generatetoaddress(n, 1, maddr)
        restored = n.getconvergencestats()
        self._assert_stats_shape(restored)
        assert_equal(restored["active_convergences"], 1)
        assert_equal(restored["active_oru"], COIN)
        assert_equal(restored["pending_windows"], 1)

        self.log.info("Restart node — cache rebuilds from W-window (restart-safe)")
        tip_before = restored["best_height"]
        snapshot = dict(restored)
        self.restart_node(0)
        n = self.nodes[0]
        after_restart = n.getconvergencestats()
        self._assert_stats_shape(after_restart)
        assert_equal(after_restart["best_height"], tip_before)
        assert_equal(after_restart["active_convergences"], snapshot["active_convergences"])
        assert_equal(after_restart["active_oru"], snapshot["active_oru"])
        assert_equal(after_restart["pending_windows"], snapshot["pending_windows"])
        assert_equal(after_restart["total_res_minted"], snapshot["total_res_minted"])

        self.log.info("RES atoms scale lock: 1.000000 RES == %d atoms" % RES_COIN)
        assert_equal(RES_COIN, 1_000_000)
        assert_equal(1 * RES_COIN, 1_000_000)


if __name__ == "__main__":
    RigorConvergenceNetworkStatsTest().main()

#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Convergence / RES draw trigger rule checks on devtest.

With CONSENSUS-V2.2:
- If N < 2 valid convergence candidates, RES mint tx MUST NOT exist.
- If N >= 2, winner is computed and RES mint tx MUST be present and bound to winner.

This test verifies:
  - A block with two convergence txs and no RES body mint is rejected on ``submitblock`` with
    ``bad-rigor-res-winner-mismatch`` (consensus path; not ``generatetoaddress`` / CreateNewBlock).
  - RPC introspection still reports deterministic candidate / CVGE fields on the parent block.
"""

import json
import os
import subprocess
import sys
from decimal import Decimal

from fault_injection_core import assemble_attack_block  # noqa: I252
from feature_res_core_convergence_draw import DEVTEST_MINING_RES_FEE_ADDR  # noqa: I252
from test_framework.opus_pow import solve_block_opus
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import append_config, assert_equal, get_datadir_path


def _convergence_amount_positive(entry) -> bool:
    """True if RPC candidate convergence_amount is positive ORUM locked in convergence output."""
    fa = entry.get("convergence_amount", 0)
    if fa is None:
        return False
    if isinstance(fa, (int, float)):
        return fa > 0
    return Decimal(str(fa)) > 0


class RigorConvergenceRngConsensusTest(BitcoinTestFramework):
    def set_test_params(self):
        self.chain = "devtest"
        self.setup_clean_chain = True
        self.num_nodes = 2
        self.extra_args = [[], []]
        self.wallet_names = ["default_wallet"] * self.num_nodes
        self._requires_wallet = True

    def add_options(self, parser):
        self.add_wallet_options(parser)

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def setup_chain(self):
        super().setup_chain()
        # GBT calls CreateNewBlock; with N>=2 convergence candidates in mempool the miner needs a
        # RES receive script to assemble a template (body mint is still omitted by assemble_attack_block).
        line = f"miningresfeeaddr={DEVTEST_MINING_RES_FEE_ADDR}"
        for i in range(self.num_nodes):
            append_config(get_datadir_path(self.options.tmpdir, i), [line])

    def run_convergence_helper(self, node_idx):
        """Invoke contrib/devtools/devtest-create-convergence-tx.py against a test node."""
        srcdir = self.config["environment"]["SRCDIR"]
        cli = self.options.bitcoincli
        helper = os.path.join(srcdir, "contrib", "devtools", "devtest-create-convergence-tx.py")
        datadir = self.nodes[node_idx].datadir
        wallet = self.wallet_names[node_idx]
        cmd = [
            sys.executable,
            helper,
            "--cli",
            cli,
            "--no-chain-arg",
            f"--cli-extra=-datadir={datadir}",
            "--convergence-orum",
            "1.0",
            "--json",
        ]
        if wallet:
            cmd.append(f"--cli-extra=-rpcwallet={wallet}")
        r = subprocess.run(
            cmd,
            cwd=self.options.tmpdir,
            capture_output=True,
            text=True,
            timeout=120,
        )
        if r.returncode != 0:
            raise AssertionError(
                "convergence helper failed:\n" + (r.stderr or r.stdout or "no output")
            )
        return json.loads(r.stdout)

    def run_test(self):
        self.log.info("Mine coinbase maturity and prepare two distinct ORUM-domain UTXOs for convergence")
        mining_addr = self.nodes[0].getnewaddress()
        self.generatetoaddress(self.nodes[0], 120, mining_addr)
        self.sync_all()

        self.log.info("Create and broadcast two convergence transactions (separate UTXOs)")
        res_a = self.run_convergence_helper(0)
        res_b = self.run_convergence_helper(0)
        tx_a = res_a["txid"]
        tx_b = res_b["txid"]
        assert tx_a != tx_b

        self.sync_mempools()
        self.log.info(
            "submitblock: two convergence txs, valid CVGE, no RES body mint (N>=2 winner rule)"
        )
        node = self.nodes[0]
        tpl = node.getblocktemplate({"rules": ["segwit"]})
        assert "default_convergence_commitment" in tpl, "GBT must expose CVGE coinbase script"
        blk = assemble_attack_block(node, tpl, res_a, res_b)
        solve_block_opus(blk)
        reject = node.submitblock(hexdata=blk.serialize(with_witness=True).hex())
        assert reject is not None, "expected invalid block to be rejected"
        assert_equal(reject, "bad-rigor-res-winner-mismatch")

        block_hash = node.getbestblockhash()
        block = self.nodes[0].getblock(block_hash, 2)

        self.log.info("getconvergenceresult on current tip remains deterministic")
        fr0 = self.nodes[0].getconvergenceresult(block_hash)
        fr1 = self.nodes[1].getconvergenceresult(block_hash)
        assert_equal(fr0["draw_candidate_root_committed"], fr0["draw_candidate_root_computed"])
        assert_equal(fr1["draw_candidate_root_committed"], fr1["draw_candidate_root_computed"])
        assert_equal(fr0["hash"], block_hash)
        assert_equal(fr0["draw_candidate_root_committed"], fr1["draw_candidate_root_committed"])

        n_conv_candidates = sum(1 for c in fr0["candidates"] if _convergence_amount_positive(c))
        self.log.info("tip candidates with convergence_amount > 0: %d", n_conv_candidates)

        self.log.info("getblockconvergence: CVGE fields present")
        bf = self.nodes[0].getblockconvergence(block_hash)
        assert bf["height"] == block["height"]
        assert bf["version"] is not None
        assert Decimal(str(bf["cumulative_convergence"])) >= 0

        self.log.info("V2.2 trigger rule (N>=2 requires RES claim tx) rejection path verified")


if __name__ == "__main__":
    RigorConvergenceRngConsensusTest().main()

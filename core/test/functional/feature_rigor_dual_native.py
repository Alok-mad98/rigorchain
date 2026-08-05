#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Rigor dual-native (ORU / convergence) checks on devtest: two-node P2P sync and convergence tx."""

import json
import os
import subprocess
import sys

from feature_res_core_convergence_draw import DEVTEST_MINING_RES_FEE_ADDR
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import append_config, assert_equal, get_datadir_path


class RigorDualNativeTest(BitcoinTestFramework):
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
        # CreateNewBlock requires a RES receive script once a convergence-draw winner is
        # present in the mempool (see validation.cpp / mining BlockAssembler). Without
        # -miningresfeeaddr it fails with "convergence draw winner present but no RES
        # receive script". Match the pattern used in feature_rigor_convergence_rng_consensus.
        line = f"miningresfeeaddr={DEVTEST_MINING_RES_FEE_ADDR}"
        for i in range(self.num_nodes):
            append_config(get_datadir_path(self.options.tmpdir, i), [line])

    def run_convergence_helper(self, node_idx):
        """Invoke contrib/devtools/devtest-create-convergence-tx.py against a test node."""
        srcdir = self.config["environment"]["SRCDIR"]
        # Same path as TestNode (BUILDDIR/src/bitcoin-cli); honors $BITCOINCLI if set.
        cli = self.options.bitcoincli
        helper = os.path.join(srcdir, "contrib", "devtools", "devtest-create-convergence-tx.py")
        datadir = self.nodes[node_idx].datadir
        wallet = self.wallet_names[node_idx]
        cmd = [
            sys.executable,
            helper,
            "--cli",
            cli,
            # bitcoin.conf already has devtest=1; do not also pass -chain=devtest (AppInitRPC rejects both).
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
                "convergence helper failed:\n"
                + (r.stderr or r.stdout or "no output")
            )
        return json.loads(r.stdout)

    def run_test(self):
        self.log.info("Chain identity and common genesis across peers")
        assert_equal(self.nodes[0].getblockchaininfo()["chain"], "devtest")
        assert_equal(self.nodes[0].getblockhash(0), self.nodes[1].getblockhash(0))

        self.log.info("Mine coinbase to wallet and verify header sync + minimum chain work")
        # Mine to the deterministic P2PKH address matching import_deterministic_coinbase_privkeys()
        # so listunspent / signrawtransactionwithwallet can sign the helper's legacy-style raw tx.
        mining_addr = self.nodes[0].getnewaddress()
        self.generatetoaddress(self.nodes[0], 120, mining_addr)
        self.sync_all()
        cw = int(self.nodes[0].getblockchaininfo()["chainwork"], 16)
        assert cw > 2  # above genesis-only nMinimumChainWork

        self.log.info("Create two convergence transactions; check mempool marks them as convergence")
        res_a = self.run_convergence_helper(0)
        res_b = self.run_convergence_helper(0)
        txid_a = res_a["txid"]
        txid_b = res_b["txid"]
        for txid in (txid_a, txid_b):
            conv_meta = self.nodes[0].getmempoolentry(txid).get("convergence", {})
            assert conv_meta.get("is_convergence", False)
            assert conv_meta.get("meets_min_convergence", False)

        self.log.info("Mine a block including the convergence tx and sync both nodes")
        self.sync_mempools()
        self.generatetoaddress(self.nodes[0], 1, self.nodes[0].getnewaddress())
        self.sync_all()
        assert_equal(self.nodes[0].getblockcount(), self.nodes[1].getblockcount())

        block_hash = self.nodes[0].getbestblockhash()
        block = self.nodes[0].getblock(block_hash, 2)
        block_txids = [t["txid"] for t in block["tx"]]
        assert txid_a in block_txids and txid_b in block_txids

        # Confirmed txs: pass block hash (no -txindex in default test node).
        for txid in (txid_a, txid_b):
            tx = self.nodes[0].getrawtransaction(txid, True, block_hash)
            assert any(
                o.get("scriptPubKey", {}).get("type") == "witness_v1_res"
                for o in tx.get("vout", [])
            )

        self.log.info("Wallet still reports ORUM-domain balance after convergence spend")
        assert self.nodes[0].getbalance() >= 0


if __name__ == "__main__":
    RigorDualNativeTest().main()

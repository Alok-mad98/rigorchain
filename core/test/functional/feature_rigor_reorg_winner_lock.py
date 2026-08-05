#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Reorg lock test: what a reorg may and may not change about a convergence draw.

SECURITY-001-A8 split this into two opposite properties, and the test asserts both.

*Locked:* a given block always awards the same draw. Disconnecting and reconnecting it
must reproduce the winner and every candidate σ bit for bit.

*Deliberately not locked:* a DIFFERENT block awards a different draw, even over the same
convergence transactions. σ is seeded by the awarding block's own hash, so rebuilding the
block with any change at all — here just a different coinbase payout address — re-rolls the
draw. That is the whole point: the only way to shop for a winner is to pay for another
proof-of-work solution per attempt.

Flow:
1) Build A->B(one-convergence block)->C
2) Record B winner / candidate digest map
3) invalidate C then B, reconsider both; assert B's draw is unchanged
4) invalidate again and rebuild B' (same convergence tx, different coinbase) then C'
5) Assert B' has a different hash and therefore a different σ assignment
"""

import json
import os
import subprocess
import sys

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal


def _candidate_digest_map(convergenceresult_obj):
    out = {}
    for c in convergenceresult_obj["candidates"]:
        out[c["txid"]] = {
            "h": c["h"],
            "draw_eligible": c["draw_eligible"],
            "convergence_amount": c["convergence_amount"],
        }
    return out


class RigorReorgWinnerLockTest(BitcoinTestFramework):
    def set_test_params(self):
        self.chain = "devtest"
        self.setup_clean_chain = True
        self.num_nodes = 1
        # miningresfeeaddr is required for auto-mint when a draw has a winner.
        # Use the devtest all-zeros witness v1 program.
        self.extra_args = [["-miningresfeeaddr=dvt1pqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqmsufsz"]]
        self.wallet_names = ["default_wallet"]
        self._requires_wallet = True

    def add_options(self, parser):
        self.add_wallet_options(parser)

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def _run_convergence_helper(self):
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

    def run_test(self):
        n = self.nodes[0]
        maddr = n.getnewaddress()

        self.log.info("Mine maturity")
        self.generatetoaddress(n, 120, maddr)

        self.log.info("Create one convergence tx and keep raw hex for rebroadcast")
        tx0 = self._run_convergence_helper()["txid"]
        raw0 = n.getrawtransaction(tx0)

        self.log.info("Mine B containing convergence tx, then mine C")
        self.generatetoaddress(n, 1, maddr)
        b_hash = n.getbestblockhash()
        b = n.getblock(b_hash, 2)
        b_txids = [t["txid"] for t in b["tx"]]
        assert tx0 in b_txids
        self.generatetoaddress(n, 1, maddr)
        c_hash = n.getbestblockhash()

        fr_b = n.getconvergenceresult(b_hash)
        winner_b = fr_b["draw_winner_txid"]
        cand_b = _candidate_digest_map(fr_b)
        assert_equal(fr_b["sigma_seed"], b_hash)

        self.log.info("Disconnect B and C, reconnect them: the same block must award the same draw")
        n.invalidateblock(c_hash)
        n.invalidateblock(b_hash)
        n.reconsiderblock(b_hash)
        n.reconsiderblock(c_hash)
        assert_equal(n.getbestblockhash(), c_hash)
        fr_b_again = n.getconvergenceresult(b_hash)
        assert_equal(winner_b, fr_b_again["draw_winner_txid"])
        assert_equal(cand_b, _candidate_digest_map(fr_b_again))

        self.log.info("Invalidate C and B again; return to parent A to rebuild")
        n.invalidateblock(c_hash)
        n.invalidateblock(b_hash)

        # invalidateblock leaves the old block header marked BLOCK_FAILED_VALID. A rebuilt
        # template identical to B (same parent, txs, coinbase payout script, nBits, …) can
        # yield the same block hash; AcceptBlockHeader then rejects it as a cached-invalid
        # duplicate. Use a fresh payout address for B'/C' so the coinbase differs.
        maddr_rebuild = n.getnewaddress()

        self.log.info("Rebroadcast convergence tx if needed")
        mem = set(n.getrawmempool())
        if tx0 not in mem:
            n.sendrawtransaction(raw0)

        self.log.info("Mine B' and C'")
        self.generatetoaddress(n, 1, maddr_rebuild)
        bp_hash = n.getbestblockhash()
        bp = n.getblock(bp_hash, 2)
        bp_txids = [t["txid"] for t in bp["tx"]]
        assert tx0 in bp_txids
        self.generatetoaddress(n, 1, maddr_rebuild)

        fr_bp = n.getconvergenceresult(bp_hash)
        cand_bp = _candidate_digest_map(fr_bp)
        assert_equal(fr_bp["sigma_seed"], bp_hash)

        self.log.info("A rebuilt block is a different block, so it re-rolls \u03c3 (Sigma-PoW-01)")
        assert bp_hash != b_hash, "rebuild must differ, or the case proves nothing"
        assert cand_b[tx0]["h"] != cand_bp[tx0]["h"], (
            "\u03c3 for the same convergence must move when the awarding block does; if it does not, "
            "a producer can re-roll the draw for free"
        )


if __name__ == "__main__":
    RigorReorgWinnerLockTest().main()

#!/usr/bin/env python3
# Copyright (c) 2026 The RigorChain developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
""": three-node pairwise divergence + STEP-0202 capture (regression).

Confirms ``pairwise_first_divergence_heights`` and ``capture_divergence`` for
labels A/B/C with deterministic ``first_mismatch.json`` keys and sorted JSON.
Does not modify consensus.
"""

from __future__ import annotations

import importlib.util
import json
import os

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal


def _load_capture_module():
    path = os.path.join(
        os.path.dirname(os.path.abspath(__file__)),
        "feature_replay_failure_capture.py",
    )
    spec = importlib.util.spec_from_file_location(
        "feature_replay_failure_capture",
        path,
    )
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


SYNC_TIMEOUT = 60


class ReplayFailureCaptureThreeNodePairwiseTest(BitcoinTestFramework):
    """Star topology (B,C -> A). Common prefix on A, sync, partition A from B
    and C, mine independently: pairwise first divergence at h=51; full capture.
    """

    def set_test_params(self):
        self.num_nodes = 3
        self.setup_clean_chain = True
        self.chain = "devtest"
        self.extra_args = [[], [], []]

    def setup_network(self):
        self.setup_nodes()
        self.connect_nodes(1, 0)
        self.connect_nodes(2, 0)
        self.sync_all()

    def run_test(self):
        rfc = _load_capture_module()
        na, nb, nc = self.nodes
        addr_a = na.get_deterministic_witness_mining_address()
        self.generatetoaddress(na, 50, addr_a)
        self.sync_blocks(self.nodes, timeout=SYNC_TIMEOUT)
        rfc._drain(na, nb, nc)

        self.disconnect_nodes(0, 1)
        self.disconnect_nodes(0, 2)
        self.wait_until(
            lambda: len(na.getpeerinfo()) == 0
            and len(nb.getpeerinfo()) == 0
            and len(nc.getpeerinfo()) == 0,
            timeout=30,
        )

        addr_b = nb.get_deterministic_witness_mining_address()
        addr_c = nc.get_deterministic_witness_mining_address()
        self.generatetoaddress(na, 3, addr_a, sync_fun=lambda: None)
        self.generatetoaddress(nb, 3, addr_b, sync_fun=lambda: None)
        self.generatetoaddress(nc, 3, addr_c, sync_fun=lambda: None)
        rfc._drain(na, nb, nc)

        max_h = min(na.getblockcount(), nb.getblockcount(), nc.getblockcount())
        assert_equal(max_h, 53)

        pair = rfc.pairwise_first_divergence_heights(
            {"A": na, "B": nb, "C": nc},
            max_h,
        )
        assert set(pair.keys()) == {"A_vs_B", "A_vs_C", "B_vs_C"}, pair
        assert_equal(pair["A_vs_B"], 51)
        assert_equal(pair["A_vs_C"], 51)
        assert_equal(pair["B_vs_C"], 51)

        out_dir = os.path.join(self.options.tmpdir, "capture_three_node")
        rfc.capture_divergence(
            self.log,
            out_dir,
            {"A": na, "B": nb, "C": nc},
            51,
            harness_checkpoint_height=51,
 harness_failure_reason=" synthetic 3-node partition",
        )

        mm_path = os.path.join(out_dir, "first_mismatch.json")
        with open(mm_path, encoding="ascii") as f:
            mm = json.load(f)
        pd = mm["pairwise_first_divergence"]
        assert list(sorted(pd.keys())) == ["A_vs_B", "A_vs_C", "B_vs_C"]
        assert_equal(pd["A_vs_B"], 51)
        assert_equal(pd["A_vs_C"], 51)
        assert_equal(pd["B_vs_C"], 51)

        required = [
            "first_mismatch.json",
            "trace.tsv",
            "A_state.json",
            "B_state.json",
            "C_state.json",
            "A_replaysnap.json",
            "B_replaysnap.json",
            "C_replaysnap.json",
            "A_replaysnap_rpl2.hex",
            "B_replaysnap_rpl2.hex",
            "C_replaysnap_rpl2.hex",
            "A_mint_state.json",
            "B_mint_state.json",
            "C_mint_state.json",
            "A_chainstatesummary.json",
            "B_chainstatesummary.json",
            "C_chainstatesummary.json",
            "A_selection.json",
            "B_selection.json",
            "C_selection.json",
            "A_block_h.json",
            "B_block_h.json",
            "C_block_h.json",
            "A_block_parent.json",
            "B_block_parent.json",
            "C_block_parent.json",
        ]
        for name in required:
            p = os.path.join(out_dir, name)
            assert os.path.isfile(p), f"missing {p}"
            assert os.path.getsize(p) > 0, f"empty {p}"

        for label in ("A", "B", "C"):
            ut = os.path.join(out_dir, f"{label}_utxo.dat")
            err = ut + ".error"
            assert os.path.isfile(ut) or os.path.isfile(err), (
                f"expected {label}_utxo.dat or sidecar .error"
            )

        print("STEP-0202-3NODE-PAIRWISE: PASS", flush=True)
 self.log.info(" three-node capture artifacts OK under %s", out_dir)


if __name__ == "__main__":
    ReplayFailureCaptureThreeNodePairwiseTest().main()

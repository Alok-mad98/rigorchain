#!/usr/bin/env python3
# Copyright (c) 2026 The RigorChain developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Baseline deterministic replay — long chain + periodic graceful NODE-B restart.

TASK baseline (no fault injection, no SIGKILL, no chaos):
  * STAR topology: NODE-A mines; NODE-B / NODE-C sync only via A.
  * 5000 blocks; every 100 blocks: ``getdeterministicstatehash`` +
    ``getreplaysnapshot`` (RPL2) byte-equal across A/B/C.
  * At heights 1000, 2000, 3000, 4000: graceful ``restart_node`` on NODE-B,
    reconnect B->A, resync; NODE-B state_hash MUST match pre-restart; tri-node
    equality MUST hold (RULE-2 restart replay).

Complements ``feature_deterministic_replay.py`` (1000 blocks + one B restart +
reindex C) and ``feature_replay_burnin.py`` (optional SIGKILL — excluded here).

Stdout on success: ``BASELINE-REPLAY-LONGRUN: PASS``
"""

from __future__ import annotations

import importlib.util
import os

from test_framework.test_framework import BitcoinTestFramework


def _load_replay_failure_capture():
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


CHECK_INTERVAL = 100
TARGET_BLOCKS = 5000
# Periodic graceful NODE-B restarts after these checkpoint heights (frozen).
RESTART_B_HEIGHTS = frozenset({1000, 2000, 3000, 4000})
SYNC_TIMEOUT = 240


class BaselineReplayLongrunTest(BitcoinTestFramework):
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

    def _drain_validation_queue(self, *nodes):
        for n in nodes:
            n.syncwithvalidationinterfacequeue()

    def _sync_mocktime_to_tip(self, *nodes):
        if not nodes:
            return
        tip_hash = nodes[0].getbestblockhash()
        tip_time = int(nodes[0].getblockheader(tip_hash)["time"])
        for n in nodes:
            n.setmocktime(tip_time)

    def _det_state(self, node):
        s = node.getdeterministicstatehash()
        return {
            "height": s["height"],
            "best_block_hash": s["best_block_hash"],
            "state_hash": s["state_hash"],
        }

    def _replay_snap(self, node):
        r = node.getreplaysnapshot()
        for key in (
            "height",
            "block_hash",
            "state_hash",
            "best_block_hash",
            "utxo_count",
            "serialized_snapshot",
        ):
            assert key in r, f"getreplaysnapshot missing required key {key!r}"
        return {
            "height": r["height"],
            "block_hash": r["block_hash"],
            "state_hash": r["state_hash"],
            "best_block_hash": r["best_block_hash"],
            "utxo_count": r["utxo_count"],
            "serialized_snapshot": r["serialized_snapshot"],
        }

    def _fail(self, height, reason):
        verdict = f"BASELINE-LONGRUN: DIVERGENCE_AT_HEIGHT={height}"
        print(verdict, flush=True)
        self.log.error(f"{verdict} — {reason}")
        capture_dir = os.path.join(self.options.tmpdir, "replay_divergence_capture")
        try:
            rfc = _load_replay_failure_capture()
            node_a, node_b, node_c = self.nodes[0], self.nodes[1], self.nodes[2]
            self._drain_validation_queue(node_a, node_b, node_c)
            max_h = min(
                node_a.getblockcount(),
                node_b.getblockcount(),
                node_c.getblockcount(),
            )
            pair = rfc.pairwise_first_divergence_heights(
                {"A": node_a, "B": node_b, "C": node_c},
                max_h,
            )
            div_candidates = [h for h in pair.values() if h is not None]
            div_h = min(div_candidates) if div_candidates else height
            rfc.capture_divergence(
                self.log,
                capture_dir,
                {"A": node_a, "B": node_b, "C": node_c},
                div_h,
                harness_checkpoint_height=height,
                harness_failure_reason=reason,
            )
            print(f"STEP-0202_CAPTURE_DIR={capture_dir}", flush=True)
        except Exception as e:
            self.log.error("STEP-0202 capture failed (primary verdict unchanged): %r", e)
        raise AssertionError(f"{verdict} — {reason}")

    def _assert_tri_equal(self, label, height, sa, sb, sc, ra, rb, rc):
        for lab, s in (("A", sa), ("B", sb), ("C", sc)):
            if s["height"] != height:
                self._fail(
                    height,
                    f"[{label}] getdeterministicstatehash height NODE-{lab}: "
                    f"{s['height']} != {height}",
                )
        if sa["best_block_hash"] != sb["best_block_hash"]:
            self._fail(
                height,
                f"[{label}] BEST_BLOCK_MISMATCH A vs B: "
                f"{sa['best_block_hash']} vs {sb['best_block_hash']}",
            )
        if sb["best_block_hash"] != sc["best_block_hash"]:
            self._fail(
                height,
                f"[{label}] BEST_BLOCK_MISMATCH B vs C: "
                f"{sb['best_block_hash']} vs {sc['best_block_hash']}",
            )
        if sa["state_hash"] != sb["state_hash"]:
            self._fail(
                height,
                f"[{label}] STATE_MISMATCH A vs B: "
                f"{sa['state_hash']} vs {sb['state_hash']}",
            )
        if sb["state_hash"] != sc["state_hash"]:
            self._fail(
                height,
                f"[{label}] STATE_MISMATCH B vs C: "
                f"{sb['state_hash']} vs {sc['state_hash']}",
            )

        for lab, r, s in (("A", ra, sa), ("B", rb, sb), ("C", rc, sc)):
            if r["state_hash"] != s["state_hash"]:
                self._fail(
                    height,
                    f"[{label}] REPLAY vs DET on NODE-{lab}: "
                    f"{r['state_hash']} vs {s['state_hash']}",
                )
        for lab, r, s in (("A", ra, sa), ("B", rb, sb), ("C", rc, sc)):
            if r["height"] != height:
                self._fail(
                    height,
                    f"[{label}] getreplaysnapshot.height NODE-{lab}: "
                    f"{r['height']} != {height}",
                )
            if r["block_hash"] != s["best_block_hash"]:
                self._fail(
                    height,
                    f"[{label}] REPLAY_BLOCK_HASH_VS_TIP NODE-{lab}",
                )
        if ra["block_hash"] != rb["block_hash"] or rb["block_hash"] != rc["block_hash"]:
            self._fail(height, f"[{label}] RPL2 block_hash mismatch across nodes")
        if ra["serialized_snapshot"] != rb["serialized_snapshot"]:
            self._fail(height, f"[{label}] RPL2 serialized_snapshot A vs B mismatch")
        if rb["serialized_snapshot"] != rc["serialized_snapshot"]:
            self._fail(height, f"[{label}] RPL2 serialized_snapshot B vs C mismatch")
        if ra["utxo_count"] != rb["utxo_count"] or rb["utxo_count"] != rc["utxo_count"]:
            self._fail(
                height,
                f"[{label}] utxo_count A={ra['utxo_count']} B={rb['utxo_count']} "
                f"C={rc['utxo_count']}",
            )

        self.log.info(f"[{label} h={height}] state_hash={sa['state_hash']}")

    def _log_path(self):
        return os.path.join(self.options.tmpdir, "baseline_replay_longrun.log")

    def _log_line(self, tag, height, sa, sb, sc, restart):
        with open(self._log_path(), "a", encoding="ascii") as f:
            f.write(
                "\t".join([
                    tag,
                    f"h={height}",
                    f"A={sa['state_hash']}",
                    f"B={sb['state_hash']}",
                    f"C={sc['state_hash']}",
                    f"best={sa['best_block_hash']}",
                    f"restart_B={restart}",
                ])
                + "\n"
            )

    def _assert_topology(self):
        node_b, node_c = self.nodes[1], self.nodes[2]

        def peers_of(n):
            return [p.get("subver", "") for p in n.getpeerinfo()]

        b_peers = peers_of(node_b)
        c_peers = peers_of(node_c)
        assert not any("testnode2" in s for s in b_peers), (
            f"NODE-B must not peer NODE-C: {b_peers}"
        )
        assert not any("testnode1" in s for s in c_peers), (
            f"NODE-C must not peer NODE-B: {c_peers}"
        )

    def run_test(self):
        node_a, node_b, node_c = self.nodes

        with open(self._log_path(), "w", encoding="ascii") as f:
            f.write(
                "# baseline_replay_longrun: TAB-separated, no timestamps\n"
                f"# target_blocks={TARGET_BLOCKS} checkpoint={CHECK_INTERVAL} "
                f"restart_B_heights={sorted(RESTART_B_HEIGHTS)}\n"
            )

        self._assert_topology()
        addr = node_a.get_deterministic_witness_mining_address()

        self.log.info(
            "Mining %s blocks (%s-block checkpoints); graceful NODE-B restarts at %s",
            TARGET_BLOCKS,
            CHECK_INTERVAL,
            sorted(RESTART_B_HEIGHTS),
        )

        for batch_start in range(0, TARGET_BLOCKS, CHECK_INTERVAL):
            batch_end = min(batch_start + CHECK_INTERVAL, TARGET_BLOCKS)
            n_gen = batch_end - batch_start
            self.generatetoaddress(node_a, n_gen, addr)
            self.sync_blocks(self.nodes, timeout=SYNC_TIMEOUT)
            self._sync_mocktime_to_tip(node_a, node_b, node_c)

            ha = node_a.getblockcount()
            hb = node_b.getblockcount()
            hc = node_c.getblockcount()
            if ha != batch_end or hb != batch_end or hc != batch_end:
                self._fail(
                    batch_end,
                    f"HEIGHT_MISMATCH A={ha} B={hb} C={hc} expected={batch_end}",
                )

            ba = node_a.getbestblockhash()
            bb = node_b.getbestblockhash()
            bc = node_c.getbestblockhash()
            if ba != bb or bb != bc:
                self._fail(
                    batch_end,
                    f"BEST_BLOCK_MISMATCH A={ba} B={bb} C={bc}",
                )

            self._drain_validation_queue(node_a, node_b, node_c)
            sa = self._det_state(node_a)
            sb = self._det_state(node_b)
            sc = self._det_state(node_c)
            ra = self._replay_snap(node_a)
            rb = self._replay_snap(node_b)
            rc = self._replay_snap(node_c)
            self._assert_tri_equal("CHECKPOINT", batch_end, sa, sb, sc, ra, rb, rc)
            self._log_line("CHECKPOINT", batch_end, sa, sb, sc, restart="n")

            if batch_end in RESTART_B_HEIGHTS:
                self.log.info("Graceful restart NODE-B at height %s", batch_end)
                self._drain_validation_queue(node_b)
                pre_b = self._det_state(node_b)

                self.restart_node(1)
                self.connect_nodes(1, 0)
                self.sync_blocks(self.nodes, timeout=SYNC_TIMEOUT)
                self._sync_mocktime_to_tip(node_a, node_b, node_c)
                self._drain_validation_queue(node_a, node_b, node_c)

                post_b = self._det_state(node_b)
                if pre_b["state_hash"] != post_b["state_hash"]:
                    self._fail(
                        batch_end,
                        f"[RESTART_B] state_hash drift {pre_b['state_hash']} -> "
                        f"{post_b['state_hash']}",
                    )
                if pre_b["height"] != post_b["height"]:
                    self._fail(
                        batch_end,
                        f"[RESTART_B] height {pre_b['height']} -> {post_b['height']}",
                    )
                if pre_b["best_block_hash"] != post_b["best_block_hash"]:
                    self._fail(
                        batch_end,
                        "[RESTART_B] best_block_hash mismatch after restart",
                    )

                sa = self._det_state(node_a)
                sb = post_b
                sc = self._det_state(node_c)
                ra = self._replay_snap(node_a)
                rb = self._replay_snap(node_b)
                rc = self._replay_snap(node_c)
                self._assert_tri_equal(
                    f"POST_RESTART_B@{batch_end}",
                    batch_end,
                    sa,
                    sb,
                    sc,
                    ra,
                    rb,
                    rc,
                )
                self._log_line(
                    "POST_RESTART_B",
                    batch_end,
                    sa,
                    sb,
                    sc,
                    restart="y",
                )

        self.log.info("Deterministic log: %s", self._log_path())
        print("BASELINE-REPLAY-LONGRUN: PASS", flush=True)
        self.log.info("BASELINE-REPLAY-LONGRUN: PASS (%s blocks)", TARGET_BLOCKS)


if __name__ == "__main__":
    BaselineReplayLongrunTest().main()

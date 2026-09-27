#!/usr/bin/env python3
# Copyright (c) 2026 The RigorChain developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Replay burn-in ( long-run test matrix LR-03 / LR-04 / LR-09).

Long-running deterministic replay stress test. Configurable via CLI:

  --blocks N        target chain length (default 1500 for CI; 10000+ for nightly)
  --dbcache N       -dbcache value for all nodes (default 4 = force flushes)
  --sigkill-at LIST comma-separated heights at which to SIGKILL NODE-B and
                    verify post-restart state_hash equals pre-kill state_hash
                    (default empty = no kills)
  --checkpoint N    state_hash sample interval in blocks (default 100)
  --par N           optional -par=N on all nodes (LR-05 nightly permutation;
                    RIGOR may treat this as compatibility no-op, but the
                    harness still proves identical state_hash under the flag)

What this test verifies
-----------------------
* LR-03 / LR-04: state_hash determinism under aggressive flush regimes.
* LR-09: SIGKILL-then-restart at multiple heights keeps state_hash byte-equal.
* All assertions are byte-exact; no probabilistic verification (Constitution).

This is the same shape as ``feature_deterministic_replay.py`` but with:
  (a) configurable chain length up to 10000+,
  (b) configurable dbcache to exercise the BatchWrite path under pressure,
  (c) midstream SIGKILL of NODE-B (uncatchable; no shutdown hooks).
"""

from __future__ import annotations

import json
import os
import signal
import time
from decimal import Decimal

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal


SYNC_TIMEOUT = 240


def _parse_int_list(s):
    if not s:
        return []
    return [int(x) for x in s.split(",") if x.strip()]


class ReplayBurninTest(BitcoinTestFramework):
    def add_options(self, parser):
        parser.add_argument(
            "--blocks", type=int, default=1500,
            help="target chain length (CI default 1500; nightly 10000+)",
        )
        parser.add_argument(
            "--dbcache", type=int, default=4,
            help="-dbcache for all nodes; small values force mid-batch flush",
        )
        parser.add_argument(
            "--sigkill-at", type=str, default="",
            help="comma-separated heights at which to SIGKILL NODE-B",
        )
        parser.add_argument(
            "--checkpoint", type=int, default=100,
            help="state_hash sample interval in blocks",
        )
        parser.add_argument(
            "--par", type=int, default=None,
            help="if set, pass -par=N to every node (LR-05 matrix)",
        )

    def set_test_params(self):
        self.num_nodes = 3
        self.setup_clean_chain = True
        # devtest: canonical RIGOR functional-test chain (see RFC-000 §5
        # CONVERGENCE_DRAW_BIND_DEPTH = 10, FREEZE-03).
        self.chain = "devtest"
        # extra_args is finalized in setup() once options are parsed.
        self.extra_args = [[], [], []]

    def _base_node_args(self):
        """Extra args for every node; must match SIGKILL restart path."""
        out = [f"-dbcache={self.options.dbcache}"]
        if self.options.par is not None:
            out.append(f"-par={self.options.par}")
        return out

    def setup_nodes(self):
        args = self._base_node_args()
        self.extra_args = [list(args), list(args), list(args)]
        super().setup_nodes()

    def setup_network(self):
        """Override default chain topology with STAR topology centered on A.
        See feature_deterministic_replay.py for the rationale.
        """
        self.setup_nodes()
        self.connect_nodes(1, 0)
        self.connect_nodes(2, 0)
        self.sync_all()

    # ------------------------------------------------------------------
    # Helpers
    # ------------------------------------------------------------------
    def _drain(self, *nodes):
        for n in nodes:
            n.syncwithvalidationinterfacequeue()

    def _sync_mocktime_to_tip(self, *nodes):
        """Long burn-in mines faster than wall clock; align mock time with tip
        header nTime so CreateNewBlock does not fail with time-too-new."""
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

    def _assert_tri_equal(self, label, height, sa, sb, sc):
        assert_equal(sa["height"], height)
        assert_equal(sb["height"], height)
        assert_equal(sc["height"], height)
        assert sa["best_block_hash"] == sb["best_block_hash"] == sc["best_block_hash"], (
            f"[{label} h={height}] best_block_hash mismatch: "
            f"A={sa['best_block_hash']} B={sb['best_block_hash']} C={sc['best_block_hash']}"
        )
        assert sa["state_hash"] == sb["state_hash"] == sc["state_hash"], (
            f"[{label} h={height}] state_hash mismatch: "
            f"A={sa['state_hash']} B={sb['state_hash']} C={sc['state_hash']}"
        )

    def _log_path(self):
        return os.path.join(self.options.tmpdir, "replay_burnin.log")

    def _log_emit(self, tag, height, sa, sb, sc):
        with open(self._log_path(), "a", encoding="ascii") as f:
            f.write("\t".join([
                tag,
                f"h={height}",
                f"A={sa['state_hash']}",
                f"B={sb['state_hash']}",
                f"C={sc['state_hash']}",
            ]) + "\n")

    def _sigkill_node(self, idx):
        """Uncatchable kill of a node process. Bypasses all shutdown hooks
        so the test exercises the on-disk recovery path, not the graceful
        flush path.
        """
        n = self.nodes[idx]
        pid = n.process.pid
        self.log.info(f"SIGKILL NODE-{idx} (pid={pid})")
        os.kill(pid, signal.SIGKILL)
        n.process.wait(timeout=30)
        # Mark the node as stopped in framework state so restart_node works.
        n.running = False

    # ------------------------------------------------------------------
    # Main
    # ------------------------------------------------------------------
    def run_test(self):
        opts = self.options
        TARGET = opts.blocks
        STEP = opts.checkpoint
        KILL_HEIGHTS = set(_parse_int_list(opts.sigkill_at))

        self.log.info(
            f"BURN-IN params: blocks={TARGET} dbcache={opts.dbcache} "
            f"checkpoint={STEP} sigkill_at={sorted(KILL_HEIGHTS)} par={opts.par}"
        )

        with open(self._log_path(), "w", encoding="ascii") as f:
            f.write(
                f"# RIGOR replay burn-in log\n"
                f"# blocks={TARGET} dbcache={opts.dbcache} "
                f"checkpoint={STEP} sigkill_at={sorted(KILL_HEIGHTS)}"
                f" par={opts.par}\n"
                f"# NO timestamps. NO PIDs.\n"
            )

        node_a, node_b, node_c = self.nodes
        # Topology already established by setup_network(): A=hub, no B<->C edge.

        addr = node_a.get_deterministic_witness_mining_address()
        # Track kill checkpoints we've already serviced.
        killed = set()
        # State at last successful checkpoint (used to verify SIGKILL recovery).
        pre_kill_state_b = None

        current = 0
        while current < TARGET:
            next_stop = min(current + STEP, TARGET)
            # If a SIGKILL height lies in this batch, snap the batch boundary
            # at the kill height so we get a clean checkpoint sample first.
            kill_here = None
            for kh in sorted(KILL_HEIGHTS):
                if current < kh <= next_stop and kh not in killed:
                    next_stop = kh
                    kill_here = kh
                    break

            n_gen = next_stop - current
            self.generatetoaddress(node_a, n_gen, addr)
            self.sync_blocks(self.nodes, timeout=SYNC_TIMEOUT)
            self._sync_mocktime_to_tip(node_a, node_b, node_c)
            current = next_stop

            assert_equal(node_a.getblockcount(), current)
            assert_equal(node_b.getblockcount(), current)
            assert_equal(node_c.getblockcount(), current)

            self._drain(node_a, node_b, node_c)
            sa = self._det_state(node_a)
            sb = self._det_state(node_b)
            sc = self._det_state(node_c)
            self._assert_tri_equal("CHECKPOINT", current, sa, sb, sc)
            self._log_emit("CHECKPOINT", current, sa, sb, sc)
            self.log.info(
                f"[h={current}/{TARGET}] state_hash={sa['state_hash']}"
            )

            if kill_here is not None:
                pre_kill_state_b = sb
                self._sigkill_node(1)
                # Brief settle to ensure A/C do not block on dead B's peer slot.
                time.sleep(1)
                # Start (not restart) — restart would invoke RPC stop which
                # fails on an already-dead process. We deliberately exercise
                # the on-disk recovery path here.
                self.start_node(1, extra_args=self._base_node_args())
                self.connect_nodes(1, 0)
                self.sync_blocks(self.nodes, timeout=SYNC_TIMEOUT)
                self._drain(node_a, node_b, node_c)

                post_b = self._det_state(node_b)
                # SIGKILL recovery: state_hash MUST be byte-identical.
                assert post_b["state_hash"] == pre_kill_state_b["state_hash"], (
                    f"[SIGKILL h={kill_here}] NODE-B state_hash drift: "
                    f"{pre_kill_state_b['state_hash']} -> {post_b['state_hash']}"
                )
                assert_equal(post_b["height"], pre_kill_state_b["height"])
                # And tri-equality still holds.
                sa2 = self._det_state(node_a)
                sc2 = self._det_state(node_c)
                self._assert_tri_equal("POST_SIGKILL_B", kill_here, sa2, post_b, sc2)
                self._log_emit("POST_SIGKILL_B", kill_here, sa2, post_b, sc2)
                killed.add(kill_here)
                self.log.info(f"[SIGKILL h={kill_here}] PASS")
                self._sync_mocktime_to_tip(node_a, node_b, node_c)

        self._write_terminal_replay_artifacts(node_a)

        for n in (node_a, node_b, node_c):
            n.setmocktime(0)

        self.log.info("=" * 60)
        self.log.info(
            f"BURN-IN PASS: {TARGET} blocks @ dbcache={opts.dbcache}, "
            f"{len(killed)} SIGKILLs survived."
        )
        self.log.info(f"Log: {self._log_path()}")
        self.log.info("=" * 60)

    def _write_terminal_replay_artifacts(self, node_a):
 """Terminal RPL2 + JSON for archival ."""
        out_dir = self.options.tmpdir
        r = node_a.getreplaysnapshot()
        for key in (
            "height",
            "block_hash",
            "state_hash",
            "best_block_hash",
            "utxo_count",
            "serialized_snapshot",
        ):
            assert key in r, f"getreplaysnapshot missing required key {key!r}"

        def _default(o):
            if isinstance(o, Decimal):
                return str(o)
            raise TypeError(f"non-JSON-serializable {type(o).__name__}")

        snap_path = os.path.join(out_dir, "final_replaysnapshot_A.json")
        with open(snap_path, "w", encoding="ascii") as f:
            json.dump(r, f, indent=2, sort_keys=True, default=_default)

        hex_path = os.path.join(out_dir, "final_replaysnapshot_A.rpl2.hex")
        with open(hex_path, "w", encoding="ascii") as f:
            f.write(r["serialized_snapshot"] + "\n")

        st = node_a.getdeterministicstatehash()
        with open(os.path.join(out_dir, "final_state_hash_A.json"), "w", encoding="ascii") as f:
            json.dump(st, f, indent=2, sort_keys=True, default=_default)


if __name__ == "__main__":
    ReplayBurninTest().main()

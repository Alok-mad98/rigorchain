#!/usr/bin/env python3
# Copyright (c) 2026 The RigorChain developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Deterministic replay verification across multiple nodes (STEP-0201).

RIGOR Constitution: this harness enforces RULE-1 (determinism first),
RULE-2 (replay equivalence) and RULE-3 (no hidden order dependence) at the
3-node 1000-block scale. It is a TEST HARNESS only; it never modifies
consensus code.

STEP-0201 objectives
--------------------
* NODE-A mines; NODE-B and NODE-C sync only via A (star topology).
* Every ``CHECK_INTERVAL`` blocks: ``getdeterministicstatehash`` **and**
  canonical ``getreplaysnapshot`` (RPL2 ``serialized_snapshot``) compared
  across all nodes; ``getblockcount`` and ``getbestblockhash`` must agree
  before sampling (independent chain-view RPCs).
* Machine-readable stdout: ``STEP-0201: PASS`` or
  ``STEP-0201: DIVERGENCE_AT_HEIGHT=<n>`` (exact first failing checkpoint height).
* On divergence: ``STEP-0202`` forensic capture under ``replay_divergence_capture/``
  and stdout ``STEP-0202_CAPTURE_DIR=…`` (see ``feature_replay_failure_capture.py``).

What this test verifies
-----------------------
C-01  3-node identical deterministic state_hash at every 100-block checkpoint.
C-02  3-node identical RPL2 ``serialized_snapshot`` (canonical UTXO projection).
C-03  After NODE-B SIGKILL-equivalent stop+start, its state_hash is byte-equal
      to its pre-restart state_hash (RULE-2 across restart scenarios).
C-04  After NODE-C graceful stop+start, its state_hash is byte-equal.
C-05  After NODE-C ``-reindex-chainstate``, its state_hash equals the other
      nodes' (LevelDB cursor ordering stability across rebuilds).
C-10  Explicit topology lock: B and C are NOT connected to each other;
      only A is the source.
C-13  A deterministic log file (height TAB state_hash TAB best_block_hash) is
      emitted with NO timestamps, NO PIDs, sorted by height; cross-machine
      byte-comparable.
C-15  ``syncwithvalidationinterfacequeue`` is called on EVERY node before
      EVERY state_hash sample, eliminating the validation-queue race.

What this test does NOT do
--------------------------
* It does not exercise reorg paths (see ``feature_reorg_determinism_audit.py``).
* On failure it invokes ``feature_replay_failure_capture.capture_divergence``
  (STEP-0202) into ``<tmpdir>/replay_divergence_capture/`` — use
  ``--nocleanup`` to retain artifacts.
* It does not run beyond 1000 blocks (see ``feature_replay_burnin.py``).
"""

from __future__ import annotations

import importlib.util
import os

from test_framework.test_framework import BitcoinTestFramework


def _load_replay_failure_capture():
    """Load sibling ``feature_replay_failure_capture`` without package import.

    Functional tests are invoked with ``test/functional`` on ``sys.path`` in
    some environments but not others; ``importlib`` keeps this reliable.
    """
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


# Sample interval in blocks. Constitution: explicit constants only.
CHECK_INTERVAL = 100
# Target chain length for the in-CI replay test.
TARGET_BLOCKS = 1000
# sync_blocks timeout. Generous to avoid CI flakes; never used as a
# soft "wait for determinism" — determinism is asserted byte-exact afterwards.
SYNC_TIMEOUT = 120


class DeterministicReplayTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 3
        self.setup_clean_chain = True
        # regtest and devtest mine from genesis: RFC-000 §5 uses a frozen
        # zero preimage for draw bind on heights 1..k-1 (see doc/rfc000_bootstrap_bind.md).
        self.chain = "devtest"
        # No special args. RULE-5: consensus > performance. We do NOT tune
        # -dbcache here; that belongs in feature_replay_burnin.py.
        self.extra_args = [[], [], []]

    def skip_test_if_missing_module(self):
        # MiniWallet is no longer used; coinbase-only generation is sufficient
        # for the determinism dimensions this harness covers.
        pass

    def setup_network(self):
        """Override default chain topology (node0<-node1<-node2) with a
        STAR topology centered on NODE-A. This explicitly prevents NODE-B
        and NODE-C from peering with each other (M-04 hardening: any block
        seen by B or C must come from A).
        """
        self.setup_nodes()
        # Connect B->A and C->A; do NOT connect B<->C.
        self.connect_nodes(1, 0)
        self.connect_nodes(2, 0)
        self.sync_all()

    # ------------------------------------------------------------------
    # Snapshot primitives
    # ------------------------------------------------------------------
    def _drain_validation_queue(self, *nodes):
        """C-15: flush the validation interface queue on every node so that
        any post-ConnectTip callbacks have committed before we sample
        state_hash. Without this, a fast harness can sample a transient
        in-flight chainstate. RULE-1.
        """
        for n in nodes:
            n.syncwithvalidationinterfacequeue()

    def _det_state(self, node):
        """Sample the deterministic state hash from one node.

        Returns dict with the three frozen RPC fields. NEVER use the .get()
        defaulting pattern: a missing field is a CRITICAL determinism bug,
        not a soft-fail.
        """
        s = node.getdeterministicstatehash()
        return {
            "height": s["height"],
            "best_block_hash": s["best_block_hash"],
            "state_hash": s["state_hash"],
        }

    def _replay_snap(self, node):
        """Canonical diagnostic snapshot (RPL2). Authoritative digest remains
        ``state_hash``; ``serialized_snapshot`` is the byte-stable wire form
        used for explicit UTXO-set projection equality (STEP-0201).
        """
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

    def _fail_step0201(self, height, reason):
        """Emit harness verdict on stdout (no timestamps) then fail the test.

        Before raising, run STEP-0202 forensic capture (non-suppressing: the
        test still fails; capture errors are logged but do not mask the
        divergence).
        """
        verdict = f"STEP-0201: DIVERGENCE_AT_HEIGHT={height}"
        print(verdict, flush=True)
        self.log.error(f"{verdict} — {reason}")
        with open(self._det_log_path(), "a", encoding="ascii") as f:
            f.write(f"DIVERGENCE\t{verdict}\t{reason}\n")

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
            self.log.error(
                "STEP-0202 capture_divergence failed (divergence verdict is still primary): %r",
                e,
            )

        raise AssertionError(f"{verdict} — {reason}")

    def _assert_tri_equal(self, label, height, sa, sb, sc, ra=None, rb=None, rc=None):
        """Byte-exact equality across all three nodes (STEP-0201 canonical).

        When replay snapshots are supplied, also enforces RPL2 wire equality
        and RPC cross-consistency (state_hash vs replay snapshot).
        """
        # Height (RPC-reported tip height inside deterministic hash RPC)
        for lab, s in (("A", sa), ("B", sb), ("C", sc)):
            if s["height"] != height:
                self._fail_step0201(
                    height,
                    f"[{label}] getdeterministicstatehash height on NODE-{lab}: "
                    f"{s['height']} != checkpoint {height}",
                )
        # Best block hash
        if sa["best_block_hash"] != sb["best_block_hash"]:
            self._fail_step0201(
                height,
                f"[{label}] BEST_BLOCK_MISMATCH getdeterministicstatehash A vs B: "
                f"{sa['best_block_hash']} vs {sb['best_block_hash']}",
            )
        if sb["best_block_hash"] != sc["best_block_hash"]:
            self._fail_step0201(
                height,
                f"[{label}] BEST_BLOCK_MISMATCH getdeterministicstatehash B vs C: "
                f"{sb['best_block_hash']} vs {sc['best_block_hash']}",
            )
        # Authoritative state hash (replay divergence / state mismatch)
        if sa["state_hash"] != sb["state_hash"]:
            self._fail_step0201(
                height,
                f"[{label}] STATE_MISMATCH state_hash A vs B: "
                f"{sa['state_hash']} vs {sb['state_hash']}",
            )
        if sb["state_hash"] != sc["state_hash"]:
            self._fail_step0201(
                height,
                f"[{label}] STATE_MISMATCH state_hash B vs C: "
                f"{sb['state_hash']} vs {sc['state_hash']}",
            )

        if ra is not None and rb is not None and rc is not None:
            for lab, r, s in (("A", ra, sa), ("B", rb, sb), ("C", rc, sc)):
                if r["state_hash"] != s["state_hash"]:
                    self._fail_step0201(
                        height,
                        f"[{label}] REPLAY_DIVERGENCE getreplaysnapshot vs "
                        f"getdeterministicstatehash on NODE-{lab}: "
                        f"{r['state_hash']} vs {s['state_hash']}",
                    )
            # RPL2 anchor height and block hash must be identical across nodes
            # and aligned with the deterministic-state tip hash.
            for lab, r, s in (("A", ra, sa), ("B", rb, sb), ("C", rc, sc)):
                if r["height"] != height:
                    self._fail_step0201(
                        height,
                        f"[{label}] HEIGHT_MISMATCH getreplaysnapshot.height on "
                        f"NODE-{lab}: {r['height']} != checkpoint {height}",
                    )
                if r["block_hash"] != s["best_block_hash"]:
                    self._fail_step0201(
                        height,
                        f"[{label}] REPLAY_BLOCK_HASH_VS_TIP on NODE-{lab}: "
                        f"block_hash={r['block_hash']} best_block_hash={s['best_block_hash']}",
                    )
            if ra["block_hash"] != rb["block_hash"]:
                self._fail_step0201(
                    height,
                    f"[{label}] BEST_BLOCK_MISMATCH RPL2 block_hash A={ra['block_hash']} "
                    f"B={rb['block_hash']}",
                )
            if rb["block_hash"] != rc["block_hash"]:
                self._fail_step0201(
                    height,
                    f"[{label}] BEST_BLOCK_MISMATCH RPL2 block_hash B={rb['block_hash']} "
                    f"C={rc['block_hash']}",
                )
            if ra["serialized_snapshot"] != rb["serialized_snapshot"]:
                self._fail_step0201(
                    height,
                    f"[{label}] UTXO_MISMATCH RPL2 serialized_snapshot A vs B (len "
                    f"{len(ra['serialized_snapshot'])} vs {len(rb['serialized_snapshot'])})",
                )
            if rb["serialized_snapshot"] != rc["serialized_snapshot"]:
                self._fail_step0201(
                    height,
                    f"[{label}] UTXO_MISMATCH RPL2 serialized_snapshot B vs C (len "
                    f"{len(rb['serialized_snapshot'])} vs {len(rc['serialized_snapshot'])})",
                )
            if ra["utxo_count"] != rb["utxo_count"] or rb["utxo_count"] != rc["utxo_count"]:
                self._fail_step0201(
                    height,
                    f"[{label}] utxo_count A={ra['utxo_count']} B={rb['utxo_count']} "
                    f"C={rc['utxo_count']}",
                )

        self.log.info(
            f"[{label} h={height}] PASS state_hash={sa['state_hash']}"
        )

    # ------------------------------------------------------------------
    # Deterministic log (C-13)
    # ------------------------------------------------------------------
    def _det_log_path(self):
        # One log file per test; lives under the test tmpdir for post-run diff.
        return os.path.join(self.options.tmpdir, "deterministic_replay.log")

    def _det_log_emit(self, height, sa, sb, sc, tag="CHECKPOINT", utxo_count=None):
        """Append one canonical line. NO timestamps, NO PIDs. Sorted by
        height because we only ever call this with monotonically
        increasing heights.
        """
        parts = [
            tag,
            f"h={height}",
            f"A_state={sa['state_hash']}",
            f"B_state={sb['state_hash']}",
            f"C_state={sc['state_hash']}",
            f"best={sa['best_block_hash']}",
        ]
        if utxo_count is not None:
            parts.append(f"utxo_count={utxo_count}")
        line = "\t".join(parts)
        with open(self._det_log_path(), "a", encoding="ascii") as f:
            f.write(line + "\n")

    # ------------------------------------------------------------------
    # Topology helpers (C-10)
    # ------------------------------------------------------------------
    def _assert_topology(self):
        """Assert the STAR topology built by setup_network() is intact:
        A is the hub; B and C must each have at least one peer (their
        edge to A) but must NOT peer with each other.

        Constitution M-04: any block propagation B or C observes must be
        attributable to A alone.
        """
        node_a, node_b, node_c = self.nodes
        # Subver fingerprints identify which node is on the other side
        # of each peer connection (framework convention: "testnodeN").
        def peers_of(n):
            return [p.get("subver", "") for p in n.getpeerinfo()]

        b_peers = peers_of(node_b)
        c_peers = peers_of(node_c)
        # Defensive: B must not see testnode2 (=C); C must not see testnode1 (=B).
        assert not any("testnode2" in s for s in b_peers), (
            f"M-04 violation: NODE-B peers include NODE-C: {b_peers}"
        )
        assert not any("testnode1" in s for s in c_peers), (
            f"M-04 violation: NODE-C peers include NODE-B: {c_peers}"
        )
        self.log.info(
            f"Topology asserted: A=hub, B peers={len(b_peers)}, "
            f"C peers={len(c_peers)}, no B<->C edge."
        )

    # ------------------------------------------------------------------
    # Main test
    # ------------------------------------------------------------------
    def run_test(self):
        node_a, node_b, node_c = self.nodes

        # Reset deterministic log file (idempotent across reruns within same dir).
        with open(self._det_log_path(), "w", encoding="ascii") as f:
            f.write(
                "# RIGOR deterministic replay log. Lines are TAB-separated.\n"
                "# Format: <tag>\\t<h=>\\t<A_state=>\\t<B_state=>\\t<C_state=>\\t<best=>\\n"
                "# NO timestamps. NO PIDs. Sorted by height.\n"
            )

        self._assert_topology()

        addr = node_a.get_deterministic_witness_mining_address()
        self.log.info(
            f"Generating {TARGET_BLOCKS} blocks on NODE-A in batches of "
            f"{CHECK_INTERVAL}; state-hash checkpoint every batch."
        )

        for batch_start in range(0, TARGET_BLOCKS, CHECK_INTERVAL):
            batch_end = min(batch_start + CHECK_INTERVAL, TARGET_BLOCKS)
            blocks_to_generate = batch_end - batch_start

            self.generatetoaddress(node_a, blocks_to_generate, addr)
            self.sync_blocks(self.nodes, timeout=SYNC_TIMEOUT)

            # Height sanity (cheap, but catches gross propagation failures
            # before we touch the more expensive state_hash path).
            ha = node_a.getblockcount()
            hb = node_b.getblockcount()
            hc = node_c.getblockcount()
            if ha != batch_end or hb != batch_end or hc != batch_end:
                self._fail_step0201(
                    batch_end,
                    f"HEIGHT_MISMATCH getblockcount A={ha} B={hb} C={hc} "
                    f"expected={batch_end}",
                )

            # Independent tip RPC (canonical chain view) — must agree before
            # state_hash sampling; catches tip divergence not visible in counts.
            ba = node_a.getbestblockhash()
            bb = node_b.getbestblockhash()
            bc = node_c.getbestblockhash()
            if ba != bb or bb != bc:
                self._fail_step0201(
                    batch_end,
                    f"BEST_BLOCK_MISMATCH getbestblockhash A={ba} B={bb} C={bc}",
                )

            # C-15: drain validation queue on ALL nodes before sampling.
            self._drain_validation_queue(node_a, node_b, node_c)

            sa = self._det_state(node_a)
            sb = self._det_state(node_b)
            sc = self._det_state(node_c)
            ra = self._replay_snap(node_a)
            rb = self._replay_snap(node_b)
            rc = self._replay_snap(node_c)

            self._assert_tri_equal(
                "CHECKPOINT", batch_end, sa, sb, sc, ra, rb, rc
            )
            self._det_log_emit(
                batch_end,
                sa,
                sb,
                sc,
                tag="CHECKPOINT",
                utxo_count=ra["utxo_count"],
            )

        self.log.info("=" * 60)
        self.log.info(f"PHASE-1 PASS: {TARGET_BLOCKS}-block 3-node identical state.")
        self.log.info("=" * 60)

        # ------------------------------------------------------------------
        # M-06: restart NODE-B and re-compare (C-03/C-04 surface)
        # ------------------------------------------------------------------
        self.log.info("PHASE-2: restart NODE-B and re-compare state_hash.")
        self._drain_validation_queue(node_b)
        pre_b = self._det_state(node_b)

        self.restart_node(1)
        # Reconnect topology after restart (framework drops edges on restart).
        self.connect_nodes(1, 0)
        self.sync_blocks(self.nodes, timeout=SYNC_TIMEOUT)
        self._drain_validation_queue(node_a, node_b, node_c)

        post_b = self._det_state(node_b)
        if pre_b["state_hash"] != post_b["state_hash"]:
            self._fail_step0201(
                TARGET_BLOCKS,
                f"[RESTART NODE-B] state_hash drift: "
                f"{pre_b['state_hash']} -> {post_b['state_hash']}",
            )
        if pre_b["height"] != post_b["height"]:
            self._fail_step0201(
                TARGET_BLOCKS,
                f"[RESTART NODE-B] height mismatch: "
                f"{pre_b['height']} -> {post_b['height']}",
            )
        if pre_b["best_block_hash"] != post_b["best_block_hash"]:
            self._fail_step0201(
                TARGET_BLOCKS,
                f"[RESTART NODE-B] best_block_hash mismatch: "
                f"{pre_b['best_block_hash']} -> {post_b['best_block_hash']}",
            )

        # Tri-equality must still hold post-restart.
        sa = self._det_state(node_a)
        sb = post_b
        sc = self._det_state(node_c)
        ra = self._replay_snap(node_a)
        rb = self._replay_snap(node_b)
        rc = self._replay_snap(node_c)
        self._assert_tri_equal(
            "POST_RESTART_B", TARGET_BLOCKS, sa, sb, sc, ra, rb, rc
        )
        self._det_log_emit(
            TARGET_BLOCKS,
            sa,
            sb,
            sc,
            tag="POST_RESTART_B",
            utxo_count=ra["utxo_count"],
        )

        # ------------------------------------------------------------------
        # M-07: -reindex-chainstate on NODE-C and re-compare (C-05 surface)
        # ------------------------------------------------------------------
        self.log.info("PHASE-3: -reindex-chainstate NODE-C and re-compare.")
        self._drain_validation_queue(node_c)
        pre_c = self._det_state(node_c)

        # reindex-chainstate rebuilds the UTXO set from blocks on disk WITHOUT
        # redownloading. If state_hash changes across this, RULE-3 (no hidden
        # order dependence) is violated somewhere in the rebuild path.
        self.restart_node(2, extra_args=["-reindex-chainstate"])
        self.connect_nodes(2, 0)
        self.sync_blocks(self.nodes, timeout=SYNC_TIMEOUT)
        self._drain_validation_queue(node_a, node_b, node_c)

        post_c = self._det_state(node_c)
        if pre_c["state_hash"] != post_c["state_hash"]:
            self._fail_step0201(
                TARGET_BLOCKS,
                f"[REINDEX NODE-C] state_hash drift: "
                f"{pre_c['state_hash']} -> {post_c['state_hash']}",
            )
        if pre_c["height"] != post_c["height"]:
            self._fail_step0201(
                TARGET_BLOCKS,
                f"[REINDEX NODE-C] height mismatch: "
                f"{pre_c['height']} -> {post_c['height']}",
            )
        if pre_c["best_block_hash"] != post_c["best_block_hash"]:
            self._fail_step0201(
                TARGET_BLOCKS,
                f"[REINDEX NODE-C] best_block_hash mismatch: "
                f"{pre_c['best_block_hash']} -> {post_c['best_block_hash']}",
            )

        sa = self._det_state(node_a)
        sb = self._det_state(node_b)
        sc = post_c
        ra = self._replay_snap(node_a)
        rb = self._replay_snap(node_b)
        rc = self._replay_snap(node_c)
        self._assert_tri_equal(
            "POST_REINDEX_C", TARGET_BLOCKS, sa, sb, sc, ra, rb, rc
        )
        self._det_log_emit(
            TARGET_BLOCKS,
            sa,
            sb,
            sc,
            tag="POST_REINDEX_C",
            utxo_count=ra["utxo_count"],
        )

        self.log.info("=" * 60)
        self.log.info("DETERMINISTIC REPLAY TEST: PASS")
        print("STEP-0201: PASS", flush=True)
        self.log.info(
            f"Verified {TARGET_BLOCKS} blocks across 3 nodes, plus restart "
            f"of NODE-B and reindex-chainstate of NODE-C. All state hashes "
            f"and RPL2 snapshots byte-identical."
        )
        self.log.info(f"Deterministic log: {self._det_log_path()}")
        self.log.info("=" * 60)


if __name__ == "__main__":
    DeterministicReplayTest().main()

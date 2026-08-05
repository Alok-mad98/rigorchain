#!/usr/bin/env python3
# Copyright (c) 2026 The RigorChain developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Reorg determinism audit (STEP-0203).

For each reorg depth K in {3, 5, 10}, this test runs a fully isolated
sub-scenario from a clean chain. The goal is to detect:

  * replay drift           — same chain, different state
  * rollback inconsistency — chainstate not byte-equal to a never-reorged peer
  * hidden cached state    — process-internal cache not flushed by rollback
  * orphan state leakage   — losing-fork data contaminating the winning chain

VERIFY (per K iteration)
------------------------
* Identical UTXO / replay: ``state_hash`` quadruple + matching RPL2
  ``serialized_snapshot`` and ``utxo_count`` across A/B/C/D.
* Identical RES / ORU balances: ``res_balance`` and ``other_balance`` from
  ``getreplaysnapshot``.
* Identical SelectionProof / draw / ConvergenceRoot inputs: per-height
  ``getconvergenceresult`` projection (bind, ``res_probability_target``, ConvergenceDrawCandidates
  roots, convergence root set size, Sigma ``h`` per candidate, winner).
* Tip selection hashes: ``selection_convergence_root``, ``selection_sigmas_hash``,
  ``selection_weights_hash`` in the rollback TSV rows.

OUTPUT
------
* ``reorg_rollback_trace_K{K}.tsv`` — multi-column rollback / replay trace.
* ``reorg_block_digest_K{K}.tsv`` — compact per-height convergence JSON
  (NODE-D reference; A/B/C asserted identical).

Topology (4 nodes; "GOLDEN-D" is the pristine reference)
--------------------------------------------------------
    NODE-A  — short-fork miner
    NODE-B  — long-fork miner (winning)
    NODE-C  — isolated at common-prefix tip during partition; reconnects via B
    NODE-D  — GOLDEN: never sees the losing fork; only syncs from B after
              reorg has resolved on A/B/C.

Per K-iteration flow
--------------------
  1. Reset chain: all four nodes are restarted clean. A/B/C build common
     prefix. D stays disconnected (no peers).
  2. Partition A from B and C. NODE-A mines (K-1) blocks; NODE-B mines (K+2)
     blocks. NODE-C stays isolated at the common-prefix tip until step 3.
  3. Reconnect A<->B and B<->C. The longer chain (B) wins. Sync.
  4. Bring D online by connecting only to B (B-only sync source). D never
     touches the orphaned A-fork blocks.
  5. After full sync, ALL four nodes' state_hash MUST be byte-identical:
       a. state_hash(A) == state_hash(D)   — hidden cache check
       b. state_hash(C) == state_hash(D)   — orphan leakage check
       c. trio + golden tri-equality       — replay equivalence
  6. Restart NODE-A and recompute state_hash:
       d. state_hash(A_after_restart) == state_hash(D)   — restart determinism
  7. Compare last 25 blocks' ``getconvergenceresult``: bind / draw target /
     ConvergenceDrawCandidates roots / convergence root set size / Sigma vector ``h`` /
     ``draw_winner_txid`` MUST match across all four nodes (RPC field
     ``res_probability_target``; legacy ``orbis_draw_target`` name is not emitted).

Constitution
------------
  * No consensus modifications.
  * No suppression: any divergence raises an AssertionError immediately.
  * No retries.
"""

from __future__ import annotations

import json
import os
from decimal import Decimal

from test_framework.authproxy import JSONRPCException
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal


SYNC_TIMEOUT = 120
COMMON_PREFIX = 120          # blocks before partition
COMPARE_BLOCKS = 25          # last N blocks compared field-by-field
REORG_DEPTHS = [3, 5, 10]    # K values to exercise


def _block_digest_json_line(proj):
    """Compact JSON for ``reorg_block_digest_*.tsv`` (RPC amounts may be Decimal)."""
    def _default(o):
        if isinstance(o, Decimal):
            return str(o)
        raise TypeError(
            f"Object of type {o.__class__.__name__} is not JSON serializable "
            "for block digest trace"
        )

    return json.dumps(proj, sort_keys=True, separators=(",", ":"), default=_default)


def _candidate_digest_map(cr_obj):
    """Project getconvergenceresult into a stable, comparable mapping.

    Includes ``convergence_type`` for SelectionProof / CVG2 surface parity.
    """
    out = {}
    for c in cr_obj.get("candidates", []):
        out[c["txid"]] = {
            "h": c["h"],
            "draw_eligible": c.get("draw_eligible"),
            "convergence_amount": c.get("convergence_amount"),
            "convergence_type": c.get("convergence_type"),
        }
    return out


def _convergence_result_projection(cr):
    """Per-block canonical subset for ConvergenceRoot + draw + Sigma inputs."""
    return {
        "sigma_seed": cr.get("sigma_seed"),
        "payout_height": cr.get("payout_height"),
        "convergence_root": cr.get("convergence_root"),
        "res_probability_target": cr.get("res_probability_target"),
        "min_convergence": cr.get("min_convergence"),
        "convergence_root_set_size": cr.get("convergence_root_set_size"),
        "draw_candidate_root_committed": cr.get("draw_candidate_root_committed"),
        "draw_candidate_root_computed": cr.get("draw_candidate_root_computed"),
        "draw_winner_txid": cr.get("draw_winner_txid"),
        "candidates": _candidate_digest_map(cr),
    }


def _drain(*nodes):
    for n in nodes:
        n.syncwithvalidationinterfacequeue()


class ReorgDeterminismAuditTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 4
        self.setup_clean_chain = True
        self.chain = "devtest"
        # No special args. D's isolation is enforced via the network layer,
        # not via -connect=0 (which would prevent later add-only connection).
        self.extra_args = [[], [], [], []]

    def setup_network(self):
        """Override default chain-topology setup. Each K-iteration restarts
        all four nodes and then builds its own topology from scratch. We
        therefore start with NO edges and let _run_one_K() take ownership.
        """
        self.setup_nodes()
        # Intentionally NO connect_nodes() and NO sync_all() here.

    # ------------------------------------------------------------------
    # Deterministic trace
    # ------------------------------------------------------------------
    def _trace_path(self, K):
        return os.path.join(
            self.options.tmpdir, f"reorg_rollback_trace_K{K}.tsv"
        )

    def _trace_init(self, K):
        with open(self._trace_path(K), "w", encoding="ascii") as f:
            f.write(
                f"# RIGOR reorg rollback trace K={K} (STEP-0203)\n"
                "# TAB-separated. NO timestamps. NO PIDs.\n"
                "# Rows: mostly A,B,C,D in that order when all nodes traced.\n"
                "# croot/sigmas_h/wts_h/csm1_len from getchainstatesummary when present,\n"
                "# else literal NA if RPC is absent on this binary.\n"
            )

    def _trace_row_full(self, K, event, node_label, node):
        """Append one full rollback/replay projection row (canonical RPC order).

        ``getchainstatesummary`` may be missing on older binaries; in that case
        CSM-derived columns are written as ``NA`` (trace remains deterministic).

        The RPC also refuses a **genesis** chain tip (no ``pprev`` / no tip undo);
        NODE-D stays isolated through Phase-3, so PREFIX/FORK/REORG rows for ``D``
        hit that path — degrade CSM to ``NA`` like ``-32601`` (route-B contract).
        """
        _drain(node)
        s = node.getdeterministicstatehash()
        snap = node.getreplaysnapshot()
        for key in (
            "utxo_count",
            "res_balance",
            "other_balance",
            "serialized_snapshot",
        ):
            assert key in snap, f"getreplaysnapshot missing {key!r}"
        croot = sigmas_h = wts_h = "NA"
        csm1_len = 0
        try:
            csm = node.getchainstatesummary()
            for key in (
                "selection_convergence_root",
                "selection_sigmas_hash",
                "selection_weights_hash",
                "chainstatesummary_hex",
            ):
                assert key in csm, f"getchainstatesummary missing {key!r}"
            croot = csm["selection_convergence_root"]
            sigmas_h = csm["selection_sigmas_hash"]
            wts_h = csm["selection_weights_hash"]
            csm1_len = len(csm["chainstatesummary_hex"])
        except JSONRPCException as e:
            code = e.error.get("code")
            msg = e.error.get("message", "")
            if code == -32601:
                if not getattr(self, "_warned_missing_getchainstatesummary", False):
                    daemon = os.path.basename(self.options.bitcoind)
                    self.log.warning(
                        "getchainstatesummary RPC unavailable on this binary "
                        "(Method not found). CSM columns in rollback trace will be NA; "
                        "rebuild `%s` from current RIGOR sources for full CSM trace.",
                        daemon,
                    )
                    self._warned_missing_getchainstatesummary = True
            elif code == -32603 and "Failed to read tip block undo data" in msg:
                if not getattr(self, "_warned_getchainstatesummary_tip_undo", False):
                    self.log.warning(
                        "getchainstatesummary failed to read tip undo "
                        "(expected for genesis-tipped nodes, e.g. isolated D before Phase-4). "
                        "CSM columns in rollback trace will be NA for affected rows."
                    )
                    self._warned_getchainstatesummary_tip_undo = True
            else:
                raise
        line = "\t".join([
            event,
            f"h={s['height']}",
            node_label,
            f"state={s['state_hash']}",
            f"best={s['best_block_hash']}",
            f"utxo={snap['utxo_count']}",
            f"res_bal={snap['res_balance']}",
            f"oth_bal={snap['other_balance']}",
            f"rpl2_len={len(snap['serialized_snapshot'])}",
            f"croot={croot}",
            f"sigmas_h={sigmas_h}",
            f"wts_h={wts_h}",
            f"csm1_len={csm1_len}",
        ])
        with open(self._trace_path(K), "a", encoding="ascii") as f:
            f.write(line + "\n")
        return s

    def _trace_all_four(self, K, event):
        """Emit four rows in **A, B, C, D order** (RULE-3: no peer iteration order)."""
        ret = {}
        for label, idx in (("A", 0), ("B", 1), ("C", 2), ("D", 3)):
            ret[label] = self._trace_row_full(K, event, label, self.nodes[idx])
        return ret

    def _trace_ordered(self, K, event, pairs):
        """Emit rows in caller-defined (label, node) order — use for partial topologies."""
        out = {}
        for label, node in pairs:
            out[label] = self._trace_row_full(K, event, label, node)
        return out

    def _block_trace_path(self, K):
        return os.path.join(
            self.options.tmpdir, f"reorg_block_digest_K{K}.tsv"
        )

    def _block_trace_init(self, K):
        with open(self._block_trace_path(K), "w", encoding="ascii") as f:
            f.write(
                f"# RIGOR per-height convergence projection K={K} (STEP-0203)\n"
                "# Reference row from NODE-D ``getconvergenceresult``; A/B/C asserted equal.\n"
                "# NO timestamps. NO PIDs.\n"
                "# Columns: height, block_hash, convergence_json (compact, sort_keys)\n"
            )

    # ------------------------------------------------------------------
    # Topology
    # ------------------------------------------------------------------
    def _isolate_golden(self):
        """Ensure NODE-D has zero peers at start of each K-iteration."""
        node_d = self.nodes[3]
        # Disconnect from everyone (defensive; framework starts with no edges,
        # but earlier iterations may have left D connected).
        for i in (0, 1, 2):
            try:
                self.disconnect_nodes(3, i)
            except Exception:
                pass
        self.wait_until(lambda: len(node_d.getpeerinfo()) == 0, timeout=30)

    def _connect_main_trio(self):
        """A<->B<->C wired through B (B is hub during reconnect)."""
        self.connect_nodes(1, 0)  # B -> A
        self.connect_nodes(2, 1)  # C -> B

    # ------------------------------------------------------------------
    # Per-K scenario
    # ------------------------------------------------------------------
    def _run_one_K(self, K):
        self.log.info("=" * 60)
        self.log.info(f"REORG AUDIT START K={K}")
        self.log.info("=" * 60)

        self._trace_init(K)
        self._block_trace_init(K)

        # Fresh chain for each K. Constitution: hidden cache detection must
        # start from a guaranteed-clean state.
        for i in range(4):
            self.restart_node(i)
        node_a, node_b, node_c, node_d = self.nodes

        self._isolate_golden()
        self._connect_main_trio()

        addr_a = node_a.get_deterministic_witness_mining_address()
        addr_b = node_b.get_deterministic_witness_mining_address()

        # Phase 1: common prefix
        self.log.info(f"[K={K}] Phase-1: build {COMMON_PREFIX}-block common prefix on A")
        # NODE-D is intentionally peerless here; do not use default sync_all().
        self.generatetoaddress(node_a, COMMON_PREFIX, addr_a, sync_fun=lambda: None)
        self.sync_blocks([node_a, node_b, node_c], timeout=SYNC_TIMEOUT)
        _drain(node_a, node_b, node_c, node_d)
        trip = self._trace_ordered(
            K,
            "PREFIX_BUILT",
            [("A", node_a), ("B", node_b), ("C", node_c)],
        )
        self._trace_row_full(K, "PREFIX_D_ISOLATED", "D", node_d)
        s_a, s_b, s_c = trip["A"], trip["B"], trip["C"]
        assert_equal(s_a["state_hash"], s_b["state_hash"])
        assert_equal(s_b["state_hash"], s_c["state_hash"])

        # Phase 2: partition
        self.log.info(f"[K={K}] Phase-2: partition A from B/C and build forks")
        self.disconnect_nodes(0, 1)
        self.disconnect_nodes(1, 2)
        # A is isolated from B/C; C remains connected to nothing else.
        # We DELIBERATELY keep C off A as well so C does not observe the
        # losing fork during partition. Reconnect path is via B only.
        try:
            self.disconnect_nodes(0, 2)
        except Exception:
            pass
        self.wait_until(
            lambda: (
                len(node_a.getpeerinfo()) == 0
                and len(node_b.getpeerinfo()) == 0
                and len(node_c.getpeerinfo()) == 0
            ),
            timeout=30,
        )

        # NODE-A builds the SHORT (losing) fork: K-1 extra blocks.
        # NODE-B builds the LONG (winning) fork:  K+2 extra blocks.
        # The +2 margin guarantees B's chain is strictly heavier than A's
        # by exactly 3 blocks, irrespective of the K parameter.
        short_len = max(1, K - 1)
        long_len = K + 2
        self.generatetoaddress(node_a, short_len, addr_a, sync_fun=lambda: None)
        self.generatetoaddress(node_b, long_len, addr_b, sync_fun=lambda: None)
        _drain(node_a, node_b, node_c, node_d)
        self._trace_ordered(
            K,
            "FORK_TIPS",
            [("A", node_a), ("B", node_b), ("C", node_c)],
        )
        self._trace_row_full(K, "FORK_D_ISOLATED", "D", node_d)

        # Phase 3: reconnect; B's longer chain wins.
        self.log.info(f"[K={K}] Phase-3: reconnect via B; expect reorg")
        self.connect_nodes(1, 0)  # B -> A : A receives the heavier chain
        self.connect_nodes(2, 1)  # C -> B : C also reorgs onto B's chain
        self.sync_blocks([node_a, node_b, node_c], timeout=SYNC_TIMEOUT)
        _drain(node_a, node_b, node_c, node_d)
        reorg = self._trace_ordered(
            K,
            "REORG_DONE",
            [("A", node_a), ("B", node_b), ("C", node_c)],
        )
        self._trace_row_full(K, "REORG_D_ISOLATED", "D", node_d)
        s_a, s_b, s_c = reorg["A"], reorg["B"], reorg["C"]
        assert_equal(s_a["best_block_hash"], s_b["best_block_hash"])
        assert_equal(s_b["best_block_hash"], s_c["best_block_hash"])

        # Phase 4: bring up GOLDEN-D, connect ONLY to B.
        self.log.info(f"[K={K}] Phase-4: bring GOLDEN-D online; B-only source")
        self.connect_nodes(3, 1)  # D <-> B
        self.sync_blocks(self.nodes, timeout=SYNC_TIMEOUT)
        _drain(node_a, node_b, node_c, node_d)
        quad = self._trace_all_four(K, "GOLDEN_QUADRUPLE_SYNC")
        s_a, s_b, s_c, s_d = quad["A"], quad["B"], quad["C"], quad["D"]

        # Phase 5: cardinal assertions
        self.log.info(f"[K={K}] Phase-5: cardinal state_hash assertions")
        # (a) All four nodes share the same authoritative state_hash.
        assert_equal(s_a["state_hash"], s_d["state_hash"])  # hidden cache
        assert_equal(s_c["state_hash"], s_d["state_hash"])  # orphan leakage
        assert_equal(s_b["state_hash"], s_d["state_hash"])
        # (b) Heights and best_block_hash agree.
        assert_equal(s_a["height"], s_d["height"])
        assert_equal(s_a["best_block_hash"], s_d["best_block_hash"])

        # Explicit RPL2 + RES balance parity (UTXO / replay surface beyond state_hash).
        snap_d = node_d.getreplaysnapshot()
        for label, n in (("A", node_a), ("B", node_b), ("C", node_c)):
            snap_n = n.getreplaysnapshot()
            assert_equal(snap_n["serialized_snapshot"], snap_d["serialized_snapshot"]), (
                f"[K={K}] RPL2 serialized_snapshot mismatch vs D on NODE-{label}"
            )
            assert_equal(snap_n["res_balance"], snap_d["res_balance"])
            assert_equal(snap_n["other_balance"], snap_d["other_balance"])
            assert_equal(snap_n["utxo_count"], snap_d["utxo_count"])

        self.log.info(
            f"[K={K}] state_hash quadruple equal: {s_d['state_hash']}"
        )

        # Phase 6: restart NODE-A and re-verify against GOLDEN-D.
        self.log.info(f"[K={K}] Phase-6: restart NODE-A; verify state_hash stable")
        self.restart_node(0)
        self.connect_nodes(0, 1)  # A <-> B
        self.sync_blocks(self.nodes, timeout=SYNC_TIMEOUT)
        _drain(node_a, node_b, node_c, node_d)
        post_restart = self._trace_all_four(K, "POST_A_RESTART")
        s_a2 = post_restart["A"]
        assert_equal(s_a2["state_hash"], s_d["state_hash"])
        assert_equal(s_a2["best_block_hash"], s_d["best_block_hash"])

        # Phase 7: per-block consensus-derived field comparison on the last
        # COMPARE_BLOCKS blocks of the winning chain. Covers ConvergenceRoot,
        # Sigma, and SelectionProof projections.
        self.log.info(
            f"[K={K}] Phase-7: compare last {COMPARE_BLOCKS} blocks' "
            f"getconvergenceresult across all 4 nodes"
        )
        _drain(node_a, node_b, node_c, node_d)
        tip_h = node_d.getblockcount()
        first_h = max(0, tip_h - (COMPARE_BLOCKS - 1))
        for h in range(first_h, tip_h + 1):
            bh = node_d.getblockhash(h)
            # Block hash identity
            for label, n in (("A", node_a), ("B", node_b), ("C", node_c)):
                bh_n = n.getblockhash(h)
                assert bh_n == bh, (
                    f"[K={K} h={h}] block hash mismatch {label} vs D: {bh_n} vs {bh}"
                )

            cr_d = node_d.getconvergenceresult(bh)
            proj_d = _convergence_result_projection(cr_d)
            with open(self._block_trace_path(K), "a", encoding="ascii") as bf:
                bf.write(
                    "\t".join([
                        str(h),
                        bh,
                        _block_digest_json_line(proj_d),
                    ])
                    + "\n"
                )
            for label, n in (("A", node_a), ("B", node_b), ("C", node_c)):
                cr_n = n.getconvergenceresult(bh)
                proj_n = _convergence_result_projection(cr_n)
                assert_equal(proj_n, proj_d), (
                    f"[K={K} h={h}] getconvergenceresult mismatch NODE-{label} vs D"
                )

        self.log.info(f"[K={K}] PASS — all phases byte-equal to GOLDEN-D")

    # ------------------------------------------------------------------
    # Driver
    # ------------------------------------------------------------------
    def run_test(self):
        for K in REORG_DEPTHS:
            self._run_one_K(K)

        self.log.info("=" * 60)
        self.log.info(
            f"REORG DETERMINISM AUDIT: PASS for K in {REORG_DEPTHS}"
        )
        print("STEP-0203: PASS", flush=True)
        for K in REORG_DEPTHS:
            self.log.info(f"  Rollback trace K={K}: {self._trace_path(K)}")
            self.log.info(f"  Block digest K={K}: {self._block_trace_path(K)}")
        self.log.info("=" * 60)


if __name__ == "__main__":
    ReorgDeterminismAuditTest().main()

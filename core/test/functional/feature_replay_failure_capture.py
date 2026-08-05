#!/usr/bin/env python3
# Copyright (c) 2026 The RigorChain developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Replay failure capture system (STEP-0202).

When deterministic replay divergence is detected, the harness MUST produce
a complete forensic record. This file both:

  1. Implements the capture pipeline (callable from other harnesses).
  2. Exercises the pipeline against an injected divergence so the
     diagnostic path itself is regression-tested.

What is captured on divergence (STEP-0202 / H-phase D-01..D-08)
---------------------------------------------------------------
* Divergence height (caller supplies, typically from
  ``find_first_divergence_height`` = first block-hash mismatch).
* ``first_mismatch.json`` — canonical per-node tip state + block hash at
  divergence height (``sort_keys=True`` JSON), plus **pairwise**
  ``pairwise_first_divergence`` heights for every node pair (lexicographic
  keys), and optional ``harness_checkpoint_height`` / ``harness_failure_reason``
  when invoked from STEP-0201.
* Block hash at divergence and at parent height (per node); full block JSON
  verbosity=2.
* Deterministic ``state_hash`` via ``getdeterministicstatehash`` (per node).
* Full ReplaySnapshot (RPL2) via ``getreplaysnapshot`` JSON **and**
  ``{label}_replaysnap_rpl2.hex`` (one-line hex wire image).
* Mint projection file ``{label}_mint_state.json`` (RES/ORU counters from
  replay snapshot; diagnostic only).
* Selection + CSM1 wire via ``getchainstatesummary`` →
  ``{label}_chainstatesummary.json`` plus ``{label}_selection.json`` subset.
* Full canonical UTXO set dump via ``dumptxoutset`` (per node).
* Deterministic ``trace.tsv``: NO timestamps, NO PIDs; rows sorted by label.

What this test DOES NOT do
--------------------------
* It does not modify consensus. The 'divergence' is constructed by network
  partition and is the expected outcome of two non-communicating miners.
* It does not auto-correct, retry, or suppress mismatches (Constitution).
"""

from __future__ import annotations

import itertools
import json
import os
from decimal import Decimal

from test_framework.authproxy import JSONRPCException
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal


SYNC_TIMEOUT = 60


def _write_json_capture(path, obj):
    """Serialize RPC-derived objects deterministically (``sort_keys=True``).

    ``python-bitcoinrpc`` may leave monetary fields as ``Decimal``; stdlib
    ``json`` cannot encode those without a default hook.
    """
    def _default(o):
        if isinstance(o, Decimal):
            return str(o)
        raise TypeError(
            f"Object of type {o.__class__.__name__} is not JSON serializable "
            f"for capture file {path}"
        )

    with open(path, "w", encoding="ascii") as f:
        json.dump(obj, f, indent=2, sort_keys=True, default=_default)


# =====================================================================
# Public capture API (reusable from other harnesses)
# =====================================================================

def _drain(*nodes):
    for n in nodes:
        n.syncwithvalidationinterfacequeue()


def find_first_divergence_height(node_a, node_b, max_height):
    """Linear backtrack to find the FIRST height at which the two nodes'
    block hashes disagree.

    Returns the lowest height h such that
      ``node_a.getblockhash(h) != node_b.getblockhash(h)``,
    or ``None`` if the two chains agree on every height in
    ``[0, max_height]``.

    Rationale (H-phase D-02): binary search is not safe here because the
    two chains may have differing depths; linear backtrack from the tip
    is O(max_height) RPC calls but is unambiguous and deterministic.
    """
    if max_height < 0:
        return None
    # Walk forward from genesis; stop at the first disagreement.
    # genesis MUST match (regtest hardcoded), but we still check it for paranoia.
    for h in range(0, max_height + 1):
        try:
            bh_a = node_a.getblockhash(h)
        except JSONRPCException:
            return h  # one side doesn't have this height yet
        try:
            bh_b = node_b.getblockhash(h)
        except JSONRPCException:
            return h
        if bh_a != bh_b:
            return h
    return None


def pairwise_first_divergence_heights(label_to_node, max_height):
    """For every unordered pair of nodes, first height where ``getblockhash``
    disagrees (same semantics as ``find_first_divergence_height``).

    Returns a dict with keys ``"{L1}_vs_{L2}"`` for ``L1 < L2`` lexicographically,
    values are ``int`` heights or ``null`` if the two chains match on
    ``[0, max_height]``.
    """
    out = {}
    labels = sorted(label_to_node.keys())
    for a, b in itertools.combinations(labels, 2):
        key = f"{a}_vs_{b}"
        na, nb = label_to_node[a], label_to_node[b]
        out[key] = find_first_divergence_height(na, nb, max_height)
    return out


def _write_first_mismatch_manifest(
    out_dir,
    label_to_node,
    divergence_height,
    *,
    extra_top_level=None,
):
    """Exact first-mismatch context at capture time (canonical JSON).

    Nodes are enumerated in **sorted label order** (RULE-3: no dict order
    dependence in serialized manifests).
    """
    nodes_data = {}
    for label in sorted(label_to_node.keys()):
        node = label_to_node[label]
        state = node.getdeterministicstatehash()
        block_at_h = None
        if divergence_height is not None and divergence_height >= 0:
            try:
                block_at_h = node.getblockhash(divergence_height)
            except Exception:
                block_at_h = "UNAVAILABLE"
        nodes_data[label] = {
            "best_block_hash_at_tip": state["best_block_hash"],
            "block_hash_at_divergence_height": block_at_h,
            "height_at_tip_from_state_rpc": state["height"],
            "state_hash_at_tip": state["state_hash"],
            "tip_block_count": node.getblockcount(),
        }
    manifest = {
        "divergence_height": divergence_height,
        "nodes": nodes_data,
    }
    if extra_top_level:
        for k in sorted(extra_top_level.keys()):
            manifest[k] = extra_top_level[k]
    path = os.path.join(out_dir, "first_mismatch.json")
    with open(path, "w", encoding="ascii") as f:
        json.dump(manifest, f, indent=2, sort_keys=True)
    return path


def _subset_selection_fields(chainstatesummary):
    """Selection-only projection for STEP-0202 requirement #5."""
    if not isinstance(chainstatesummary, dict) or "error" in chainstatesummary:
        return chainstatesummary
    keys = sorted(
        k for k in chainstatesummary.keys() if k.startswith("selection_")
    )
    out = {k: chainstatesummary[k] for k in keys}
    out["block_hash"] = chainstatesummary.get("block_hash")
    out["height"] = chainstatesummary.get("height")
    return out


def _mint_projection_from_replaysnap(snap):
    """Mint / classifier projection for STEP-0202 requirement #6 (RPL2-derived)."""
    if not isinstance(snap, dict) or "error" in snap:
        return snap
    keys = (
        "converge_count",
        "lock_count",
        "mint_count",
        "other_balance",
        "res_balance",
        "utxo_count",
    )
    return {k: snap[k] for k in keys if k in snap}


def capture_divergence(
    log,
    out_dir,
    label_to_node,
    divergence_height,
    *,
    harness_checkpoint_height=None,
    harness_failure_reason=None,
):
    """Capture the full forensic bundle for a divergence event.

    Parameters
    ----------
    log : logger
        Test logger. Capture writes WARN+ lines describing each artifact.
    out_dir : str
        Existing directory to receive ``{label}_*`` files.
    label_to_node : dict[str, AuthServiceProxy]
        e.g. ``{"A": node_a, "B": node_b}``. Iteration is always **sorted by
        label** for deterministic outputs (RULE-3).
    divergence_height : int
        Height to snapshot. May be ``None`` if no divergence (then a
        "no_divergence" marker file is written and the call is a no-op).
    harness_checkpoint_height : int | None
        Optional gate height from the calling harness (e.g. STEP-0201 batch end).
    harness_failure_reason : str | None
        Optional verbatim failure reason string for the manifest (ASCII only
        recommended).

    Side effects
    ------------
    Writes the following files under ``out_dir`` per node (labels sorted):
      * ``first_mismatch.json``       — per-node tip vs divergence-height hashes
      * ``{label}_state.json``        — getdeterministicstatehash
      * ``{label}_replaysnap.json``   — getreplaysnapshot
      * ``{label}_replaysnap_rpl2.hex`` — serialized_snapshot one line (canonical)
      * ``{label}_chainstatesummary.json`` — getchainstatesummary (CSM1 + selection)
      * ``{label}_selection.json``    — selection_* subset only
      * ``{label}_mint_state.json``   — mint classifier projection (from RPL2 RPC)
      * ``{label}_block_h.json``      — getblock(divergence_hash, 2)
      * ``{label}_block_parent.json`` — getblock(parent_hash, 2)
      * ``{label}_utxo.dat``          — dumptxoutset
      * ``trace.tsv``                 — deterministic trace (shared)

    Returns
    -------
    dict
        ``{"divergence_height": h, "files": [...]}``.
    """
    os.makedirs(out_dir, exist_ok=True)

    if divergence_height is None:
        marker = os.path.join(out_dir, "no_divergence")
        with open(marker, "w", encoding="ascii") as f:
            f.write("OK\n")
        return {"divergence_height": None, "files": [marker]}

    _drain(*label_to_node.values())

    files = []
    trace_lines = []

    max_h = min(n.getblockcount() for n in label_to_node.values())
    pair_heights = pairwise_first_divergence_heights(label_to_node, max_h)
    extra_manifest = {"pairwise_first_divergence": dict(sorted(pair_heights.items()))}
    if harness_checkpoint_height is not None:
        extra_manifest["harness_checkpoint_height"] = harness_checkpoint_height
    if harness_failure_reason is not None:
        extra_manifest["harness_failure_reason"] = harness_failure_reason

    mm_path = _write_first_mismatch_manifest(
        out_dir,
        label_to_node,
        divergence_height,
        extra_top_level=extra_manifest,
    )
    files.append(mm_path)

    for label in sorted(label_to_node.keys()):
        node = label_to_node[label]
        try:
            block_hash = node.getblockhash(divergence_height)
        except Exception as e:
            log.error(f"[{label}] no block at height {divergence_height}: {e}")
            block_hash = None

        parent_hash = None
        if divergence_height > 0:
            try:
                parent_hash = node.getblockhash(divergence_height - 1)
            except Exception:
                parent_hash = None

        # state_hash (authoritative)
        state = node.getdeterministicstatehash()
        path = os.path.join(out_dir, f"{label}_state.json")
        _write_json_capture(path, state)
        files.append(path)

        # Replay snapshot (RPL2 wire format + projection counters)
        try:
            snap = node.getreplaysnapshot()
        except Exception as e:
            log.error(f"[{label}] getreplaysnapshot failed: {e}")
            snap = {"error": str(e)}
        path = os.path.join(out_dir, f"{label}_replaysnap.json")
        _write_json_capture(path, snap)
        files.append(path)

        # RPL2 wire image as raw one-line hex (canonical for byte diffs / external tools).
        if isinstance(snap, dict) and "serialized_snapshot" in snap and "error" not in snap:
            hex_path = os.path.join(out_dir, f"{label}_replaysnap_rpl2.hex")
            with open(hex_path, "w", encoding="ascii") as f:
                f.write(snap["serialized_snapshot"] + "\n")
            files.append(hex_path)

        path = os.path.join(out_dir, f"{label}_mint_state.json")
        _write_json_capture(path, _mint_projection_from_replaysnap(snap))
        files.append(path)

        # Chainstate summary (CSM1 wire + explicit selection fields at tip).
        try:
            csm = node.getchainstatesummary()
        except Exception as e:
            log.error(f"[{label}] getchainstatesummary failed: {e}")
            csm = {"error": str(e)}
        path = os.path.join(out_dir, f"{label}_chainstatesummary.json")
        _write_json_capture(path, csm)
        files.append(path)

        path = os.path.join(out_dir, f"{label}_selection.json")
        _write_json_capture(path, _subset_selection_fields(csm))
        files.append(path)

        # Block at divergence height (verbosity=2)
        if block_hash is not None:
            try:
                blk = node.getblock(block_hash, 2)
            except Exception as e:
                blk = {"error": str(e)}
            path = os.path.join(out_dir, f"{label}_block_h.json")
            _write_json_capture(path, blk)
            files.append(path)

        # Parent block
        if parent_hash is not None:
            try:
                pblk = node.getblock(parent_hash, 2)
            except Exception as e:
                pblk = {"error": str(e)}
            path = os.path.join(out_dir, f"{label}_block_parent.json")
            _write_json_capture(path, pblk)
            files.append(path)

        # Full UTXO set dump via dumptxoutset (canonical, muhash-committed)
        utxo_path = os.path.join(out_dir, f"{label}_utxo.dat")
        try:
            node.dumptxoutset(utxo_path)
            files.append(utxo_path)
        except Exception as e:
            log.error(f"[{label}] dumptxoutset failed: {e}")
            err_path = utxo_path + ".error"
            with open(err_path, "w", encoding="ascii") as f:
                f.write(str(e) + "\n")
            files.append(err_path)

        # Trace line (deterministic; rows sorted by label below).
        csm_hex_len = 0
        if isinstance(csm, dict) and "chainstatesummary_hex" in csm:
            csm_hex_len = len(csm["chainstatesummary_hex"])
        trace_lines.append("\t".join([
            label,
            f"h={divergence_height}",
            f"block={block_hash or 'NONE'}",
            f"state={state['state_hash']}",
            f"best={state['best_block_hash']}",
            f"csm1_hex_len={csm_hex_len}",
        ]))

    # Trace file is written sorted by label for cross-run byte-equality.
    trace_path = os.path.join(out_dir, "trace.tsv")
    trace_lines.sort()
    with open(trace_path, "w", encoding="ascii") as f:
        f.write(
            "# RIGOR replay trace (STEP-0202). TAB-separated. Sorted by label.\n"
            "# Columns: label, h=, block=, state=, best=, csm1_hex_len=\n"
            "# NO timestamps. NO PIDs.\n"
        )
        for line in trace_lines:
            f.write(line + "\n")
    files.append(trace_path)

    log.error(f"Divergence at height {divergence_height}: captured {len(files)} artifacts in {out_dir}")
    return {"divergence_height": divergence_height, "files": files}


# =====================================================================
# Self-test: inject divergence via partition and verify capture runs
# =====================================================================

class ReplayFailureCaptureTest(BitcoinTestFramework):
    """Inject a divergence by network-partitioning two miners, then
    exercise the full capture path and assert all required artifacts are
    produced.

    Two miners cannot agree on a tip if they each mine independently in
    isolation. We deliberately do NOT reconnect them; the test PASSES iff
    the capture pipeline correctly identifies the first divergence height
    and writes every required artifact. The capture itself never tries to
    'fix' the divergence (Constitution: no auto-correct, no suppress).
    """

    def set_test_params(self):
        self.num_nodes = 2
        self.setup_clean_chain = True
        # regtest/devtest: mining from genesis is valid after RFC-000 §5 bootstrap bind
        # (see doc/rfc000_bootstrap_bind.md). devtest remains the canonical RIGOR FT chain.
        self.chain = "devtest"
        self.extra_args = [[], []]

    def run_test(self):
        node_a, node_b = self.nodes

        # Step 1: build a 50-block common prefix on A and sync to B.
        # NOTE: framework setup_network() has already connected node0<->node1;
        # do NOT re-connect here, which would create a duplicate edge and
        # confuse the wait_until handshake counter.
        addr_a = node_a.get_deterministic_witness_mining_address()
        self.generatetoaddress(node_a, 50, addr_a)
        self.sync_blocks(self.nodes, timeout=SYNC_TIMEOUT)
        _drain(node_a, node_b)

        pre_state_a = node_a.getdeterministicstatehash()
        pre_state_b = node_b.getdeterministicstatehash()
        assert_equal(pre_state_a["state_hash"], pre_state_b["state_hash"])
        self.log.info(f"Common prefix h=50 state_hash={pre_state_a['state_hash']}")

        # Step 2: partition and mine independently to force divergence at h=51.
        self.disconnect_nodes(0, 1)
        self.wait_until(
            lambda: len(node_a.getpeerinfo()) == 0 and len(node_b.getpeerinfo()) == 0,
            timeout=30,
        )

        addr_b = node_b.get_deterministic_witness_mining_address()
        # Each side mines 3 blocks. Because mining addresses differ AND the
        # miners are partitioned, the chains diverge starting at height 51.
        self.generatetoaddress(node_a, 3, addr_a, sync_fun=lambda: None)
        self.generatetoaddress(node_b, 3, addr_b, sync_fun=lambda: None)
        _drain(node_a, node_b)

        # Step 3: verify state_hash now differs.
        post_a = node_a.getdeterministicstatehash()
        post_b = node_b.getdeterministicstatehash()
        assert post_a["state_hash"] != post_b["state_hash"], (
            "Partition did not produce divergence; cannot exercise capture path"
        )
        self.log.info(
            f"Injected divergence: A_state={post_a['state_hash']} "
            f"B_state={post_b['state_hash']}"
        )

        # Step 4: find first divergence height via the backtrack helper.
        max_h = min(node_a.getblockcount(), node_b.getblockcount())
        first_h = find_first_divergence_height(node_a, node_b, max_h)
        assert first_h == 51, (
            f"Expected first divergence at height 51, got {first_h}"
        )
        self.log.info(f"First divergence height detected: {first_h}")

        # Step 5: exercise full capture pipeline.
        out_dir = os.path.join(self.options.tmpdir, "capture_test")
        result = capture_divergence(
            self.log,
            out_dir,
            {"A": node_a, "B": node_b},
            divergence_height=first_h,
        )
        assert_equal(result["divergence_height"], 51)

        # Step 6: assert every required artifact exists on disk.
        required = [
            "first_mismatch.json",
            "A_state.json", "B_state.json",
            "A_replaysnap.json", "B_replaysnap.json",
            "A_replaysnap_rpl2.hex", "B_replaysnap_rpl2.hex",
            "A_mint_state.json", "B_mint_state.json",
            "A_chainstatesummary.json", "B_chainstatesummary.json",
            "A_selection.json", "B_selection.json",
            "A_block_h.json", "B_block_h.json",
            "A_block_parent.json", "B_block_parent.json",
            "A_utxo.dat", "B_utxo.dat",
            "trace.tsv",
        ]
        for name in required:
            path = os.path.join(out_dir, name)
            assert os.path.exists(path), f"Missing required capture artifact: {path}"
            assert os.path.getsize(path) > 0, f"Empty capture artifact: {path}"
            self.log.info(f"Artifact OK: {name} ({os.path.getsize(path)} bytes)")

        with open(os.path.join(out_dir, "first_mismatch.json")) as f:
            mm = json.load(f)
        assert mm["pairwise_first_divergence"]["A_vs_B"] == 51

        # Step 7: assert replaysnap counters disagree between A and B
        # (sanity: the diagnostic-only fields actually surface the divergence).
        with open(os.path.join(out_dir, "A_replaysnap.json")) as f:
            snap_a = json.load(f)
        with open(os.path.join(out_dir, "B_replaysnap.json")) as f:
            snap_b = json.load(f)
        # state_hash MUST differ (already asserted in Step 3 via different RPC,
        # but cross-check the captured artifacts themselves).
        assert snap_a["state_hash"] != snap_b["state_hash"], (
            "Captured replaysnaps have identical state_hash despite divergence"
        )

        # Step 8: assert trace.tsv is byte-deterministic (sorted by label,
        # no timestamps). Recompute and compare.
        with open(os.path.join(out_dir, "trace.tsv"), "rb") as f:
            content_1 = f.read()
        # Re-capture to a second directory and compare.
        out_dir_2 = os.path.join(self.options.tmpdir, "capture_test_rerun")
        capture_divergence(
            self.log,
            out_dir_2,
            {"A": node_a, "B": node_b},
            divergence_height=first_h,
        )
        with open(os.path.join(out_dir_2, "trace.tsv"), "rb") as f:
            content_2 = f.read()
        assert content_1 == content_2, (
            "trace.tsv is NOT byte-deterministic across reruns"
        )

        self.log.info("=" * 60)
        self.log.info("REPLAY FAILURE CAPTURE: PASS")
        print("STEP-0202: PASS", flush=True)
        self.log.info(f"All {len(required)} required artifacts captured in {out_dir}")
        self.log.info("trace.tsv verified byte-deterministic across reruns")
        self.log.info("=" * 60)


if __name__ == "__main__":
    ReplayFailureCaptureTest().main()

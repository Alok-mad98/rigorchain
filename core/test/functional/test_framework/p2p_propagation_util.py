# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Shared helpers for PHASE-B2 P2P propagation functional tests."""

from __future__ import annotations

import time
from typing import Callable, List, Sequence

from test_framework.util import assert_equal


def assert_tips_equal(nodes: Sequence, label: str = "") -> None:
    tips = [n.getbestblockhash() for n in nodes]
    prefix = f"{label}: " if label else ""
    for i, t in enumerate(tips[1:], start=1):
        if tips[0] != t:
            raise AssertionError(
                f"{prefix}tip mismatch node0={tips[0]} node{i}={t}"
            )


def assert_peer_counts_stable(
    nodes: Sequence,
    *,
    min_peers: int = 1,
    label: str = "",
) -> None:
    for i, n in enumerate(nodes):
        pc = len(n.getpeerinfo())
        assert pc >= min_peers, (
            f"{label}node {i}: expected >={min_peers} peers, got {pc}"
        )


def wait_block_height(node, height: int, *, timeout: float = 120, test_framework=None) -> None:
    if test_framework is not None:
        test_framework.wait_until(
            lambda: node.getblockcount() >= height,
            timeout=timeout,
        )
    else:
        deadline = time.time() + timeout
        while time.time() < deadline:
            if node.getblockcount() >= height:
                return
            time.sleep(0.05)
        raise AssertionError(f"height {height} not reached on node (tip {node.getblockcount()})")


def measure_propagation_latency(
    miner_node,
    observer_nodes: Sequence,
    block_hash: str,
    *,
    timeout: float = 60,
) -> List[float]:
    """Return per-observer seconds until ``getblockheader(hash)`` succeeds."""
    t0 = time.time()
    latencies: List[float] = []
    for obs in observer_nodes:
        deadline = t0 + timeout
        while time.time() < deadline:
            try:
                obs.getblockheader(block_hash)
                latencies.append(time.time() - t0)
                break
            except Exception:
                time.sleep(0.02)
        else:
            raise AssertionError(
                f"block {block_hash} not observed within {timeout}s"
            )
    return latencies


def sample_network_metrics(node) -> dict:
    """RPC snapshot for propagation / stability logs."""
    mi = node.getmempoolinfo()
    peers = node.getpeerinfo()
    ch = node.getblockchaininfo()
    return {
        "height": ch["blocks"],
        "tip": ch["bestblockhash"],
        "mempool_txs": mi.get("size", 0),
        "mempool_bytes": mi.get("bytes", 0),
        "minrelaytxfee": mi.get("minrelaytxfee", 0),
        "peer_count": len(peers),
        "peers_in": sum(1 for p in peers if p.get("inbound")),
        "peers_out": sum(1 for p in peers if not p.get("inbound")),
    }

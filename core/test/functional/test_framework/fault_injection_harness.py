#!/usr/bin/env python3
# Copyright (c) 2026 The RigorChain developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Fault injection helpers for multi-node deterministic consensus tests.

This module ONLY records outcomes and compares fingerprints — it does not
modify consensus, auto-correct failures, or suppress divergences.

RULE-3: serialized artifacts use sorted keys / sorted row order where
dicts or cross-node sets appear.
"""

from __future__ import annotations

import json
import os
import re
from decimal import Decimal
from typing import Any, Callable, Dict, List, Mapping, Optional, Sequence, Tuple, Union

from test_framework.authproxy import JSONRPCException


def _eq_or_fail(a, b, msg: str) -> None:
    """Two-value equality with message (Bitcoin ``assert_equal`` has no message slot)."""
    if a != b:
        raise AssertionError(f"{msg}: {a!r} != {b!r}")


def drain(*nodes) -> None:
    """Flush validation interface callbacks before sampling chainstate."""
    for n in nodes:
        n.syncwithvalidationinterfacequeue()


def consensus_fingerprint(node) -> Dict[str, Union[int, str]]:
    """Authoritative per-node consensus + RPL2 wire digest (post-drain)."""
    drain(node)
    st = node.getdeterministicstatehash()
    rp = node.getreplaysnapshot()
    return {
        "height": int(st["height"]),
        "best_block_hash": st["best_block_hash"],
        "state_hash": st["state_hash"],
        "serialized_snapshot": rp["serialized_snapshot"],
        "utxo_count": int(rp["utxo_count"]),
        "mint_count": int(rp["mint_count"]),
    }


def assert_fingerprints_equal(
    fp_a: Mapping[str, Any],
    fp_b: Mapping[str, Any],
    fp_c: Mapping[str, Any],
    ctx: str,
) -> None:
    """Hard-fail on any mismatch (no repair)."""
    _eq_or_fail(fp_a["height"], fp_b["height"], f"{ctx} height A vs B")
    _eq_or_fail(fp_b["height"], fp_c["height"], f"{ctx} height B vs C")
    _eq_or_fail(fp_a["best_block_hash"], fp_b["best_block_hash"], f"{ctx} best A vs B")
    _eq_or_fail(fp_b["best_block_hash"], fp_c["best_block_hash"], f"{ctx} best B vs C")
    _eq_or_fail(fp_a["state_hash"], fp_b["state_hash"], f"{ctx} state_hash A vs B")
    _eq_or_fail(fp_b["state_hash"], fp_c["state_hash"], f"{ctx} state_hash B vs C")
    _eq_or_fail(
        fp_a["serialized_snapshot"],
        fp_b["serialized_snapshot"],
        f"{ctx} RPL2 A vs B",
    )
    _eq_or_fail(
        fp_b["serialized_snapshot"],
        fp_c["serialized_snapshot"],
        f"{ctx} RPL2 B vs C",
    )


def _json_canonical(obj: Any) -> str:
    """Stable JSON for logging (Decimals → str)."""

    def _default(o: Any) -> Any:
        if isinstance(o, Decimal):
            return str(o)
        raise TypeError(f"not JSON serializable: {type(o)!r}")

    return json.dumps(obj, sort_keys=True, separators=(",", ":"), default=_default)


def write_fault_artifact(base_dir: str, fault_id: str, case_id: str, payload: Any) -> str:
    """Write one deterministic JSON artifact; returns path."""
    os.makedirs(base_dir, exist_ok=True)
    safe_case = re.sub(r"[^a-zA-Z0-9_.-]+", "_", case_id)
    path = os.path.join(base_dir, f"{fault_id}__{safe_case}.json")
    with open(path, "w", encoding="ascii") as f:
        f.write(_json_canonical(payload))
        f.write("\n")
    return path


def append_fault_trace(trace_path: str, row: Sequence[str]) -> None:
    """Append one TAB-separated row (caller sorts full file if needed)."""
    d = os.path.dirname(trace_path)
    if d:
        os.makedirs(d, exist_ok=True)
    with open(trace_path, "a", encoding="ascii") as f:
        f.write("\t".join(row) + "\n")


def finalize_sorted_trace(trace_path: str, header_lines: Sequence[str]) -> None:
    """Rewrite trace with header then body rows sorted (deterministic)."""
    if not os.path.isfile(trace_path):
        return
    with open(trace_path, "r", encoding="ascii") as f:
        body = f.readlines()
    rows = [ln.rstrip("\n") for ln in body if ln.strip()]
    rows.sort()
    with open(trace_path, "w", encoding="ascii") as f:
        for h in header_lines:
            f.write(h + "\n")
        for r in rows:
            f.write(r + "\n")


def rpc_error_tuple(exc: BaseException) -> Tuple[str, int, str]:
    """Normalize JSON-RPC errors for cross-node comparison."""
    if isinstance(exc, JSONRPCException):
        e = exc.error
        return ("jsonrpc", int(e["code"]), str(e["message"]))
    return ("exception", -1, f"{type(exc).__name__}: {exc}")


def normalize_decode_error_message(msg: str) -> str:
    """Strip variable-length hex blobs from decode error text if present."""
    # sendrawtransaction: "TX decode failed. Make sure the tx has at least one input."
    # testmempoolaccept: "TX decode failed: <hex> Make sure..."
    if "TX decode failed" not in msg:
        return msg
    msg = re.sub(r"TX decode failed: [0-9a-fA-F]*\s*", "TX decode failed: ", msg)
    return msg


def testmempoolaccept_result(node, hex_tx: str, *, maxfeerate: Decimal = Decimal("0")) -> Dict[str, Any]:
    """Single-tx testmempoolaccept; never swallows node differences."""
    r = node.testmempoolaccept(rawtxs=[hex_tx], maxfeerate=maxfeerate)
    assert len(r) == 1
    return r[0]


def assert_mempool_reject_shape(
    verdict: Mapping[str, Any],
    ctx: str,
    *,
    expect_reject_exact: Optional[str] = None,
    expect_reject_prefix: Optional[str] = None,
    expect_reject_one_of: Optional[Tuple[str, ...]] = None,
    expect_reject_starts_with_any: Optional[Tuple[str, ...]] = None,
) -> None:
    """Require explicit rejection shape (never a bare ``allowed is False``)."""
    assert "allowed" in verdict, f"{ctx}: missing allowed field: {verdict!r}"
    assert verdict["allowed"] is False, f"{ctx}: expected rejection, got allowed={verdict['allowed']!r}"
    rr = verdict.get("reject-reason")
    assert isinstance(rr, str) and len(rr) > 0, f"{ctx}: missing non-empty reject-reason: {verdict!r}"
    if expect_reject_exact is not None:
        _eq_or_fail(rr, expect_reject_exact, ctx)
    if expect_reject_prefix is not None:
        assert rr.startswith(expect_reject_prefix), (
            f"{ctx}: reject-reason {rr!r} does not start with {expect_reject_prefix!r}"
        )
    if expect_reject_one_of is not None:
        assert rr in expect_reject_one_of, (
            f"{ctx}: reject-reason {rr!r} not in frozen alternates {expect_reject_one_of!r}"
        )
    if expect_reject_starts_with_any is not None:
        assert any(rr.startswith(p) for p in expect_reject_starts_with_any), (
            f"{ctx}: reject-reason {rr!r} does not start with any of {expect_reject_starts_with_any!r}"
        )


def assert_identical_mempool_verdict(
    nodes: Sequence[Any],
    labels: Sequence[str],
    hex_tx: str,
    *,
    maxfeerate: Decimal = Decimal("0"),
    ctx: str,
    expect_reject_exact: Optional[str] = None,
    expect_reject_prefix: Optional[str] = None,
    expect_reject_one_of: Optional[Tuple[str, ...]] = None,
    expect_reject_starts_with_any: Optional[Tuple[str, ...]] = None,
) -> Dict[str, Any]:
    """All nodes must return the same allowed / reject-reason projection."""
    assert len(nodes) == len(labels)
    first = testmempoolaccept_result(nodes[0], hex_tx, maxfeerate=maxfeerate)
    proj0 = _mempool_verdict_projection(first)
    for n, lab in zip(nodes[1:], labels[1:]):
        ri = testmempoolaccept_result(n, hex_tx, maxfeerate=maxfeerate)
        pi = _mempool_verdict_projection(ri)
        _eq_or_fail(
            _json_canonical(proj0),
            _json_canonical(pi),
            f"{ctx} mempool verdict {labels[0]} vs {lab}",
        )
    if proj0.get("allowed") is False:
        assert any(
            x is not None
            for x in (
                expect_reject_exact,
                expect_reject_prefix,
                expect_reject_one_of,
                expect_reject_starts_with_any,
            )
        ), (
            f"{ctx}: rejected transaction must specify an explicit expected reject shape "
            f"(exact / prefix / one_of / starts_with_any); see fault_injection_expect.py"
        )
        assert_mempool_reject_shape(
            proj0,
            ctx,
            expect_reject_exact=expect_reject_exact,
            expect_reject_prefix=expect_reject_prefix,
            expect_reject_one_of=expect_reject_one_of,
            expect_reject_starts_with_any=expect_reject_starts_with_any,
        )
    return {"reference_label": labels[0], "verdict": proj0, "raw_first": first}


def _mempool_verdict_projection(tma: Dict[str, Any]) -> Dict[str, Any]:
    out: Dict[str, Any] = {}
    if "allowed" in tma:
        out["allowed"] = tma["allowed"]
    if "reject-reason" in tma:
        out["reject-reason"] = tma["reject-reason"]
    if "package-error" in tma:
        out["package-error"] = tma["package-error"]
    out["txid"] = tma.get("txid")
    out["wtxid"] = tma.get("wtxid")
    return out


def assert_identical_rpc_errors(
    errors: Sequence[BaseException],
    labels: Sequence[str],
    ctx: str,
    *,
    message_must_contain: Optional[str] = None,
) -> Tuple[str, int, str]:
    """After parallel RPC failure injection, compare normalized error tuples."""
    assert len(errors) == len(labels)
    norms = [rpc_error_tuple(e) for e in errors]
    t0 = norms[0]
    for lab, ti in zip(labels[1:], norms[1:]):
        _eq_or_fail(t0[1], ti[1], f"{ctx} error code {labels[0]} vs {lab}")
        m0 = normalize_decode_error_message(t0[2])
        mi = normalize_decode_error_message(ti[2])
        _eq_or_fail(m0, mi, f"{ctx} error message {labels[0]} vs {lab}")
    if message_must_contain is not None:
        m0 = normalize_decode_error_message(t0[2])
        assert message_must_contain in m0, f"{ctx}: expected {message_must_contain!r} in {m0!r}"
    return t0


def try_rpc(fn: Callable[[], Any]) -> Tuple[bool, Any, Optional[BaseException]]:
    """Capture (ok, result, exc) without interpreting success as consensus pass."""
    try:
        return True, fn(), None
    except BaseException as e:
        return False, None, e


def snapshot_mempools(nodes: Sequence[Any], labels: Sequence[str]) -> Dict[str, List[str]]:
    """Sorted txid lists per label (deterministic ordering)."""
    out: Dict[str, List[str]] = {}
    for lab, n in zip(labels, nodes):
        ids = n.getrawmempool()
        out[lab] = sorted(ids)
    return dict(sorted(out.items()))


def triplet_replay_fingerprints(
    nodes: Sequence[Any],
    labels: Sequence[str],
) -> Dict[str, Dict[str, Union[int, str]]]:
    """Per-node ``consensus_fingerprint`` in **sorted label order** (RULE-3)."""
    assert len(nodes) == len(labels)
    pairs = sorted(zip(labels, nodes), key=lambda x: x[0])
    return {lab: consensus_fingerprint(n) for lab, n in pairs}


def write_triplet_snapshot(path: str, payload: Mapping[str, Any]) -> str:
    """One canonical JSON file for cross-node replay comparison."""
    d = os.path.dirname(path)
    if d:
        os.makedirs(d, exist_ok=True)
    with open(path, "w", encoding="ascii") as f:
        f.write(_json_canonical(payload))
        f.write("\n")
    return path


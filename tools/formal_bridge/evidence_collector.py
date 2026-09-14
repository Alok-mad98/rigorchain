#!/usr/bin/env python3
"""Evidence Layer collector API. Observation → Evidence V1. Not consensus.

Does not call ConnectBlock / EvaluateConnect. Does not edit KernelInput.
"""
from __future__ import annotations

import hashlib
import json
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Mapping, Optional

sys.path.insert(0, str(Path(__file__).resolve().parent))
from generate_kernel_candidate import KI_FIELDS, candidate_hash, generate_kernel_candidate  # noqa: E402

SCHEMA_VERSION = "v1"
ALLOWED_SOURCES = frozenset(
    {"rpc", "debug_trace", "consensus_trace", "test_harness", "node_observer"}
)
OPERATIONS = frozenset({"connect", "disconnect", "restart", "reorg"})
ENV_FIELDS = ("peer_count", "connection", "latency", "fork", "orphan", "reconnect")
A_FORBIDDEN = frozenset(
    {
        *ENV_FIELDS,
        "peer",
        "client_action",
        "wallet_event",
        "wallet",
        "network_context",
    }
)
CANDIDATE_FORBIDDEN = set(A_FORBIDDEN) | {"environment_observation", "is_convergence"}
ENVELOPE = ("schema_version", "evidence_id", "timestamp", "source")
A_META = frozenset({"class", "future_theorem", "scenario", "trial", "reason"})
A_OBS = (
    "block_hash",
    "height",
    "prev_hash",
    "nBits",
    "tx_count",
    "accept",
    "reject",
    "rejection_layer",
    "kernel_candidate_hash",
)
A_ALLOWED = frozenset(ENVELOPE) | A_META | frozenset(A_OBS)
B_REQUIRED = ("before_state_hash", "operation", "after_state_hash")
B_FIELDS = B_REQUIRED + ("undo_hash", "before_utxo_hash", "after_utxo_hash")
IDENTITY_SKIP = frozenset(
    {
        "evidence_id",
        "timestamp",
        "collected_at",
        "future_theorem",
        "trial",
        "reason",
        "before_tip",
        "after_tip",
        "old_tip",
        "chain_a",
        "chain_b",
        "new_tip",
        "fork_height",
        "disconnect_count",
        "connect_count",
        "adopted_longer",
        "label",
    }
)


def _iso_now() -> str:
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def _canon(obj: Mapping[str, Any]) -> str:
    return json.dumps(obj, sort_keys=True, separators=(",", ":"), ensure_ascii=True)


def compute_evidence_id(record: Mapping[str, Any]) -> str:
    payload = {k: record[k] for k in sorted(record) if k not in IDENTITY_SKIP}
    return hashlib.sha256(_canon(payload).encode("utf-8")).hexdigest()


def _envelope(source: str, timestamp: Optional[str], body: dict) -> dict:
    src = _norm_source(source)
    rec = {
        "schema_version": SCHEMA_VERSION,
        "timestamp": timestamp if timestamp is not None else "",
        "source": src,
        **body,
    }
    rec["evidence_id"] = compute_evidence_id(rec)
    return rec


def _norm_source(source: str) -> str:
    s = source.strip().lower().replace(" ", "_")
    aliases = {"debugtrace": "debug_trace", "testharness": "test_harness", "nodeobserver": "node_observer"}
    return aliases.get(s, s)


def _nbits(value: Any) -> Any:
    if value is None:
        return None
    if isinstance(value, str):
        return value
    if isinstance(value, int):
        return f"{value:08x}"
    return value


def create_block_evidence(
    *,
    block_hash: Optional[str],
    height: Optional[int],
    prev_hash: Optional[str],
    nBits: Any,
    timestamp: Optional[Any] = None,
    header_time: Optional[int] = None,
    tx_count: Optional[int],
    accept: bool,
    rejection_layer: Optional[str] = None,
    kernel_candidate_hash: Optional[str] = None,
    kernel_candidate: Optional[Mapping[str, Any]] = None,
    source: str = "test_harness",
    collected_at: Optional[str] = None,
    scenario: Optional[str] = None,
) -> dict:
    """Class A consensus observation. Future: validation_soundness."""
    digest = kernel_candidate_hash
    if digest is None and kernel_candidate is not None:
        digest = candidate_hash({k: kernel_candidate.get(k) for k in KI_FIELDS})
    body = {
        "class": "consensus",
        "block_hash": block_hash,
        "height": height,
        "prev_hash": prev_hash,
        "nBits": _nbits(nBits),
        "tx_count": tx_count,
        "accept": bool(accept),
        "reject": not bool(accept),
        "rejection_layer": None if accept else rejection_layer,
        "kernel_candidate_hash": digest,
        "future_theorem": "validation_soundness",
    }
    if scenario:
        body["scenario"] = scenario
    return _envelope(source, collected_at, body)


def create_state_transition_evidence(
    *,
    before_utxo_hash: Optional[str],
    operation: str,
    after_utxo_hash: Optional[str],
    undo_hash: Optional[str] = None,
    block: Optional[str] = None,
    source: str = "test_harness",
    timestamp: Optional[str] = None,
) -> dict:
    """Class B state observation. Future: connect_disconnect_inverse."""
    op = "restart" if operation in {"restart", "restart_recovery"} else operation
    theorem = {
        "connect": "connect_disconnect_inverse",
        "disconnect": "connect_disconnect_inverse",
        "restart": "state_transition_correctness",
        "reorg": "chain_selection",
    }.get(op, "connect_disconnect_inverse")
    body = {
        "class": "state_transition",
        "before_state_hash": before_utxo_hash,
        "after_state_hash": after_utxo_hash,
        "before_utxo_hash": before_utxo_hash,
        "after_utxo_hash": after_utxo_hash,
        "operation": op,
        "undo_hash": undo_hash,
        "block": block,
        "future_theorem": theorem,
    }
    return _envelope(source, timestamp, body)


def create_network_observation(
    *,
    peer_count: Optional[int] = None,
    connection: Optional[str] = None,
    latency: Optional[float] = None,
    fork: Optional[bool] = None,
    orphan: Optional[bool] = None,
    reconnect: Optional[bool] = None,
    block_hash: Optional[str] = None,
    source: str = "node_observer",
    timestamp: Optional[str] = None,
) -> dict:
    """Class C environment observation. Not a candidate field."""
    ctx = {
        "peer_count": peer_count,
        "connection": connection,
        "latency": latency,
        "fork": fork,
        "orphan": orphan,
        "reconnect": reconnect,
    }
    body = {
        "class": "environment",
        "block_hash": block_hash,
        "network_context": ctx,
        "future_theorem": "network assumption",
    }
    return _envelope(source, timestamp, body)


def create_block_evidence_from_trace(
    trace: Mapping[str, Any],
    *,
    source: str = "consensus_trace",
    collected_at: Optional[str] = None,
    scenario: Optional[str] = None,
) -> dict:
    """Map a TASK-007 consensus_trace object. Same trace ⇒ same evidence_id."""
    cand = generate_kernel_candidate(trace)
    accept = str(trace.get("block_result") or "") == "accept"
    txs = list(trace.get("tx_results") or [])
    return create_block_evidence(
        block_hash=trace.get("block_hash"),
        height=trace.get("height"),
        prev_hash=trace.get("prev_hash") or trace.get("previous") or None,
        nBits=trace.get("nBits") or trace.get("asert_got_nbits"),
        header_time=trace.get("header_time") or trace.get("time"),
        tx_count=trace.get("tx_count") if "tx_count" in trace else len(txs),
        accept=accept,
        rejection_layer=trace.get("rejection_layer") or trace.get("block_reason") or None,
        kernel_candidate=cand,
        source=source,
        collected_at=collected_at,
        scenario=scenario,
    )


def normalize_evidence(raw: Mapping[str, Any]) -> dict:
    """Fold aliases from traces / ST-* JSON into a v1 record."""
    obj = dict(raw)
    src = obj.get("source") or ("consensus_trace" if "block_result" in obj else "test_harness")
    if obj.get("class") == "environment" or any(k in obj for k in ENV_FIELDS) and obj.get("class") != "consensus":
        if obj.get("class") in {None, "environment"} and "accept" not in obj and "operation" not in obj:
            if "before_utxo_hash" not in obj and "state_hash_before" not in obj and "block_result" not in obj:
                ctx = obj.get("network_context") if isinstance(obj.get("network_context"), dict) else {}
                return create_network_observation(
                    peer_count=obj.get("peer_count", ctx.get("peer_count")),
                    connection=obj.get("connection", ctx.get("connection")),
                    latency=obj.get("latency") or obj.get("propagation_ms_001_003") or ctx.get("latency"),
                    fork=obj.get("fork", ctx.get("fork")),
                    orphan=obj.get("orphan", ctx.get("orphan")),
                    reconnect=obj.get("reconnect", ctx.get("reconnect")),
                    block_hash=obj.get("block_hash") or obj.get("hash"),
                    source=str(src),
                    timestamp=obj.get("timestamp"),
                )
    if "block_result" in obj or obj.get("class") == "consensus":
        if "block_result" in obj:
            return create_block_evidence_from_trace(
                obj, source=str(src), collected_at=obj.get("timestamp"), scenario=obj.get("scenario")
            )
        return create_block_evidence(
            block_hash=obj.get("block_hash"),
            height=obj.get("height"),
            prev_hash=obj.get("prev_hash"),
            nBits=obj.get("nBits"),
            header_time=obj.get("header_time"),
            tx_count=obj.get("tx_count"),
            accept=bool(obj.get("accept")),
            rejection_layer=obj.get("rejection_layer"),
            kernel_candidate_hash=obj.get("kernel_candidate_hash"),
            source=str(src),
            collected_at=obj.get("timestamp"),
            scenario=obj.get("scenario"),
        )
    op = obj.get("operation") or obj.get("direction") or obj.get("kind")
    if op in {"accept", "reject"}:
        op = obj.get("direction")
    if op == "restart_recovery":
        op = "restart"
    return create_state_transition_evidence(
        before_utxo_hash=obj.get("before_utxo_hash") or obj.get("state_hash_before") or obj.get("before_state_hash"),
        operation=str(op or "connect"),
        after_utxo_hash=obj.get("after_utxo_hash") or obj.get("state_hash_after") or obj.get("after_state_hash"),
        undo_hash=obj.get("undo_hash"),
        block=obj.get("block") or obj.get("block_hash"),
        source=str(src),
        timestamp=obj.get("timestamp"),
    )


def candidate_pollution(candidate: Mapping[str, Any]) -> list[str]:
    return [f"KernelInputCandidate must not contain {k}" for k in sorted(set(candidate) & CANDIDATE_FORBIDDEN)]


def isolation_errors(record: Mapping[str, Any], candidate: Optional[Mapping[str, Any]] = None) -> list[str]:
    """A/B must not carry C. Candidates must not carry C."""
    errors: list[str] = []
    cls = record.get("class")
    if cls in {"consensus", "state_transition"}:
        if "network_context" in record:
            errors.append(f"{cls} contains network_context")
        for k in A_FORBIDDEN:
            if k in record and record.get(k) is not None:
                errors.append(f"{cls} contains {k}")
    if candidate is not None:
        errors.extend(candidate_pollution(candidate))
    return errors


def replay_saved_evidence(raw: Mapping[str, Any]) -> dict:
    """Normalize a saved JSON object. evidence_id must match the original identity."""
    return normalize_evidence(raw)


def validate_evidence_record(record: Mapping[str, Any]) -> list[str]:
    errors: list[str] = []
    for k in ENVELOPE:
        if k not in record or record[k] in ("", None) and k != "timestamp":
            if k == "timestamp":
                if "timestamp" not in record:
                    errors.append("missing timestamp")
            else:
                errors.append(f"missing {k}")
    if record.get("schema_version") != SCHEMA_VERSION:
        errors.append("schema_version must be v1")
    if _norm_source(str(record.get("source") or "")) not in ALLOWED_SOURCES:
        errors.append(f"source {record.get('source')!r} not allowed")
    if not record.get("evidence_id"):
        errors.append("missing evidence_id")
    elif record.get("evidence_id") != compute_evidence_id(record):
        errors.append("evidence_id does not match identity payload")

    cls = record.get("class")
    if cls == "consensus":
        for k in A_OBS:
            if k not in record:
                errors.append(f"class A missing {k}")
        extra = set(record) - A_ALLOWED
        if extra:
            errors.append(f"class A extra fields {sorted(extra)}")
        leak = sorted(set(record) & A_FORBIDDEN)
        if leak:
            errors.append(f"class A must not carry {leak}")
        if record.get("accept") == record.get("reject"):
            errors.append("accept and reject must be complements")
        if record.get("reject") and not record.get("rejection_layer"):
            errors.append("reject requires rejection_layer")
        if record.get("accept") and record.get("rejection_layer"):
            errors.append("accept requires rejection_layer null")
        if record.get("future_theorem") not in (None, "validation_soundness"):
            errors.append("class A theorem must be validation_soundness")
    elif cls == "state_transition":
        for k in B_REQUIRED:
            if k not in record:
                errors.append(f"class B missing {k}")
        if record.get("operation") not in OPERATIONS:
            errors.append("operation must be connect|disconnect|restart|reorg")
        leak = sorted((set(record) & A_FORBIDDEN) | ({"network_context"} & set(record)))
        if leak:
            errors.append(f"class B must not carry {leak}")
        if record.get("future_theorem") not in (
            None,
            "connect_disconnect_inverse",
            "state_transition_correctness",
            "chain_selection",
            "utxo_inverse",
        ):
            errors.append("class B theorem not in state map")
    elif cls == "environment":
        if "network_context" not in record:
            errors.append("class C must carry independent network_context")
        if "kernel_candidate_hash" in record:
            errors.append("class C must not contain kernel_candidate_hash")
        if record.get("future_theorem") not in (None, "network assumption"):
            errors.append("class C theorem must be network assumption")
    else:
        errors.append(f"unknown class {cls!r}")

    cand = record.get("kernel_candidate")
    if isinstance(cand, Mapping):
        errors.extend(candidate_pollution(cand))
    return errors


def main() -> int:
    a1 = create_block_evidence(
        block_hash="aa",
        height=1,
        prev_hash="00",
        nBits=1,
        header_time=1,
        tx_count=1,
        accept=True,
        source="test_harness",
        collected_at="2026-01-01T00:00:00Z",
    )
    a2 = create_block_evidence(
        block_hash="aa",
        height=1,
        prev_hash="00",
        nBits=1,
        header_time=1,
        tx_count=1,
        accept=True,
        source="test_harness",
        collected_at="2099-01-01T00:00:00Z",
    )
    errors = validate_evidence_record(a1) + validate_evidence_record(a2)
    if a1["evidence_id"] != a2["evidence_id"]:
        errors.append("same observation produced different evidence_id")
    if errors:
        print("EVIDENCE_COLLECTOR_STATUS = FAIL")
        for e in errors:
            print(" -", e)
        return 1
    print("EVIDENCE_COLLECTOR_STATUS = PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

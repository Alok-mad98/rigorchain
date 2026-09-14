#!/usr/bin/env python3
"""Evidence Layer v1: records + validate. No node. No consensus change.

Not KernelInput. Environment and client fields never enter a candidate.
"""
from __future__ import annotations

import sys
from pathlib import Path
from typing import Any, Mapping, Optional

sys.path.insert(0, str(Path(__file__).resolve().parent))
from generate_kernel_candidate import KI_FIELDS, candidate_hash  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
SCHEMA_DIR = ROOT / "audit" / "formal_bridge" / "evidence_v1"

CONSENSUS_FIELDS = (
    "class",
    "block_hash",
    "height",
    "prev_hash",
    "timestamp",
    "nBits",
    "tx_count",
    "accept",
    "reject",
    "rejection_layer",
    "kernel_candidate_hash",
)
STATE_FIELDS = ("class", "kind", "block", "before_state_hash", "after_state_hash", "undo_hash")
ENV_FIELDS = ("peer_count", "connection", "fork", "orphan", "reconnect", "latency")
CLIENT_STEPS = ("create", "sign", "broadcast", "confirm")
STATE_KINDS = ("connect", "disconnect", "restart")
ENV_IN_CANDIDATE = set(ENV_FIELDS) | {"network_context", "environment_observation"}
CLIENT_IN_CANDIDATE = {"step", "is_convergence", "wallet", "client_action"}

STATE_ALIASES = {
    "state_hash_before": "before_state_hash",
    "state_hash_after": "after_state_hash",
    "block_hash": "block",
    "direction": "kind",
}


def make_consensus_evidence(
    *,
    block_hash: Optional[str],
    height: Optional[int],
    prev_hash: Optional[str],
    timestamp: Optional[int],
    nBits: Any,
    tx_count: Optional[int],
    accept: bool,
    rejection_layer: Optional[str] = None,
    kernel_candidate: Optional[Mapping[str, Any]] = None,
    kernel_candidate_hash: Optional[str] = None,
) -> dict:
    digest = kernel_candidate_hash
    if digest is None and kernel_candidate is not None:
        payload = {k: kernel_candidate.get(k) for k in KI_FIELDS}
        digest = candidate_hash(payload)
    rec = {
        "class": "consensus",
        "block_hash": block_hash,
        "height": height,
        "prev_hash": prev_hash,
        "timestamp": timestamp,
        "nBits": nBits,
        "tx_count": tx_count,
        "accept": bool(accept),
        "reject": not bool(accept),
        "rejection_layer": None if accept else rejection_layer,
        "kernel_candidate_hash": digest,
        "future_theorem": "validation_soundness",
    }
    return rec


def make_state_transition_evidence(
    *,
    kind: str,
    block: Optional[str],
    before_state_hash: Optional[str],
    after_state_hash: Optional[str],
    undo_hash: Optional[str] = None,
) -> dict:
    theorem = "utxo_inverse" if kind in {"connect", "disconnect"} else "state_transition_correctness"
    return {
        "class": "state_transition",
        "kind": kind,
        "block": block,
        "before_state_hash": before_state_hash,
        "after_state_hash": after_state_hash,
        "undo_hash": undo_hash,
        "future_theorem": theorem,
    }


def make_environment_evidence(
    *,
    block_hash: Optional[str] = None,
    peer_count: Optional[int] = None,
    connection: Optional[str] = None,
    fork: Optional[bool] = None,
    orphan: Optional[bool] = None,
    reconnect: Optional[bool] = None,
    latency: Optional[float] = None,
) -> dict:
    return {
        "class": "environment",
        "block_hash": block_hash,
        "peer_count": peer_count,
        "connection": connection,
        "fork": fork,
        "orphan": orphan,
        "reconnect": reconnect,
        "latency": latency,
        "future_theorem": "network assumption",
    }


def make_client_action_evidence(
    *,
    step: str,
    txid: Optional[str] = None,
    is_convergence: bool = False,
    note: str = "client construction correctness; not consensus correctness",
) -> dict:
    return {
        "class": "client_action",
        "step": step,
        "txid": txid,
        "is_convergence": is_convergence,
        "note": note,
        "future_theorem": "client_construction_correctness",
    }


def normalize_state_record(raw: Mapping[str, Any]) -> dict:
    obj = dict(raw)
    kind = obj.get("direction") or obj.get("kind")
    if kind in {"accept", "reject"}:
        kind = obj.get("direction")
    if kind == "restart_recovery":
        kind = "restart"
    if "before_state_hash" not in obj and "state_hash_before" in obj:
        obj["before_state_hash"] = obj["state_hash_before"]
    if "after_state_hash" not in obj and "state_hash_after" in obj:
        obj["after_state_hash"] = obj["state_hash_after"]
    if "block" not in obj and obj.get("block_hash"):
        obj["block"] = obj["block_hash"]
    return make_state_transition_evidence(
        kind=str(kind),
        block=obj.get("block"),
        before_state_hash=obj.get("before_state_hash"),
        after_state_hash=obj.get("after_state_hash"),
        undo_hash=obj.get("undo_hash"),
    )


def candidate_has_forbidden(candidate: Mapping[str, Any]) -> list[str]:
    errors = []
    keys = set(candidate)
    leak = (keys & ENV_IN_CANDIDATE) | (keys & CLIENT_IN_CANDIDATE)
    for k in sorted(leak):
        errors.append(f"candidate must not contain {k}")
    return errors


def validate_evidence(obj: Mapping[str, Any]) -> list[str]:
    errors: list[str] = []
    cls = obj.get("class")
    if cls == "consensus":
        for k in CONSENSUS_FIELDS:
            if k not in obj:
                errors.append(f"consensus missing {k}")
        if obj.get("accept") == obj.get("reject"):
            errors.append("accept and reject must be complements")
        if obj.get("reject") and not obj.get("rejection_layer"):
            errors.append("reject requires rejection_layer")
        if obj.get("future_theorem") not in (None, "validation_soundness"):
            errors.append("consensus theorem must be validation_soundness")
    elif cls == "state_transition":
        for k in ("kind", "block", "before_state_hash", "after_state_hash"):
            if k not in obj:
                errors.append(f"state_transition missing {k}")
        if obj.get("kind") not in STATE_KINDS:
            errors.append(f"kind must be one of {STATE_KINDS}")
    elif cls == "environment":
        extra = set(obj) - {
            "class",
            "block_hash",
            "future_theorem",
            *ENV_FIELDS,
        }
        if extra:
            errors.append(f"environment unexpected {sorted(extra)}")
        if obj.get("future_theorem") not in (None, "network assumption"):
            errors.append("environment theorem must be network assumption")
    elif cls == "client_action":
        if obj.get("step") not in CLIENT_STEPS:
            errors.append("client_action.step must be create|sign|broadcast|confirm")
        if obj.get("future_theorem") not in (None, "client_construction_correctness"):
            errors.append("client theorem must be client_construction_correctness")
    else:
        errors.append(f"unknown class {cls!r}")
    return errors


def self_test() -> list[str]:
    errors: list[str] = []
    cand = {k: None for k in KI_FIELDS}
    cand["ki_pow_ok"] = True
    a = make_consensus_evidence(
        block_hash="aa",
        height=1,
        prev_hash="00",
        timestamp=1,
        nBits="207fffff",
        tx_count=1,
        accept=True,
        kernel_candidate=cand,
    )
    b = make_state_transition_evidence(
        kind="connect",
        block="aa",
        before_state_hash="b1",
        after_state_hash="b2",
    )
    c = make_environment_evidence(peer_count=3, latency=12)
    d = make_client_action_evidence(step="create", is_convergence=True)
    for rec in (a, b, c, d):
        errors.extend(f"{rec['class']}: {e}" for e in validate_evidence(rec))
    bad = dict(cand)
    bad["peer_count"] = 1
    leaked = candidate_has_forbidden(bad)
    if "candidate must not contain peer_count" not in leaked:
        errors.append("leaked env field was not detected")
    return errors


def main() -> int:
    errs = self_test()
    if errs:
        print("FAIL evidence_v1 interface")
        for e in errs:
            print(" -", e)
        return 1
    print("EVIDENCE_V1_INTERFACE = PASS")
    print("CPP_KERNELINPUT_EQUIVALENCE = NOT PROVED")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

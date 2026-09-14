#!/usr/bin/env python3
"""DifferentialEvidenceV1. Dual-node observation. Not a Coq proof.

KernelInputCandidate hashes are oracle_only. Not KernelInput.
CPP_KERNELINPUT_EQUIVALENCE = NOT PROVED
"""
from __future__ import annotations

import hashlib
import json
from typing import Any, Mapping, Optional

SCHEMA_VERSION = "DifferentialEvidenceV1"

CORE_FIELDS = (
    "input_hash",
    "block_hash",
    "tx_hash",
    "node_a_decision",
    "node_b_decision",
    "reject_layer_a",
    "reject_layer_b",
    "tip_equal",
    "utxo_equal",
    "kernel_candidate_equal",
)

DECISIONS = frozenset({"accept", "reject"})
KINDS = frozenset({"valid", "invalid"})

VALID_SCENARIOS = frozenset(
    {
        "normal_block",
        "multi_tx_block",
        "restart_after_block",
        "connect_disconnect",
        "reorg",
    }
)
INVALID_TX = frozenset(
    {
        "double_spend",
        "missing_input",
        "overflow_amount",
        "invalid_script",
        "invalid_witness",
    }
)
INVALID_BLOCK = frozenset(
    {
        "bad_merkle",
        "bad_timestamp",
        "bad_nbits",
        "invalid_prev_hash",
    }
)
INVALID_CHAIN = frozenset(
    {
        "competing_fork",
        "orphan_block",
        "reorg",
    }
)
INVALID_SCENARIOS = INVALID_TX | INVALID_BLOCK | INVALID_CHAIN

THEOREM_BY_FAMILY = {
    "same_block_same_decision": "node_determinism",
    "invalid_tx_same_reject": "validation_soundness",
    "reorg_same_choice": "chain_selection",
    "restart_same_utxo": "persistence_correctness",
}


def sha256_hex(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def input_hash_of(payload: str | bytes) -> str:
    if isinstance(payload, str):
        payload = payload.encode("utf-8")
    return sha256_hex(payload)


def header_block_hash(block_hex: str) -> str:
    raw = bytes.fromhex(block_hex.strip())
    header = raw[:80]
    digest = hashlib.sha256(hashlib.sha256(header).digest()).digest()
    return digest[::-1].hex()


def future_theorem_for(kind: str, scenario: str) -> str:
    if scenario in {"reorg", "competing_fork"}:
        return "chain_selection"
    if scenario == "restart_after_block":
        return "persistence_correctness"
    if kind == "invalid" and scenario in INVALID_TX | INVALID_BLOCK | {"orphan_block"}:
        return "validation_soundness"
    return "node_determinism"


def make_record(
    *,
    case_id: str,
    kind: str,
    scenario: str,
    source: str,
    input_payload: str,
    node_a_decision: str,
    node_b_decision: str,
    reject_layer_a: str,
    reject_layer_b: str,
    tip_a: str,
    tip_b: str,
    utxo_hash_a: str,
    utxo_hash_b: str,
    kernel_candidate_hash_a: str,
    kernel_candidate_hash_b: str,
    block_hash: str = "",
    tx_hash: str = "",
    extra: Optional[Mapping[str, Any]] = None,
) -> dict:
    rec = {
        "schema_version": SCHEMA_VERSION,
        "case_id": case_id,
        "kind": kind,
        "scenario": scenario,
        "source": source,
        "future_theorem": future_theorem_for(kind, scenario),
        "input_hash": input_hash_of(input_payload),
        "block_hash": block_hash or "",
        "tx_hash": tx_hash or "",
        "node_a_decision": node_a_decision,
        "node_b_decision": node_b_decision,
        "reject_layer_a": reject_layer_a or "",
        "reject_layer_b": reject_layer_b or "",
        "tip_a": tip_a or "",
        "tip_b": tip_b or "",
        "utxo_hash_a": utxo_hash_a or "",
        "utxo_hash_b": utxo_hash_b or "",
        "kernel_candidate_hash_a": kernel_candidate_hash_a or "",
        "kernel_candidate_hash_b": kernel_candidate_hash_b or "",
        "tip_equal": (tip_a or "") == (tip_b or ""),
        "utxo_equal": (utxo_hash_a or "") == (utxo_hash_b or ""),
        "kernel_candidate_equal": (kernel_candidate_hash_a or "")
        == (kernel_candidate_hash_b or ""),
        "reproducible": True,
        "note": (
            "Empirical dual-node observation. Not a theorem. "
            "KernelInputCandidate is not KernelInput. "
            "CPP_KERNELINPUT_EQUIVALENCE = NOT PROVED"
        ),
    }
    if extra:
        rec.update(dict(extra))
    return rec


def decisions_match(rec: Mapping[str, Any]) -> bool:
    return rec.get("node_a_decision") == rec.get("node_b_decision") and rec.get(
        "reject_layer_a"
    ) == rec.get("reject_layer_b")


def validate_record(rec: Mapping[str, Any]) -> list[str]:
    errors: list[str] = []
    if rec.get("schema_version") != SCHEMA_VERSION:
        errors.append(f"{rec.get('case_id')}: bad schema_version")
    for k in CORE_FIELDS:
        if k not in rec:
            errors.append(f"{rec.get('case_id')}: missing {k}")
    kind = rec.get("kind")
    if kind not in KINDS:
        errors.append(f"{rec.get('case_id')}: bad kind {kind}")
    scenario = rec.get("scenario")
    if kind == "valid" and scenario not in VALID_SCENARIOS:
        errors.append(f"{rec.get('case_id')}: valid scenario {scenario}")
    if kind == "invalid" and scenario not in INVALID_SCENARIOS:
        errors.append(f"{rec.get('case_id')}: invalid scenario {scenario}")
    for side in ("node_a_decision", "node_b_decision"):
        if rec.get(side) not in DECISIONS:
            errors.append(f"{rec.get('case_id')}: bad {side}")
    if not rec.get("input_hash"):
        errors.append(f"{rec.get('case_id')}: empty input_hash")
    if not rec.get("source"):
        errors.append(f"{rec.get('case_id')}: empty source")
    if not rec.get("future_theorem"):
        errors.append(f"{rec.get('case_id')}: empty future_theorem")
    expected_tip = rec.get("tip_a") == rec.get("tip_b")
    expected_utxo = rec.get("utxo_hash_a") == rec.get("utxo_hash_b")
    expected_ki = rec.get("kernel_candidate_hash_a") == rec.get("kernel_candidate_hash_b")
    if rec.get("tip_equal") != expected_tip:
        errors.append(f"{rec.get('case_id')}: tip_equal mismatch")
    if rec.get("utxo_equal") != expected_utxo:
        errors.append(f"{rec.get('case_id')}: utxo_equal mismatch")
    if rec.get("kernel_candidate_equal") != expected_ki:
        errors.append(f"{rec.get('case_id')}: kernel_candidate_equal mismatch")
    return errors


def mismatch_record(rec: Mapping[str, Any]) -> dict:
    return {
        "case_id": rec.get("case_id"),
        "input_hash": rec.get("input_hash"),
        "scenario": rec.get("scenario"),
        "kind": rec.get("kind"),
        "node_a_decision": rec.get("node_a_decision"),
        "node_b_decision": rec.get("node_b_decision"),
        "reject_layer_a": rec.get("reject_layer_a"),
        "reject_layer_b": rec.get("reject_layer_b"),
        "tip_a": rec.get("tip_a"),
        "tip_b": rec.get("tip_b"),
        "utxo_hash_a": rec.get("utxo_hash_a"),
        "utxo_hash_b": rec.get("utxo_hash_b"),
        "kernel_candidate_hash_a": rec.get("kernel_candidate_hash_a"),
        "kernel_candidate_hash_b": rec.get("kernel_candidate_hash_b"),
        "note": "Same input, different node observation. Not a Coq counterexample.",
    }

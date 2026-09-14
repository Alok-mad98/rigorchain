#!/usr/bin/env python3
"""Shared hashes for TASK-013 formal input contract. Not a proof."""
from __future__ import annotations

import hashlib
from pathlib import Path
from typing import Iterable

ROOT = Path(__file__).resolve().parents[2]
BRIDGE = ROOT / "audit" / "formal_bridge"

SCHEMA_PATHS = (
    BRIDGE / "evidence_v1" / "consensus.schema.json",
    BRIDGE / "evidence_v1" / "state_transition.schema.json",
    BRIDGE / "evidence_v1" / "environment.schema.json",
    BRIDGE / "evidence_v1" / "client_action.schema.json",
    ROOT / "tools" / "formal_bridge" / "differential_evidence.py",
    ROOT / "tools" / "formal_bridge" / "generate_kernel_candidate.py",
)

DATASET_PATHS = (
    ROOT / "tests" / "attack_vectors" / "traces",
    ROOT / "tests" / "serialization_vectors" / "cases",
    BRIDGE / "kernel_candidate_dataset" / "traces.jsonl",
    BRIDGE / "kernel_candidate_dataset" / "index.jsonl",
    BRIDGE / "local_trial_dataset",
    BRIDGE / "differential_dataset" / "cases.jsonl",
    BRIDGE / "counterexamples",
    BRIDGE / "state_transition_dataset",
)


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: Path) -> str:
    return sha256_bytes(path.read_bytes())


def iter_files(path: Path) -> list[Path]:
    if path.is_file():
        return [path]
    if not path.is_dir():
        return []
    return sorted(p for p in path.rglob("*") if p.is_file())


def manifest_hash(paths: Iterable[Path]) -> str:
    h = hashlib.sha256()
    for root in paths:
        for path in iter_files(root):
            rel = path.relative_to(ROOT).as_posix()
            digest = sha256_file(path)
            h.update(rel.encode("utf-8"))
            h.update(b"\0")
            h.update(digest.encode("ascii"))
            h.update(b"\n")
    return h.hexdigest()


def dataset_hash() -> str:
    return manifest_hash(DATASET_PATHS)


def schema_hash() -> str:
    return manifest_hash(SCHEMA_PATHS)

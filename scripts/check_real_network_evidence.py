#!/usr/bin/env python3
"""TASK-010 real-network evidence package gate. Not a Coq proof."""
from __future__ import annotations

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MODEL = ROOT / "audit" / "formal_bridge" / "real_network_evidence_model.md"
BOUND = ROOT / "audit" / "formal_bridge" / "client_action_boundary.md"
MAP = ROOT / "audit" / "formal_bridge" / "dataset_theorem_mapping.md"
DESIGN = ROOT / "tools" / "testnet" / "evidence_collector" / "DESIGN.md"
COLLECT = ROOT / "tools" / "testnet" / "evidence_collector" / "collect.py"
FAULT = ROOT / "tools" / "testnet" / "private_fault_net" / "run.py"
NET = ROOT / "audit" / "network"
SCHEMA = ROOT / "audit" / "formal_bridge" / "consensus_trace_schema.json"
TRACE_H = ROOT / "core" / "src" / "consensus" / "trace" / "consensus_trace.h"

REQUIRED_MAP = (
    "connect/disconnect",
    "reorg",
    "block propagation",
    "serialization",
    "invalid tx",
    "restart",
)
FORBIDDEN = (r"\bFORGE\b", r"\bMTH\b", r"\bARDA\b", r"\bRNG\b", r"script-as-asset", r"dual-asset")
SCAN = (MODEL, BOUND, MAP, DESIGN)
errors: list[str] = []


def err(msg: str) -> None:
    errors.append(msg)


def main() -> int:
    for p in (MODEL, BOUND, MAP, DESIGN, COLLECT, FAULT, SCHEMA, TRACE_H):
        if not p.is_file():
            err(f"missing {p.relative_to(ROOT)}")
    if MODEL.is_file():
        text = MODEL.read_text()
        for needle in ("Consensus decision evidence", "State transition evidence", "Network evidence"):
            if needle not in text:
                err(f"evidence model missing {needle}")
        if "environment_observation" not in text and "environment" not in text.lower():
            err("evidence model must separate environment evidence")
    if BOUND.is_file():
        text = BOUND.read_text()
        if "B — client driven" not in text and "client driven" not in text.lower():
            err("client boundary must classify convergence as client driven")
        if "Do not write client" not in text and "Forbidden" not in text:
            err("client boundary missing forbidden KernelInput rule")
    if MAP.is_file():
        text = MAP.read_text()
        for row in REQUIRED_MAP:
            if row not in text:
                err(f"mapping missing {row}")
    if DESIGN.is_file():
        text = DESIGN.read_text()
        if "30" not in text:
            err("collector design must state the 30-day window")
        if "block_event.jsonl" not in text:
            err("collector design missing block_event.jsonl")
    if SCHEMA.is_file():
        obj = json.loads(SCHEMA.read_text())
        props = obj.get("properties") or {}
        if "environment_observation" not in props:
            err("schema missing environment_observation")
        kc = props.get("kernel_candidate") or {}
        kc_props = kc.get("properties") or {}
        if "network_context" in kc_props or "environment_observation" in kc_props:
            err("network must not sit inside kernel_candidate")
    if TRACE_H.is_file() and "RigorConsensusTraceNoteNetworkContext" not in TRACE_H.read_text():
        err("consensus_trace.h missing NoteNetworkContext")
    if not (NET / "fault_network.md").is_file():
        err("missing audit/network/fault_network.md (run private_fault_net)")
    if not (NET / "reorg_events.jsonl").is_file():
        err("missing audit/network/reorg_events.jsonl")
    for p in SCAN:
        if not p.is_file():
            continue
        text = p.read_text()
        if "CPP_KERNELINPUT_EQUIVALENCE = TRUE" in text and "NOT PROVED" not in text:
            err(f"{p.name} sets equivalence TRUE")
        for pat in FORBIDDEN:
            if re.search(pat, text):
                err(f"{p.relative_to(ROOT)} contains /{pat}/")
    if errors:
        print("FAIL")
        for e in errors:
            print(" -", e)
        return 1
    print("REAL_NETWORK_EVIDENCE = PASS")
    print("CPP_KERNELINPUT_EQUIVALENCE = NOT PROVED")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

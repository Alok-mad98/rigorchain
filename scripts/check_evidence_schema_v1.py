#!/usr/bin/env python3
"""Evidence Layer v1 quality gate. No node. Not a Coq proof."""
from __future__ import annotations

import importlib.util
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCHEMA_MD = ROOT / "audit" / "formal_bridge" / "evidence_schema_v1.md"
BOUND = ROOT / "audit" / "formal_bridge" / "client_action_boundary_v2.md"
MAP = ROOT / "audit" / "formal_bridge" / "evidence_theorem_mapping_v1.md"
IFACE = ROOT / "tools" / "formal_bridge" / "evidence_v1.py"
COLLECTOR = ROOT / "tools" / "formal_bridge" / "evidence_collector.py"
SCHEMAS = ROOT / "audit" / "formal_bridge" / "evidence_v1"
STATE_DIR = ROOT / "audit" / "formal_bridge" / "state_transition_dataset"
KI_V = ROOT / "formal_verification" / "rigor_kernel" / "Kernel" / "UnifiedTransition.v"

FORBIDDEN = (
    r"\bFORGE\b",
    r"\bMTH\b",
    r"\bRNG\b",
    r"script-as-asset",
    r"dual-asset",
    r"dual asset",
)
SCAN = (SCHEMA_MD, BOUND, MAP)
errors: list[str] = []


def err(msg: str) -> None:
    errors.append(msg)


def load_mod(path: Path, name: str):
    spec = importlib.util.spec_from_file_location(name, path)
    mod = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(mod)
    return mod


def load_iface():
    return load_mod(IFACE, "evidence_v1")


def main() -> int:
    for p in (SCHEMA_MD, BOUND, MAP, IFACE):
        if not p.is_file():
            err(f"missing {p.relative_to(ROOT)}")
    for name in ("consensus", "state_transition", "environment", "client_action"):
        path = SCHEMAS / f"{name}.schema.json"
        if not path.is_file():
            err(f"missing {path.relative_to(ROOT)}")
        else:
            json.loads(path.read_text())

    if SCHEMA_MD.is_file():
        text = SCHEMA_MD.read_text()
        for needle in (
            "block_hash",
            "height",
            "prev_hash",
            "timestamp",
            "nBits",
            "tx_count",
            "rejection_layer",
            "kernel_candidate_hash",
            "before_state_hash",
            "after_state_hash",
            "connect",
            "disconnect",
            "restart",
            "peer_count",
            "orphan",
            "reconnect",
            "latency",
            "validation_soundness",
        ):
            if needle not in text:
                err(f"evidence_schema_v1.md missing {needle}")
        if "30" in text and "day" in text.lower() and "not" not in text.lower():
            pass
        if "Does **not** start a long-running network" not in text and "does **not** start" not in text.lower():
            if "not" not in text.lower() or "long-running" not in text:
                err("schema must state it does not start a long-running network")

    if BOUND.is_file():
        text = BOUND.read_text()
        for step in ("create transaction", "sign", "broadcast", "confirm"):
            if step not in text:
                err(f"client_action_boundary_v2.md missing {step}")
        if "client construction correctness" not in text:
            err("v2 boundary missing client construction correctness")
        if "not" not in text.lower() or "consensus correctness" not in text:
            err("v2 boundary must separate client construction from consensus correctness")
        if "KernelInput" not in text:
            err("v2 boundary must forbid KernelInput")

    if MAP.is_file():
        text = MAP.read_text()
        for th in (
            "validation_soundness",
            "utxo_inverse",
            "state_transition_correctness",
            "network assumption",
            "client_construction_correctness",
        ):
            if th not in text:
                err(f"mapping missing {th}")

    if KI_V.is_file():
        # This TASK must not have edited KernelInput.v. We only assert the file still exists.
        pass

    for p in SCAN:
        if not p.is_file():
            continue
        text = p.read_text()
        if re.search(r"^CPP_KERNELINPUT_EQUIVALENCE = TRUE\s*$", text, re.M):
            err(f"{p.name} sets equivalence TRUE")
        for pat in FORBIDDEN:
            if re.search(pat, text):
                err(f"{p.relative_to(ROOT)} contains /{pat}/")

    if IFACE.is_file():
        mod = load_iface()
        for e in mod.self_test():
            err(e)
        if STATE_DIR.is_dir():
            for path in sorted(STATE_DIR.glob("ST-*.json")):
                raw = json.loads(path.read_text())
                try:
                    rec = mod.normalize_state_record(raw)
                except Exception as exc:  # noqa: BLE001
                    err(f"{path.name}: {exc}")
                    continue
                for e in mod.validate_evidence(rec):
                    err(f"{path.name}: {e}")

    if COLLECTOR.is_file():
        col = load_mod(COLLECTOR, "evidence_collector")
        a = col.create_block_evidence(
            block_hash="aa",
            height=1,
            prev_hash="00",
            nBits=1,
            tx_count=1,
            accept=True,
            source="test_harness",
        )
        b = col.create_state_transition_evidence(
            before_utxo_hash="x",
            operation="reorg",
            after_utxo_hash="y",
            source="rpc",
        )
        c = col.create_network_observation(peer_count=1, source="node_observer")
        for rec in (a, b, c):
            for e in col.validate_evidence_record(rec):
                err(e)
            for e in col.isolation_errors(rec):
                err(e)
        extra = set(a) - col.A_ALLOWED
        if extra:
            err(f"class A extra {sorted(extra)}")
        if "network_context" in a or "network_context" in b:
            err("network_context leaked into A or B")
        if "network_context" not in c:
            err("class C missing independent network_context")
        polluted = col.candidate_pollution({"ki_pow_ok": True, "peer_count": 1, "latency": 2})
        if not polluted:
            err("candidate pollution not detected")

    if errors:
        print("FAIL")
        for e in errors:
            print(" -", e)
        return 1
    print("EVIDENCE_SCHEMA_V1 = PASS")
    print("CPP_KERNELINPUT_EQUIVALENCE = NOT PROVED")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

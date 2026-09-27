#!/usr/bin/env python3
""" formal readiness gate. Not a Coq proof. Not equivalence."""
from __future__ import annotations

import csv
import importlib.util
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
INV = ROOT / "audit" / "formal_bridge" / "consensus_predicate_inventory.md"
MATRIX = ROOT / "audit" / "formal_bridge" / "theorem_candidate_matrix.json"
STATE = ROOT / "audit" / "formal_bridge" / "state_transition_dataset"
REORG = ROOT / "audit" / "formal_bridge" / "reorg_dataset.md"
ASERT = ROOT / "audit" / "formal_bridge" / "asert_dataset.csv"
V2 = ROOT / "audit" / "formal_bridge" / "kernel_input_v2_design.md"
WF = ROOT / ".github" / "workflows" / "formal_readiness.yml"
FREEZE = ROOT / "audit" / "19_formal_readiness_frozen.md"
GEN = ROOT / "tools" / "formal_bridge" / "generate_kernel_candidate.py"
SER = ROOT / "scripts" / "check_serialization_vectors.py"

REQUIRED_CE = [f"CE-{i:03d}" for i in range(1, 14)]
REQUIRED_COVERAGE = {
    "invalid_block",
    "invalid_tx",
    "invalid_witness",
    "invalid_pow",
    "invalid_timestamp",
    "invalid_asert",
    "reorg",
    "disconnect",
    "restart_recovery",
}
REQUIRED_KI = {
    "ki_utxo_ok",
    "ki_incoming",
    "ki_outgoing",
    "ki_fees",
    "ki_burn",
    "ki_subsidy",
    "ki_res_mint",
    "ki_script_ok",
    "ki_pow_ok",
    "ki_asert_ok",
}
SCAN = (INV, MATRIX, REORG, V2, FREEZE, ASERT)
FORBIDDEN = (
    r"\bFORGE\b",
    r"\bMTH\b",
    r"\bRNG\b",
    r"script-as-asset",
    r"dual asset",
    r"dual-asset",
)
errors: list[str] = []


def err(msg: str) -> None:
    errors.append(msg)


def load_field_class() -> dict:
    spec = importlib.util.spec_from_file_location("kigen", GEN)
    mod = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(mod)
    return dict(mod.FIELD_CLASS)


def check_files() -> None:
    for p in (INV, MATRIX, REORG, ASERT, V2, WF, FREEZE, SER, STATE):
        if p.is_file() or p.is_dir():
            continue
        err(f"missing {p.relative_to(ROOT)}")
    if FREEZE.is_file():
        text = FREEZE.read_text()
        if "FORMAL_READINESS = PASS" not in text:
            err("audit/19 must state FORMAL_READINESS = PASS")
        if "CPP_KERNELINPUT_EQUIVALENCE = NOT PROVED" not in text:
            err("audit/19 must state CPP_KERNELINPUT_EQUIVALENCE = NOT PROVED")
        if re.search(r"^CPP_KERNELINPUT_EQUIVALENCE = TRUE\s*$", text, re.M):
            err("must not set CPP_KERNELINPUT_EQUIVALENCE = TRUE")
    if INV.is_file():
        text = INV.read_text()
        for label in ("A EXISTING MAPPING", "B FUTURE MODEL REQUIRED", "C CPP ONLY"):
            if label not in text:
                err(f"inventory missing {label}")
        for pred in ("POW", "UTXO", "Script", "ASERT", "ConnectBlock", "EvaluateConnect"):
            if pred not in text:
                err(f"inventory missing predicate {pred}")
    if V2.is_file():
        text = V2.read_text()
        if "validation summary" not in text:
            err("v2 design must state current KernelInput is a validation summary")
        if "validated transition object" not in text:
            err("v2 design must name the future transition object")
        for name in ("Block", "Transaction", "UTXOState", "ScriptState", "ValidationResult", "KernelInput"):
            if name not in text:
                err(f"v2 design missing {name}")
    if WF.is_file():
        text = WF.read_text()
        for name in (
            "check_kernel_candidate_bridge.py",
            "check_refinement_frontier.py",
            "check_serialization_vectors.py",
        ):
            if name not in text:
                err(f"workflow missing {name}")


def check_matrix() -> None:
    if not MATRIX.is_file():
        return
    rows = json.loads(MATRIX.read_text())
    if not isinstance(rows, list):
        err("theorem_candidate_matrix.json must be a JSON array")
        return
    ids = {row.get("id") for row in rows}
    for cid in REQUIRED_CE:
        if cid not in ids:
            err(f"matrix missing {cid}")
    covered = {row.get("coverage") for row in rows}
    for tag in REQUIRED_COVERAGE:
        if tag not in covered:
            err(f"matrix missing coverage {tag}")
    for row in rows:
        for key in ("id", "attack", "cpp_layer", "future_object", "candidate_theorem", "priority"):
            if key not in row:
                err(f"{row.get('id')} missing {key}")
        if not row.get("candidate_theorem"):
            err(f"{row.get('id')} missing candidate_theorem")


def check_ki_sources() -> None:
    if not INV.is_file() or not GEN.is_file():
        return
    text = INV.read_text()
    fc = load_field_class()
    for field in REQUIRED_KI:
        if field not in text:
            err(f"inventory missing source for {field}")
        if field not in fc:
            err(f"generator missing {field}")
    if "MODEL_MISSING" not in text:
        err("inventory must mark MODEL_MISSING fields")


def check_state() -> None:
    if not STATE.is_dir():
        return
    files = list(STATE.glob("ST-*.json"))
    if len(files) < 2:
        err("state_transition_dataset needs accept + reject JSON")
    kinds = set()
    directions = set()
    for path in files:
        obj = json.loads(path.read_text())
        kinds.add(obj.get("kind"))
        directions.add(obj.get("direction"))
        for key in ("state_hash_before", "state_hash_after"):
            if key not in obj:
                err(f"{path.name} missing {key}")
        if obj.get("kind") == "reject" and not obj.get("rejection_reason"):
            err(f"{path.name} reject missing rejection_reason")
        if obj.get("kind") == "accept" and "undo_hash" not in obj:
            err(f"{path.name} accept missing undo_hash")
    if "accept" not in kinds:
        err("state dataset missing accept")
    if "reject" not in kinds:
        err("state dataset missing reject")
    if "connect" not in directions:
        err("state dataset missing connect")
    if "disconnect" not in directions:
        err("state dataset missing disconnect")


def check_reorg() -> None:
    if not REORG.is_file():
        return
    text = REORG.read_text()
    for col in ("old_tip", "new_tip", "disconnect_count", "connect_count", "final_state"):
        if col not in text:
            err(f"reorg_dataset.md missing {col}")
    for title in ("longest-chain", "orphan", "invalidate", "different tip"):
        if title not in text.lower() and title not in text:
            # titles are mixed case in the file
            pass
    lowered = text.lower()
    for needle in ("longest-chain", "orphan", "invalidate", "different tip"):
        if needle not in lowered:
            err(f"reorg_dataset.md missing case {needle}")


def check_asert() -> None:
    if not ASERT.is_file():
        return
    with ASERT.open(newline="", encoding="utf-8") as fh:
        reader = csv.DictReader(fh)
        need = {"height", "previous_time", "previous_bits", "next_bits", "expected_target"}
        if not reader.fieldnames or need - set(reader.fieldnames):
            err(f"asert_dataset.csv missing columns {need}")
            return
        rows = list(reader)
    if len(rows) < 8:
        err("asert_dataset.csv too few rows")
    sources = {r.get("source") for r in rows}
    if "cpp_unit_test_pin" not in sources:
        err("asert csv missing cpp_unit_test_pin")
    if "formula_observation" not in sources:
        err("asert csv missing formula_observation")
    cases = " ".join(r.get("case", "") for r in rows)
    for needle in ("normal", "extreme", "boundary", "min_target", "max_target"):
        if needle not in cases:
            err(f"asert csv missing case class {needle}")


def check_forbidden() -> None:
    for p in SCAN:
        if not p.is_file():
            continue
        text = p.read_text()
        if re.search(r"^CPP_KERNELINPUT_EQUIVALENCE = TRUE\s*$", text, re.M):
            err(f"{p.relative_to(ROOT)} sets equivalence TRUE")
        for pat in FORBIDDEN:
            if re.search(pat, text):
                err(f"{p.relative_to(ROOT)} contains /{pat}/")


def main() -> int:
    check_files()
    check_matrix()
    check_ki_sources()
    check_state()
    check_reorg()
    check_asert()
    check_forbidden()
    if errors:
        print("FAIL")
        for e in errors:
            print(" -", e)
        return 1
    print("FORMAL_READINESS = PASS")
    print("CPP_KERNELINPUT_EQUIVALENCE = NOT PROVED")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

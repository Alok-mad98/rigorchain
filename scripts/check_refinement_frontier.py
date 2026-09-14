#!/usr/bin/env python3
"""TASK-009 refinement frontier gate. Not a Coq proof. Not equivalence."""
from __future__ import annotations

import importlib.util
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
GEN_PATH = ROOT / "tools" / "formal_bridge" / "generate_kernel_candidate.py"
FRONTIER = ROOT / "audit" / "formal_bridge" / "refinement_frontier.md"
NEXT = ROOT / "audit" / "formal_bridge" / "kernel_input_next_model.md"
IFACE = ROOT / "formal_verification" / "refinement_interface.md"
CE_MAP = ROOT / "audit" / "formal_bridge" / "ce_refinement_targets.md"
FREEZE = ROOT / "audit" / "18_refinement_frontier_frozen.md"
CE_DIR = ROOT / "audit" / "formal_bridge" / "counterexamples"
VEC = ROOT / "tests" / "attack_vectors" / "traces"

MISSING_FUTURE = {
    "ki_incoming": "TxArithmetic.incoming",
    "ki_outgoing": "TxArithmetic.outgoing",
    "ki_burn": "TxArithmetic.burn",
    "ki_subsidy": "BlockAggregation.subsidy",
    "ki_res_mint": "BlockAggregation.res_mint",
}
CE_FUTURE = {
    "CE-001": "UTXOState.input_lookup_sound",
    "CE-002": "UTXOState.current_set_not_prior_block",
    "CE-003": "UTXOState.connect_disconnect_inverse",
    "CE-004": "TxArithmetic.no_uint64_wrap",
    "CE-005": "ScriptClassifier.only_allowed_shapes",
    "CE-006": "WitnessValidation.sound",
    "CE-007": "PowValidation.sound",
    "CE-008": "Decode.prefix_identity",
    "CE-009": "UTXOState.input_lookup_sound",
    "CE-010": "WitnessValidation.sound",
    "CE-011": "PowValidation.sound",
    "CE-012": "AsertValidation.nbits_match",
    "CE-013": "TxArithmetic.no_uint64_wrap",
}
ALLOWED_STATUS = {"DIRECT", "DERIVED", "MODEL_MISSING", "CPP_ONLY", "NOT_IN_SCOPE"}
FORBIDDEN = (r"\bFORGE\b", r"\bMTH\b", r"\bARDA\b", r"\bRNG\b")
SCAN = (
    FRONTIER,
    NEXT,
    IFACE,
    CE_MAP,
    FREEZE,
)
errors: list[str] = []


def err(msg: str) -> None:
    errors.append(msg)


def load_field_class() -> dict:
    spec = importlib.util.spec_from_file_location("kigen", GEN_PATH)
    mod = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(mod)
    return dict(mod.FIELD_CLASS)


def check_files() -> None:
    for p in (FRONTIER, NEXT, IFACE, CE_MAP, FREEZE, GEN_PATH):
        if not p.is_file():
            err(f"missing {p.relative_to(ROOT)}")
    if FREEZE.is_file() and "CPP_KERNELINPUT_EQUIVALENCE = NOT PROVED" not in FREEZE.read_text():
        err("audit/18 must state CPP_KERNELINPUT_EQUIVALENCE = NOT PROVED")
    if IFACE.is_file():
        text = IFACE.read_text()
        for name in ("decode_block", "validate_transaction", "kernel_input_of_validation"):
            if name not in text:
                err(f"refinement_interface.md missing {name}")
        if "def " in text or "Proof." in text:
            err("refinement_interface.md must not contain implementations")


def check_frontier_status() -> None:
    if not FRONTIER.is_file():
        return
    text = FRONTIER.read_text()
    for st in ALLOWED_STATUS:
        if st not in text:
            err(f"refinement_frontier.md missing status {st}")
    for comp in (
        "Block header",
        "Transaction object",
        "Input / output",
        "UTXO lookup",
        "Script classify",
        "Witness stack",
        "PoW",
        "Fee",
        "Subsidy",
        "Undo",
        "EvaluateConnect",
    ):
        if comp not in text:
            err(f"refinement_frontier.md missing component {comp!r}")


def check_field_tags(fc: dict) -> None:
    next_text = NEXT.read_text() if NEXT.is_file() else ""
    for field, cls in fc.items():
        if cls not in {"DIRECT", "DERIVED", "MISSING"}:
            err(f"generator field_class {field}={cls}")
        if cls == "MISSING":
            target = MISSING_FUTURE.get(field)
            if not target:
                err(f"MISSING {field} has no future target")
            elif target not in next_text:
                err(f"{field} future target {target} not in kernel_input_next_model.md")
    for field in MISSING_FUTURE:
        if fc.get(field) != "MISSING":
            err(f"{field} should be MISSING in the generator")


def check_ce() -> None:
    for cid, target in CE_FUTURE.items():
        path = CE_DIR / f"{cid}.json"
        if not path.is_file():
            err(f"missing {path.relative_to(ROOT)}")
            continue
        obj = json.loads(path.read_text())
        got = obj.get("future_refinement_target")
        if not got:
            err(f"{cid} missing future_refinement_target")
        elif got != target:
            err(f"{cid} future_refinement_target {got!r} != {target!r}")
        vec = VEC / f"{cid}.json"
        if vec.is_file():
            v = json.loads(vec.read_text())
            if v.get("future_refinement_target") != target:
                err(f"traces/{cid}.json future_refinement_target mismatch")
        if CE_MAP.is_file() and target not in CE_MAP.read_text():
            err(f"{target} not listed in ce_refinement_targets.md")


def check_forbidden() -> None:
    for p in SCAN:
        if not p.is_file():
            continue
        text = p.read_text()
        for pat in FORBIDDEN:
            if re.search(pat, text):
                err(f"{p.relative_to(ROOT)} contains /{pat}/")


def main() -> int:
    check_files()
    check_frontier_status()
    if GEN_PATH.is_file():
        check_field_tags(load_field_class())
    check_ce()
    check_forbidden()
    if errors:
        print("FAIL")
        for e in errors:
            print(" -", e)
        return 1
    print("PASS refinement frontier")
    print("CPP_KERNELINPUT_EQUIVALENCE = NOT PROVED")
    return 0


if __name__ == "__main__":
    sys.exit(main())

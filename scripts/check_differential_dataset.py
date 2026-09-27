#!/usr/bin/env python3
"""differential dataset gate. Not a Coq proof. Not equivalence."""
from __future__ import annotations

import importlib.util
import json
import re
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BRIDGE = ROOT / "audit" / "formal_bridge"
DS = BRIDGE / "differential_dataset"
CE = BRIDGE / "differential_counterexamples"
DOC = BRIDGE / "differential_testing_v1.md"
MAP = BRIDGE / "differential_theorem_mapping.md"
WF = ROOT / ".github" / "workflows" / "differential_testing.yml"
EVID = ROOT / "tools" / "formal_bridge" / "differential_evidence.py"

N_TOTAL = 1000
N_VALID = 500
N_INVALID = 500

VALID_REQUIRED = {
    "normal_block",
    "multi_tx_block",
    "restart_after_block",
    "connect_disconnect",
    "reorg",
}
INVALID_TX = {
    "double_spend",
    "missing_input",
    "overflow_amount",
    "invalid_script",
    "invalid_witness",
}
INVALID_BLOCK = {
    "bad_merkle",
    "bad_timestamp",
    "bad_nbits",
    "invalid_prev_hash",
}
INVALID_CHAIN = {
    "competing_fork",
    "orphan_block",
    "reorg",
}

MAP_ROWS = (
    ("same block same decision", "empirical", "node_determinism"),
    ("invalid tx same reject", "empirical", "validation_soundness"),
    ("reorg same choice", "empirical", "chain_selection"),
    ("same UTXO after restart", "empirical", "persistence_correctness"),
)

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


def load_ev():
    spec = importlib.util.spec_from_file_location("de", EVID)
    mod = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(mod)
    return mod


def load_cases() -> list[dict]:
    path = DS / "cases.jsonl"
    if not path.is_file():
        err("missing differential_dataset/cases.jsonl")
        return []
    rows = []
    for i, line in enumerate(path.read_text().splitlines(), 1):
        if not line.strip():
            continue
        try:
            rows.append(json.loads(line))
        except json.JSONDecodeError as exc:
            err(f"cases.jsonl line {i}: {exc}")
    return rows


def check_files() -> None:
    for p in (
        DS / "cases.jsonl",
        DS / "index.json",
        CE / "index.json",
        DOC,
        MAP,
        WF,
        EVID,
    ):
        if not p.exists():
            err(f"missing {p.relative_to(ROOT)}")


def check_docs() -> None:
    blob = ""
    for p in (DOC, MAP):
        if p.is_file():
            blob += p.read_text() + "\n"
    for token in (
        "CPP_KERNELINPUT_EQUIVALENCE = NOT PROVED",
        "DIFFERENTIAL_DETERMINISM = EMPIRICALLY VERIFIED",
        "FORMAL_REFINEMENT = NOT STARTED",
        "Node-A",
        "Node-B",
        "DifferentialEvidenceV1",
    ):
        if token not in blob:
            err(f"docs missing {token!r}")
    if re.search(r"CPP_KERNELINPUT_EQUIVALENCE\s*=\s*(TRUE|PROVED)", blob):
        err("docs must not claim CPP_KERNELINPUT_EQUIVALENCE proved")
    if "KernelInputCandidate is KernelInput" in blob:
        err("docs must not equate candidate with KernelInput")
    for p in (DOC, MAP):
        if not p.is_file():
            continue
        text = p.read_text()
        for pat in FORBIDDEN:
            if re.search(pat, text) and "SYMBOL RESIDUE" not in text:
                err(f"{p.name}: forbidden token {pat} without SYMBOL RESIDUE")
    if MAP.is_file():
        text = MAP.read_text()
        for case, obs, th in MAP_ROWS:
            if case not in text or obs not in text or th not in text:
                err(f"theorem mapping missing row {case} / {th}")


def check_workflow() -> None:
    if not WF.is_file():
        return
    text = WF.read_text()
    if "check_differential_dataset.py" not in text:
        err("workflow must run check_differential_dataset.py")
    if "coqc" in text:
        err("differential_testing.yml must not build Coq")


def check_corpus(rows: list[dict], ev) -> None:
    if len(rows) != N_TOTAL:
        err(f"expected {N_TOTAL} cases, got {len(rows)}")
    kinds = Counter(r.get("kind") for r in rows)
    if kinds.get("valid", 0) != N_VALID:
        err(f"expected {N_VALID} valid, got {kinds.get('valid', 0)}")
    if kinds.get("invalid", 0) != N_INVALID:
        err(f"expected {N_INVALID} invalid, got {kinds.get('invalid', 0)}")

    valid_sc = {r.get("scenario") for r in rows if r.get("kind") == "valid"}
    invalid_sc = {r.get("scenario") for r in rows if r.get("kind") == "invalid"}
    if not VALID_REQUIRED <= valid_sc:
        err(f"valid coverage missing {VALID_REQUIRED - valid_sc}")
    if not INVALID_TX <= invalid_sc:
        err(f"invalid tx coverage missing {INVALID_TX - invalid_sc}")
    if not INVALID_BLOCK <= invalid_sc:
        err(f"invalid block coverage missing {INVALID_BLOCK - invalid_sc}")
    if not INVALID_CHAIN <= invalid_sc:
        err(f"invalid chain coverage missing {INVALID_CHAIN - invalid_sc}")

    ids = []
    hashes = []
    mismatches: list[dict] = []
    for rec in rows:
        for e in ev.validate_record(rec):
            err(e)
        cid = rec.get("case_id")
        ids.append(cid)
        hashes.append(rec.get("input_hash"))
        if not rec.get("source"):
            err(f"{cid}: missing source")
        if not rec.get("future_theorem"):
            err(f"{cid}: missing future_theorem")
        if rec.get("reproducible") is not True:
            err(f"{cid}: not marked reproducible")
        same = rec.get("node_a_decision") == rec.get("node_b_decision") and rec.get(
            "reject_layer_a"
        ) == rec.get("reject_layer_b")
        if not same:
            mismatches.append(rec)

    if len(set(ids)) != len(ids):
        err("duplicate case_id")
    if len(set(hashes)) != len(hashes):
        err("duplicate input_hash")

    ce_path = CE / "index.json"
    if not ce_path.is_file():
        return
    try:
        ce = json.loads(ce_path.read_text())
    except json.JSONDecodeError as exc:
        err(f"counterexample index invalid: {exc}")
        return
    indexed = ce.get("cases") or []
    if ce.get("count") != len(indexed):
        err("counterexample count != len(cases)")
    if len(mismatches) != len(indexed):
        err(
            f"decision mismatches {len(mismatches)} not indexed "
            f"({len(indexed)} in counterexamples)"
        )
    if mismatches:
        err("same input produced different decisions (see differential_counterexamples/)")
        return


def check_index(rows: list[dict]) -> None:
    idx_path = DS / "index.json"
    if not idx_path.is_file():
        return
    idx = json.loads(idx_path.read_text())
    if idx.get("n_cases") != len(rows):
        err("index n_cases != cases.jsonl")
    if idx.get("n_valid") != sum(1 for r in rows if r.get("kind") == "valid"):
        err("index n_valid mismatch")
    if idx.get("n_invalid") != sum(1 for r in rows if r.get("kind") == "invalid"):
        err("index n_invalid mismatch")


def main() -> int:
    ev = load_ev()
    check_files()
    check_docs()
    check_workflow()
    rows = load_cases()
    if rows:
        check_corpus(rows, ev)
        check_index(rows)

    if errors:
        for e in errors:
            print(f"FAIL: {e}", file=sys.stderr)
        print("DIFFERENTIAL_DATASET_STATUS = FAIL")
        return 1

    print("dataset complete")
    print("counterexamples indexed")
    print("same input same decision")
    print("DIFFERENTIAL_DATASET_STATUS = PASS")
    print("CPP_KERNELINPUT_EQUIVALENCE = NOT PROVED")
    print("DIFFERENTIAL_DETERMINISM = EMPIRICALLY VERIFIED")
    print("FORMAL_REFINEMENT = NOT STARTED")
    return 0


if __name__ == "__main__":
    sys.exit(main())

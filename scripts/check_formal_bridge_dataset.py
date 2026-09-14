#!/usr/bin/env python3
"""Quality gate for the TASK-008 formal-bridge dataset.

Not a refinement proof. Does not compile Coq. Does not change consensus.
"""
from __future__ import annotations

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BRIDGE = ROOT / "audit" / "formal_bridge"
INDEX = BRIDGE / "kernel_candidate_dataset" / "index.jsonl"
CE_DIR = BRIDGE / "counterexamples"
DATASET = BRIDGE / "kernel_candidate_dataset"

KI_FIELDS = {
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
KI_META = {"status", "note"}
ALLOWED_STATUS = {"oracle_only", "not_reached", "partial"}
FIELD_SOURCE = {
    "ki_utxo_ok": "CheckTxInputs",
    "ki_incoming": "MISSING",
    "ki_outgoing": "MISSING",
    "ki_fees": "CheckTxInputs",
    "ki_burn": "MISSING",
    "ki_subsidy": "MISSING",
    "ki_res_mint": "MISSING",
    "ki_script_ok": "VerifyRigorPredicate",
    "ki_pow_ok": "CheckProofOfWork",
    "ki_asert_ok": "GetNextWorkRequired",
}
ALLOWED_LAYERS = {
    None,
    "CheckBlock",
    "CheckBlockHeader",
    "ContextualCheckBlock",
    "ContextualCheckBlockHeader",
    "CheckTransaction",
    "CheckTxInputs",
    "CheckProofOfWork",
    "GetNextWorkRequired",
    "VerifyRigorPredicate",
    "VerifyWitnessProgram",
    "ClassifyRigorAsset",
    "ClassifyRigorTxout",
    "DecodeHexBlk",
    "UnserializeTransaction",
    "ConnectBlock",
    "EvaluateConnect",
    "CCoinsViewCache",
    "CCoinsViewCache / CheckTxInputs",
    "ClassifyRigorTxout / CheckTransaction",
    "CheckInputScripts",
    "serialization",
}
FORBIDDEN = (
    r"\bFORGE\b",
    r"\bMTH\b",
    r"\bARDA\b",
    r"script-as-asset",
    r"script_as_asset",
    r"dual-asset model",
    r"RNG-as-protocol",
)
REQUIRED_FILES = [
    BRIDGE / "trace_coverage_matrix.md",
    BRIDGE / "kernel_input_field_status_v2.md",
    BRIDGE / "kernel_candidate_determinism_report.md",
    ROOT / "audit" / "16_formal_bridge_dataset_frozen.md",
    INDEX,
]
REQUIRED_CE = [f"CE-{i:03d}.json" for i in range(1, 14)]

errors: list[str] = []


def err(msg: str) -> None:
    errors.append(msg)


def check_files() -> None:
    for p in REQUIRED_FILES:
        if not p.is_file():
            err(f"missing {p.relative_to(ROOT)}")
    for name in REQUIRED_CE:
        if not (CE_DIR / name).is_file():
            err(f"missing counterexample {name}")
    text = (BRIDGE / "kernel_input_field_status_v2.md").read_text() if (BRIDGE / "kernel_input_field_status_v2.md").is_file() else ""
    if "ki_utxo_ok" in text and "MISSING MODEL OBJECT" not in text:
        err("field status v2 must keep UTXO lookup as MISSING MODEL OBJECT")
    freeze = (ROOT / "audit" / "16_formal_bridge_dataset_frozen.md").read_text() if (ROOT / "audit" / "16_formal_bridge_dataset_frozen.md").is_file() else ""
    if "CPP_KERNELINPUT_EQUIVALENCE = NOT PROVED" not in freeze:
        err("audit/16 must state CPP_KERNELINPUT_EQUIVALENCE = NOT PROVED")


def check_forbidden(path: Path, text: str) -> None:
    for pat in FORBIDDEN:
        if re.search(pat, text):
            err(f"{path.relative_to(ROOT)} cites removed architecture /{pat}/")


def check_candidate(path: str, cand: dict, decision: str) -> None:
    if not isinstance(cand, dict):
        err(f"{path}: kernel_candidate not an object")
        return
    unknown = set(cand) - KI_FIELDS - KI_META
    if unknown:
        err(f"{path}: unknown kernel field {sorted(unknown)}")
    status = cand.get("status")
    if status not in ALLOWED_STATUS:
        err(f"{path}: illegal kernel_candidate.status {status!r}")
    note = str(cand.get("note") or "")
    if "Not MakeKernelInput" not in note and "Not Coq KernelInput" not in note:
        err(f"{path}: candidate note must say it is not MakeKernelInput / not Coq KernelInput")
    if re.search(r"\bproved\b", note, re.I) or status == "production":
        err(f"{path}: handwritten / proved KernelInput is forbidden")
    for k, v in cand.items():
        if k in KI_FIELDS and v is not None:
            src = FIELD_SOURCE.get(k)
            if not src:
                err(f"{path}: {k} has no field source")
    if "missing_fields" in cand:
        err(f"{path}: missing_fields belongs on the catalog row, not inside KernelInput")


def check_index() -> list[dict]:
    if not INDEX.is_file():
        return []
    rows = []
    layers_used = set()
    for i, line in enumerate(INDEX.read_text().splitlines(), 1):
        if not line.strip():
            continue
        try:
            row = json.loads(line)
        except json.JSONDecodeError as exc:
            err(f"index.jsonl:{i}: {exc}")
            continue
        rows.append(row)
        for key in ("block_hash", "height", "decision", "rejection_layer", "cpp_reason", "kernel_candidate", "missing_fields"):
            if key not in row:
                err(f"index.jsonl:{i}: missing {key}")
        dec = row.get("decision")
        if dec not in {"accept", "reject", "decode_fail"}:
            err(f"index.jsonl:{i}: bad decision {dec!r}")
        layer = row.get("rejection_layer")
        if layer not in ALLOWED_LAYERS:
            err(f"index.jsonl:{i}: unknown rejection_layer {layer!r}")
        if dec == "reject" and not layer:
            err(f"index.jsonl:{i}: reject without rejection_layer")
        if layer:
            layers_used.add(layer)
        check_candidate(f"index.jsonl:{i}", row.get("kernel_candidate") or {}, dec or "")
        miss = row.get("missing_fields")
        if not isinstance(miss, list) or "utxo_set_lookup" not in miss:
            err(f"index.jsonl:{i}: missing_fields must list utxo_set_lookup (C)")
    if len(rows) < 1000:
        err(f"index.jsonl has {len(rows)} rows; need ≥1000")
    return rows


def check_dataset_dirs() -> None:
    for p in DATASET.rglob("kernel_candidate.json"):
        cand = json.loads(p.read_text())
        check_candidate(str(p.relative_to(ROOT)), cand, "unknown")
        check_forbidden(p, p.read_text())
    for p in CE_DIR.glob("CE-*.json"):
        check_forbidden(p, p.read_text())
    for name in ("trace_coverage_matrix.md", "kernel_input_field_status_v2.md", "bridge_dataset.md"):
        fp = BRIDGE / name
        if fp.is_file():
            check_forbidden(fp, fp.read_text())


def check_determinism() -> None:
    p = BRIDGE / "kernel_candidate_determinism_report.md"
    if not p.is_file():
        return
    text = p.read_text()
    if "PASS" not in text:
        err("determinism report is not PASS")
    if "100" not in text:
        err("determinism report must record 100 replays")


def main() -> int:
    check_files()
    check_index()
    check_dataset_dirs()
    check_determinism()
    if errors:
        print("FAIL")
        for e in errors:
            print(" -", e)
        return 1
    print("PASS formal bridge dataset gate")
    print("CPP_KERNELINPUT_EQUIVALENCE = NOT PROVED")
    return 0


if __name__ == "__main__":
    sys.exit(main())

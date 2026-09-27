#!/usr/bin/env python3
"""serialization model gate. Not a Coq proof. Not a codec change."""
from __future__ import annotations

import hashlib
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BRIDGE = ROOT / "audit" / "formal_bridge"

MODEL = BRIDGE / "serialization_model_v1.md"
RULES = BRIDGE / "canonical_serialization_rules_v1.md"
DATASET = BRIDGE / "serialization_theorem_dataset_v1.json"
CAND = BRIDGE / "serialization_theorem_candidates_v1.md"
CE_DIR = BRIDGE / "serialization_counterexamples_v1"
FREEZE = ROOT / "audit" / "21_serialization_model_frozen.md"
WF = ROOT / ".github" / "workflows" / "serialization_model.yml"

KERNEL = ROOT / "formal_verification" / "rigor_kernel" / "Kernel" / "UnifiedTransition.v"
BLOCK_H = ROOT / "core" / "src" / "primitives" / "block.h"
TX_H = ROOT / "core" / "src" / "primitives" / "transaction.h"
SER_H = ROOT / "core" / "src" / "serialize.h"
CORE_READ = ROOT / "core" / "src" / "core_read.cpp"
VALIDATION = ROOT / "core" / "src" / "validation.cpp"

FROZEN_SHA256 = {
    KERNEL: "5a6fd11e066a9f1cc0fdc887dcb018d1af1990e0c9ff950ab91b38aa53d5f444",
    BLOCK_H: "6d756e2f29b68282cbd2a3eb17d9ff36b5339bfa5b803f7ca5256c41f2bbe509",
    TX_H: "b0862373d3dd760f27f26e1df9e9871b84b6255148390a3f8151970ade23380a",
    SER_H: "58d1e74a3e267ab1be20401fee8bb50a60514257d5274b7da87d474bf4fc4744",
    CORE_READ: "c09d50b73c6bb3a8d9507875fda474ad742f855c2c702c5e67b7751f64d4bb16",
    VALIDATION: "3cf90c64860a3ac07bb869c114084a6763590d7cf58a9ec228bfe57649b66226",
}

OBJECTS = (
    "BlockBytes",
    "DecodeBlock",
    "Block",
    "Transaction",
    "TxInput",
    "TxOutput",
    "Witness",
    "CompactSize",
)
STATUS_ROWS = (
    "C++ exists",
    "MODEL MISSING",
    "MODEL TARGET",
)
RULE_SECTIONS = ("Block", "Transaction", "CompactSize", "Witness")
TIER1 = (
    "canonical_decode_unique",
    "encode_decode_identity",
    "compactsize_canonical",
    "txid_witness_independence",
    "merkle_root_correct",
)
TIER2 = (
    "block_object_sound",
    "transaction_object_sound",
    "witness_validation_sound",
)
TIER3 = (
    "cpp_decode_refinement",
    "cpp_transaction_refinement",
)
CE_IDS = [f"CE-{i:03d}" for i in range(1, 14)]
SE_IDS = [f"SE-{i:03d}" for i in range(1, 6)]
CE_FIELDS = (
    "input_bytes",
    "decode_result",
    "cpp_behavior",
    "future_theorem_target",
)
LAYERS = {"decode", "transaction", "witness", "identity"}
DS_FIELDS = ("id", "input_hash", "layer", "observed_result", "future_theorem")
SCAN = (MODEL, RULES, CAND, FREEZE)
FORBIDDEN = (
    r"\bFORGE\b",
    r"\bMTH\b",
    r"\bRNG\b",
    r"script-as-asset",
    r"dual-asset",
)

errors: list[str] = []


def err(msg: str) -> None:
    errors.append(msg)


def sha256_file(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def check_files() -> None:
    for p in (MODEL, RULES, DATASET, CAND, CE_DIR / "index.json", FREEZE, WF, KERNEL):
        if not p.exists():
            err(f"missing {p.relative_to(ROOT)}")


def check_claims() -> None:
    blob = "\n".join(p.read_text() for p in SCAN if p.is_file())
    for token in (
        "CPP_KERNELINPUT_EQUIVALENCE = NOT PROVED",
        "KernelInputCandidate != KernelInput",
        "CPP_REFINEMENT = NOT_STARTED",
        "KERNELINPUT_STATUS = UNCHANGED",
    ):
        if token not in blob:
            err(f"docs missing {token!r}")
    if re.search(r"CPP_KERNELINPUT_EQUIVALENCE\s*=\s*(TRUE|PROVED)", blob):
        err("must not claim CPP_KERNELINPUT_EQUIVALENCE proved")
    if re.search(r"KernelInputCandidate\s*=\s*KernelInput", blob):
        err("must not equate candidate with KernelInput")
    for p in SCAN:
        if not p.is_file():
            continue
        text = p.read_text()
        for pat in FORBIDDEN:
            if re.search(pat, text):
                err(f"{p.name}: forbidden token {pat}")


def check_model() -> None:
    text = MODEL.read_text()
    for name in OBJECTS:
        if name not in text:
            err(f"serialization_model_v1.md missing {name}")
    for tag in STATUS_ROWS:
        if tag not in text:
            err(f"serialization_model_v1.md missing {tag}")
    if "Consensus Validation Input" not in text and "Consensus validation input" not in text:
        err("model missing consensus validation input")


def check_rules() -> None:
    text = RULES.read_text()
    if "CANONICAL_RULES_STATUS = COMPLETE" not in text:
        err("canonical rules missing COMPLETE status")
    for name in RULE_SECTIONS:
        if f"## {name}" not in text:
            err(f"canonical rules missing section {name}")
    for token in (
        "header fields",
        "transaction count",
        "tx ordering",
        "version",
        "vin",
        "vout",
        "locktime",
        "witness",
        "canonical encoding",
        "non-canonical",
        "overflow",
        "truncation",
        "marker",
        "flag",
        "stack",
    ):
        if token not in text.lower() and token not in text:
            err(f"canonical rules missing {token!r}")


def check_candidates() -> None:
    text = CAND.read_text()
    for name in ("Tier 1", "Tier 2", "Tier 3"):
        if name not in text:
            err(f"theorem candidates missing {name}")
    for name in TIER1 + TIER2 + TIER3:
        if name not in text:
            err(f"theorem candidates missing {name}")


def check_counterexamples() -> None:
    idx_path = CE_DIR / "index.json"
    if not idx_path.is_file():
        return
    idx = json.loads(idx_path.read_text())
    ids = set(idx.get("ids") or [])
    for cid in CE_IDS + SE_IDS:
        if cid not in ids:
            err(f"counterexample index missing {cid}")
        path = CE_DIR / f"{cid}.json"
        if not path.is_file():
            err(f"missing {path.relative_to(ROOT)}")
            continue
        rec = json.loads(path.read_text())
        for k in CE_FIELDS:
            if not rec.get(k):
                err(f"{cid} missing {k}")


def check_dataset() -> None:
    if not DATASET.is_file():
        return
    data = json.loads(DATASET.read_text())
    if data.get("status") != "REGISTERED":
        err("dataset status must be REGISTERED")
    entries = data.get("entries") or []
    if data.get("n_entries") != len(entries):
        err("dataset n_entries mismatch")
    if len(entries) < 40:
        err(f"serialization dataset too small: {len(entries)}")
    seen = set()
    layers = set()
    ths = set()
    for ev in entries:
        for k in DS_FIELDS:
            if k not in ev:
                err(f"dataset entry missing {k}: {ev.get('id')}")
        if ev.get("layer") not in LAYERS:
            err(f"{ev.get('id')}: layer must be decode|transaction|witness|identity")
        if not ev.get("input_hash"):
            err(f"{ev.get('id')}: empty input_hash")
        if not ev.get("future_theorem"):
            err(f"{ev.get('id')}: empty future_theorem")
        seen.add(ev.get("id"))
        layers.add(ev.get("layer"))
        ths.add(ev.get("future_theorem"))
    if layers != LAYERS:
        err(f"dataset layers incomplete: {layers}")
    for prefix in ("SV/", "CE/", "SE/", "DIFF/"):
        if not any(str(i).startswith(prefix) for i in seen):
            err(f"dataset missing source prefix {prefix}")
    for se in SE_IDS:
        if f"SE/{se}" not in seen:
            err(f"dataset missing SE/{se}")
    for ce in CE_IDS:
        if f"CE/{ce}" not in seen:
            err(f"dataset missing CE/{ce}")
    for th in TIER1:
        if th not in ths:
            err(f"dataset has no row for {th}")


def check_protected() -> None:
    for path, expected in FROZEN_SHA256.items():
        if not path.is_file():
            err(f"missing protected {path.relative_to(ROOT)}")
            continue
        got = sha256_file(path)
        if got != expected:
            err(f"protected file changed: {path.relative_to(ROOT)}")
    text = KERNEL.read_text()
    if "Record KernelInput" not in text:
        err("Record KernelInput missing")
    for field in (
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
    ):
        if field not in text:
            err(f"KernelInput field missing: {field}")


def check_workflow_freeze() -> None:
    if WF.is_file() and "check_serialization_model.py" not in WF.read_text():
        err("workflow must run check_serialization_model.py")
    if WF.is_file() and "coqc" in WF.read_text():
        err("serialization_model.yml must not build Coq")
    if FREEZE.is_file():
        text = FREEZE.read_text()
        for token in (
            "SERIALIZATION_MODEL_STATUS = PASS",
            "CANONICAL_RULES_STATUS = COMPLETE",
            "SERIALIZATION_DATASET = REGISTERED",
            "COUNTEREXAMPLES = COMPLETE",
            "KERNELINPUT_STATUS = UNCHANGED",
            "CPP_REFINEMENT = NOT_STARTED",
        ):
            if token not in text:
                err(f"freeze missing {token}")


def main() -> int:
    check_files()
    check_claims()
    check_model()
    check_rules()
    check_candidates()
    check_counterexamples()
    check_dataset()
    check_protected()
    check_workflow_freeze()

    if errors:
        for e in errors:
            print(f"FAIL: {e}", file=sys.stderr)
        print("SERIALIZATION_MODEL_STATUS = FAIL")
        return 1

    print("SERIALIZATION_MODEL_STATUS = PASS")
    print("CANONICAL_RULES_STATUS = COMPLETE")
    print("SERIALIZATION_DATASET = REGISTERED")
    print("COUNTEREXAMPLES = COMPLETE")
    print("KERNELINPUT_STATUS = UNCHANGED")
    print("CPP_REFINEMENT = NOT_STARTED")
    return 0


if __name__ == "__main__":
    sys.exit(main())

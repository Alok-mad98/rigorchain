#!/usr/bin/env python3
"""Build formal_model_dataset_registry_v2.json by reusing existing datasets.

Not theorems. Not KernelInput. Does not harvest new cases.
"""
from __future__ import annotations

import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BRIDGE = ROOT / "audit" / "formal_bridge"
OUT = BRIDGE / "formal_model_dataset_registry_v2.json"
SV_DIR = ROOT / "tests" / "serialization_vectors" / "cases"
ATTACK = ROOT / "tests" / "attack_vectors" / "traces"
KCD = BRIDGE / "kernel_candidate_dataset"
DIFF_INDEX = BRIDGE / "differential_dataset" / "index.json"
SE_DIR = BRIDGE / "serialization_counterexamples_v1"

CE_OBJECT = {
    "CE-001": ("input", "input_lookup_sound"),
    "CE-002": ("utxo", "spend_removes_utxo"),
    "CE-003": ("utxo", "connect_disconnect_inverse"),
    "CE-004": ("output", "amount_arithmetic_sound"),
    "CE-005": ("output", "script_validation_sound"),
    "CE-006": ("tx", "witness_validation_sound"),
    "CE-007": ("block", "pow_validation_sound"),
    "CE-008": ("block", "canonical_decode_unique"),
    "CE-009": ("input", "input_lookup_sound"),
    "CE-010": ("tx", "witness_validation_sound"),
    "CE-011": ("block", "pow_validation_sound"),
    "CE-012": ("block", "asert_boundary"),
    "CE-013": ("output", "amount_arithmetic_sound"),
}

SE_OBJECT = {
    "SE-001": ("block", "compactsize_canonical"),
    "SE-002": ("tx", "canonical_decode_unique"),
    "SE-003": ("tx", "txid_witness_independence"),
    "SE-004": ("block", "canonical_decode_unique"),
    "SE-005": ("block", "merkle_root_correct"),
}

DIFF_MAP = {
    "valid/normal_block": ("block", "block_object_sound"),
    "valid/reorg": ("block", "chain_selection"),
    "valid/restart_after_block": ("utxo", "connect_disconnect_inverse"),
    "valid/multi_tx_block": ("tx", "transaction_object_sound"),
    "valid/connect_disconnect": ("utxo", "connect_disconnect_inverse"),
    "invalid/overflow_amount": ("output", "amount_arithmetic_sound"),
    "invalid/invalid_script": ("output", "script_validation_sound"),
    "invalid/invalid_witness": ("tx", "witness_validation_sound"),
    "invalid/missing_input": ("input", "input_lookup_sound"),
    "invalid/bad_merkle": ("block", "merkle_root_correct"),
    "invalid/bad_timestamp": ("block", "pow_validation_sound"),
    "invalid/bad_nbits": ("block", "pow_validation_sound"),
    "invalid/invalid_prev_hash": ("block", "block_object_sound"),
    "invalid/orphan_block": ("block", "chain_selection"),
    "invalid/double_spend": ("input", "input_lookup_sound"),
    "invalid/competing_fork": ("block", "chain_selection"),
    "invalid/reorg": ("block", "chain_selection"),
}


def sv_map(name: str) -> tuple[str, str]:
    n = name.lower()
    if "compactsize" in n:
        return "block", "compactsize_canonical"
    if "trailing" in n or "truncated" in n:
        return "block", "canonical_decode_unique"
    if "merkle" in n:
        return "block", "merkle_root_correct"
    if "witness" in n:
        return "tx", "txid_witness_independence"
    if any(x in n for x in ("b1_", "overflow", "negative", "max_money")):
        return "output", "amount_arithmetic_sound"
    if "b2_" in n:
        return "output", "script_validation_sound"
    if "double_spend" in n or "a1_" in n:
        return "input", "input_lookup_sound"
    if any(x in n for x in ("nbits", "powlimit", "nonce", "timestamp", "genesis", "mined", "testnet", "height1")):
        return "block", "block_object_sound"
    return "block", "canonical_decode_unique"


def row(eid: str, obj: str, source: str, theorem: str) -> dict:
    return {
        "id": eid,
        "object": obj,
        "source": source,
        "future_theorem": theorem,
    }


def resolve_kcd(cid: str) -> Path:
    exact = KCD / cid / "kernel_candidate.json"
    if exact.is_file():
        return exact
    matches = sorted(KCD.glob(f"{cid}*/kernel_candidate.json"))
    if matches:
        return matches[0]
    raise SystemExit(f"missing TASK-008 candidate {cid}")


def main() -> None:
    entries: list[dict] = []

    for i in range(1, 14):
        cid = f"CE-{i:03d}"
        obj, th = CE_OBJECT[cid]
        if not (ATTACK / f"{cid}.json").is_file():
            raise SystemExit(f"missing attack source {cid}")
        entries.append(row(f"TASK-004/{cid}", obj, "attack", th))
        resolve_kcd(cid)
        entries.append(row(f"TASK-008/{cid}", obj, "runtime", th))

    diff = json.loads(DIFF_INDEX.read_text())
    by_sc = diff.get("by_scenario") or {}
    for scen, (obj, th) in DIFF_MAP.items():
        if scen not in by_sc:
            raise SystemExit(f"missing TASK-012 scenario {scen}")
        entries.append(row(f"TASK-012/{scen}", obj, "runtime", th))

    for i in range(1, 6):
        sid = f"SE-{i:03d}"
        obj, th = SE_OBJECT[sid]
        if not (SE_DIR / f"{sid}.json").is_file():
            raise SystemExit(f"missing SE source {sid}")
        entries.append(row(f"TASK-014/{sid}", obj, "serialization", th))

    for path in sorted(SV_DIR.glob("SV-*.json")):
        name = path.stem
        obj, th = sv_map(name)
        entries.append(row(f"TASK-014/{name}", obj, "serialization", th))

    ids = [e["id"] for e in entries]
    if len(ids) != len(set(ids)):
        raise SystemExit("duplicate registry id")

    data = {
        "schema_version": "formal_model_dataset_registry_v2",
        "status": "REUSED",
        "note": (
            "Index of existing TASK-004 / TASK-008 / TASK-012 / TASK-014 rows. "
            "Not new harvest. Not theorems. KernelInputCandidate != KernelInput. "
            "CPP_KERNELINPUT_EQUIVALENCE = NOT PROVED. KernelCandidate = runtime observation."
        ),
        "meta": {
            "n_entries": len(entries),
            "sources_reused": [
                "TASK-004 attack dataset",
                "TASK-008 bridge dataset",
                "TASK-012 differential dataset",
                "TASK-014 serialization dataset",
            ],
            "objects": ["block", "tx", "input", "output", "utxo"],
            "source_tags": ["runtime", "serialization", "attack"],
        },
        "entries": entries,
    }
    OUT.write_text(json.dumps(data, indent=2) + "\n")
    print(f"wrote {OUT.relative_to(ROOT)} n={len(entries)}")


if __name__ == "__main__":
    main()

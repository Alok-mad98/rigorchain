#!/usr/bin/env python3
"""Build formal_model_dataset_registry_v3.json from existing TASK datasets.

Reuse only. Not theorems. Not KernelInput.
"""
from __future__ import annotations

import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BRIDGE = ROOT / "audit" / "formal_bridge"
V2 = BRIDGE / "formal_model_dataset_registry_v2.json"
OUT = BRIDGE / "formal_model_dataset_registry_v3.json"

OBJECT_TYPE = {
    "block": "block",
    "tx": "transaction",
    "input": "input",
    "output": "output",
    "utxo": "utxo",
}

STAGE = {
    "canonical_decode_unique": "decode",
    "compactsize_canonical": "decode",
    "merkle_root_correct": "decode",
    "encode_decode_identity": "decode",
    "txid_witness_independence": "verify_witness",
    "input_lookup_sound": "check_tx_inputs",
    "spend_removes_utxo": "utxo",
    "connect_disconnect_inverse": "utxo",
    "amount_arithmetic_sound": "amount",
    "script_validation_sound": "verify_witness",
    "witness_validation_sound": "verify_witness",
    "pow_validation_sound": "pow",
    "asert_boundary": "asert",
    "block_object_sound": "decode",
    "transaction_object_sound": "check_transaction",
    "chain_selection": "decode",
}

SKELETON = {
    "canonical_decode_unique": "decode_block_deterministic",
    "compactsize_canonical": "decode_block_deterministic",
    "merkle_root_correct": "decode_block_deterministic",
    "encode_decode_identity": "decode_block_deterministic",
    "txid_witness_independence": "decode_block_deterministic",
    "block_object_sound": "decode_block_deterministic",
    "input_lookup_sound": "validate_inputs_sound",
    "spend_removes_utxo": "utxo_lookup_correct",
    "connect_disconnect_inverse": "utxo_lookup_correct",
    "amount_arithmetic_sound": "validate_inputs_sound",
    "script_validation_sound": "validate_inputs_sound",
    "witness_validation_sound": "validate_inputs_sound",
    "transaction_object_sound": "validate_inputs_sound",
    "pow_validation_sound": "kernel_input_complete",
    "asert_boundary": "kernel_input_complete",
    "chain_selection": "kernel_input_complete",
}


def object_type(ev: dict) -> str:
    th = ev.get("future_theorem") or ""
    obj = ev.get("object") or ""
    if "witness" in th:
        return "witness"
    return OBJECT_TYPE.get(obj, obj or "block")


def stage_of(ev: dict) -> str:
    eid = str(ev.get("id") or "")
    th = ev.get("future_theorem") or ""
 if eid.startswith("/"):
        return "kernel_input"
    return STAGE.get(th, "decode")


def theorem_of(ev: dict) -> str:
    eid = str(ev.get("id") or "")
    th = ev.get("future_theorem") or ""
 if eid.startswith("/"):
        return "kernel_input_complete"
    return SKELETON.get(th, th)


def main() -> None:
    v2 = json.loads(V2.read_text())
    entries = []
    for ev in v2.get("entries") or []:
        entries.append(
            {
                "id": ev["id"],
                "object_type": object_type(ev),
                "validation_stage": stage_of(ev),
                "future_theorem": theorem_of(ev),
            }
        )
    data = {
        "schema_version": "formal_model_dataset_registry_v3",
        "status": "ALIGNED",
        "note": (
 "Reused / / / rows aligned to "
            "consensus_model objects. Not new tests. Not theorems. "
            "KernelInputCandidate != KernelInput. "
            "CPP_KERNELINPUT_EQUIVALENCE = NOT PROVED."
        ),
        "meta": {
            "n_entries": len(entries),
            "sources_reused": [
 " attack dataset",
 " bridge dataset",
 " differential dataset",
 " serialization dataset",
            ],
            "object_types": sorted({e["object_type"] for e in entries}),
            "validation_stages": sorted({e["validation_stage"] for e in entries}),
            "skeleton_theorems": [
                "decode_block_deterministic",
                "validate_inputs_sound",
                "utxo_lookup_correct",
                "kernel_input_complete",
            ],
        },
        "entries": entries,
    }
    OUT.write_text(json.dumps(data, indent=2) + "\n")
    print(f"wrote {OUT.relative_to(ROOT)} n={len(entries)}")


if __name__ == "__main__":
    main()

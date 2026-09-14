#!/usr/bin/env python3
"""Build frozen dataset_inventory_v1.json. Not a proof. Not KernelInput."""
from __future__ import annotations

import importlib.util
import json
import sys
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parents[2]
BRIDGE = ROOT / "audit" / "formal_bridge"
OUT = BRIDGE / "dataset_inventory_v1.json"
GEN = ROOT / "tools" / "formal_bridge" / "generate_kernel_candidate.py"

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

A_THEOREM = {
    "valid_block": "validation_soundness",
    "bad_nbits": "pow_soundness",
    "invalid_tx": "validation_soundness",
    "double_spend": "UTXOState.input_lookup_sound",
    "invalid_witness": "WitnessValidation.sound",
}

B_THEOREM = {
    "connect": "connect_disconnect_inverse",
    "disconnect": "connect_disconnect_inverse",
    "restart": "state_transition_correctness",
    "reorg": "chain_selection",
}


def load_gen() -> Any:
    spec = importlib.util.spec_from_file_location("kigen", GEN)
    mod = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(mod)
    return mod


def candidate_hash_of(path: Path, gen: Any) -> str | None:
    if not path.is_file():
        return None
    data = json.loads(path.read_text())
    fields = {k: data.get(k) for k in gen.KI_FIELDS}
    return gen.candidate_hash(fields)


def load_json(path: Path) -> dict:
    return json.loads(path.read_text())


def consensus_events(gen: Any) -> list[dict]:
    events: list[dict] = []
    trial = BRIDGE / "local_trial_dataset"
    for name in (
        "A_valid_block.json",
        "A_bad_nbits.json",
        "A_invalid_tx.json",
        "A_double_spend.json",
        "A_invalid_witness.json",
    ):
        rec = load_json(trial / name)
        events.append(
            {
                "event_id": rec.get("evidence_id") or name,
                "source": "local_trial_dataset",
                "scenario": rec.get("scenario"),
                "validation_layer": rec.get("rejection_layer"),
                "kernel_candidate_hash": rec.get("kernel_candidate_hash"),
                "future_theorem_candidate": A_THEOREM.get(
                    rec.get("scenario", ""), rec.get("future_theorem")
                ),
            }
        )

    for i in range(1, 14):
        ce = f"CE-{i:03d}"
        rec = load_json(BRIDGE / "counterexamples" / f"{ce}.json")
        vl = rec.get("validation_layer") or {}
        kh = candidate_hash_of(
            BRIDGE / "kernel_candidate_dataset" / ce / "kernel_candidate.json",
            gen,
        )
        events.append(
            {
                "event_id": ce,
                "source": "attack_vectors",
                "scenario": rec.get("attack_type"),
                "validation_layer": vl.get("function") or rec.get("cpp_validation_function"),
                "kernel_candidate_hash": kh,
                "future_theorem_candidate": CE_FUTURE[ce],
            }
        )

    live = BRIDGE / "kernel_candidate_dataset" / "live_connectblock"
    if live.is_dir():
        for cand in sorted(live.glob("*/kernel_candidate.json")):
            parent = cand.parent.name
            events.append(
                {
                    "event_id": f"live_connectblock/{parent}",
                    "source": "kernel_candidate_dataset",
                    "scenario": "live_connectblock",
                    "validation_layer": "ConnectBlock",
                    "kernel_candidate_hash": candidate_hash_of(cand, gen),
                    "future_theorem_candidate": "validation_soundness",
                }
            )
    return events


def state_events() -> list[dict]:
    events: list[dict] = []
    trial = BRIDGE / "local_trial_dataset"
    mapping = {
        "B_connect_valid.json": ("connect", "ConnectBlock"),
        "B_disconnect_invalidate.json": ("disconnect", "DisconnectBlock"),
        "B_reconnect_reconsider.json": ("connect", "ConnectBlock"),
        "B_restart.json": ("restart", "LoadChainstate"),
        "B_reorg_longest.json": ("reorg", "ActivateBestChain"),
    }
    for name, (op, layer) in mapping.items():
        rec = load_json(trial / name)
        events.append(
            {
                "event_id": rec.get("evidence_id") or name,
                "source": "local_trial_dataset",
                "scenario": op,
                "validation_layer": layer,
                "kernel_candidate_hash": None,
                "future_theorem_candidate": B_THEOREM[op],
                "operation": rec.get("operation") or op,
            }
        )

    st_index = load_json(BRIDGE / "state_transition_dataset" / "index.json")
    st_map = {
        "ST-001": ("connect", "connect_disconnect_inverse"),
        "ST-002": ("disconnect", "connect_disconnect_inverse"),
        "ST-003": ("connect", "connect_disconnect_inverse"),
        "ST-004": ("connect", "state_transition_correctness"),
        "ST-005": ("restart", "state_transition_correctness"),
    }
    for case in st_index.get("cases", []):
        sid = case["id"]
        op, th = st_map[sid]
        if sid == "ST-004":
            op = "connect"
        events.append(
            {
                "event_id": sid,
                "source": "state_transition_dataset",
                "scenario": op,
                "validation_layer": case.get("direction") or case.get("kind"),
                "kernel_candidate_hash": None,
                "future_theorem_candidate": th,
                "operation": op,
            }
        )
    return events


def serialization_events() -> list[dict]:
    events: list[dict] = []
    cases = ROOT / "tests" / "serialization_vectors" / "cases"
    for path in sorted(cases.glob("SV-*.json")):
        rec = load_json(path)
        cid = rec.get("case_id") or path.stem
        leftover = False
        ser = rec.get("serialization") or rec.get("decode_result") or {}
        if cid == "SV-TRAILING-BYTE-AFTER-BLOCK":
            leftover = True
        events.append(
            {
                "event_id": cid,
                "source": "serialization_vectors",
                "scenario": rec.get("kind") or "decode",
                "validation_layer": "DecodeHexBlk / UnserializeTransaction",
                "kernel_candidate_hash": None,
                "future_theorem_candidate": (
                    "object_identity" if leftover else "canonical_decode"
                ),
            }
        )

    for ce in ("CE-008",):
        events.append(
            {
                "event_id": ce,
                "source": "attack_vectors",
                "scenario": "leftover_decode",
                "validation_layer": "DecodeHexBlk",
                "kernel_candidate_hash": None,
                "future_theorem_candidate": "object_identity",
            }
        )
    for i in range(1, 14):
        if i == 8:
            continue
        ce = f"CE-{i:03d}"
        events.append(
            {
                "event_id": ce,
                "source": "attack_vectors",
                "scenario": "ce_payload_decode",
                "validation_layer": "UnserializeTransaction / DecodeHexBlk",
                "kernel_candidate_hash": None,
                "future_theorem_candidate": "canonical_decode",
            }
        )
    return events


def corpus_counts() -> dict:
    traces = BRIDGE / "kernel_candidate_dataset" / "traces.jsonl"
    index = BRIDGE / "kernel_candidate_dataset" / "index.jsonl"
    av = ROOT / "tests" / "attack_vectors" / "traces"
    return {
        "kernel_candidate_traces_jsonl": sum(1 for _ in traces.open()) if traces.is_file() else 0,
        "kernel_candidate_index_jsonl": sum(1 for _ in index.open()) if index.is_file() else 0,
        "attack_vector_trace_files": len(list(av.glob("CE-*.json"))) if av.is_dir() else 0,
        "serialization_vector_cases": len(
            list((ROOT / "tests" / "serialization_vectors" / "cases").glob("SV-*.json"))
        ),
        "local_trial_evidence_files": 13,
    }


def build() -> dict:
    gen = load_gen()
    return {
        "schema_version": "dataset_inventory_v1",
        "status": "FROZEN",
        "note": (
            "Engineering inventory only. KernelInputCandidate is not KernelInput. "
            "Evidence is not a proof. Tests are not theorems. "
            "CPP_KERNELINPUT_EQUIVALENCE = NOT PROVED"
        ),
        "baseline_head": "0211fec04553b0155a4badf6204d158338560b1d",
        "corpus": corpus_counts(),
        "consensus_dataset": {
            "sources": [
                "kernel_candidate_dataset/",
                "local_trial_dataset/",
                "attack_vectors/",
            ],
            "events": consensus_events(gen),
        },
        "state_dataset": {
            "operations": ["connect", "disconnect", "restart", "reorg"],
            "future_theorems": {
                "connect": "connect_disconnect_inverse",
                "disconnect": "connect_disconnect_inverse",
                "restart": "state_transition_correctness",
                "reorg": "chain_selection",
            },
            "events": state_events(),
        },
        "serialization_dataset": {
            "sources": ["serialization_vectors", "CE-001..CE-013"],
            "future_theorems": {
                "decode": "canonical_decode",
                "leftover": "object_identity",
            },
            "events": serialization_events(),
        },
    }


def main() -> int:
    data = build()
    OUT.write_text(json.dumps(data, indent=2, sort_keys=False) + "\n")
    print(f"wrote {OUT.relative_to(ROOT)}")
    print(f"consensus_events={len(data['consensus_dataset']['events'])}")
    print(f"state_events={len(data['state_dataset']['events'])}")
    print(f"serialization_events={len(data['serialization_dataset']['events'])}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

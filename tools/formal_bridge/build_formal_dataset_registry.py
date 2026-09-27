#!/usr/bin/env python3
"""Build formal_dataset_registry_v1.json. Observation registry. Not theorems."""
from __future__ import annotations

import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "formal_bridge"))
from formal_contract_hashes import dataset_hash, schema_hash  # noqa: E402

BRIDGE = ROOT / "audit" / "formal_bridge"
OUT = BRIDGE / "formal_dataset_registry_v1.json"

CE_THEOREM = {
    "CE-001": "utxo_lookup_sound",
    "CE-002": "utxo_lookup_sound",
    "CE-003": "connect_disconnect_inverse",
    "CE-004": "amount_arithmetic_sound",
    "CE-005": "script_validation_sound",
    "CE-006": "script_validation_sound",
    "CE-007": "pow_validation_sound",
    "CE-008": "canonical_decode",
    "CE-009": "utxo_lookup_sound",
    "CE-010": "script_validation_sound",
    "CE-011": "pow_validation_sound",
    "CE-012": "asert_boundary",
    "CE-013": "amount_arithmetic_sound",
}

DIFF_THEOREM = {
    "normal_block": "node_determinism",
    "multi_tx_block": "node_determinism",
    "connect_disconnect": "connect_disconnect_inverse",
    "restart_after_block": "persistence_correctness",
    "reorg": "chain_selection",
    "double_spend": "utxo_lookup_sound",
    "missing_input": "utxo_lookup_sound",
    "overflow_amount": "amount_arithmetic_sound",
    "invalid_script": "script_validation_sound",
    "invalid_witness": "script_validation_sound",
    "bad_merkle": "canonical_decode",
    "bad_timestamp": "pow_validation_sound",
    "bad_nbits": "pow_validation_sound",
    "invalid_prev_hash": "canonical_decode",
    "orphan_block": "chain_selection",
    "competing_fork": "chain_selection",
}

DIFF_CLASS = {
    "restart_after_block": "B",
    "connect_disconnect": "B",
    "reorg": "B",
    "competing_fork": "B",
}

EV_CLASS = {
    "consensus": "A",
    "state_transition": "B",
    "environment": "C",
    "network": "C",
}


def row(
    eid: str,
    source: str,
    cls: str,
    inp: str,
    layer: str,
    theorem: str,
) -> dict:
    return {
        "id": eid,
        "source": source,
        "class": cls,
        "input": inp,
        "expected_layer": layer or "",
        "future_theorem": theorem,
        "status": "OBSERVED",
    }


def load_json(path: Path) -> dict:
    return json.loads(path.read_text())


def sv_theorem(rec: dict) -> str:
    raw = str(rec.get("future_theorem") or "")
    layer = str((rec.get("validation_rule") or {}).get("layer") or "")
    fn = str((rec.get("validation_rule") or {}).get("function") or "")
    cid = str(rec.get("case_id") or "")
    if "compactsize" in raw.lower() or "COMPACTSIZE" in cid:
        return "compactsize_unique"
    if "canonical" in raw.lower() or cid.startswith("SV-TRAILING") or "TRUNCATED" in cid:
        return "canonical_decode"
    if "pow" in raw.lower() or "CheckProofOfWork" in fn or "nbits" in cid.lower():
        return "pow_validation_sound"
    if "asert" in raw.lower() or "GetNextWork" in fn:
        return "asert_boundary"
    if "wrap" in raw.lower() or "MAX_MONEY" in cid or "overflow" in cid.lower() or "negative" in cid.lower():
        return "amount_arithmetic_sound"
    if "script" in raw.lower() or "witness" in cid.lower() or cid.startswith("SV-B2") or cid.startswith("SV-B3"):
        return "script_validation_sound"
    if "double" in cid.lower() or cid.startswith("SV-A1"):
        return "utxo_lookup_sound"
    if "merkle" in cid.lower():
        return "canonical_decode"
    if layer == "serialization" or fn in {"DecodeHexBlk", "UnserializeTransaction"}:
        return "canonical_decode"
    return raw or "canonical_decode"


def build() -> dict:
    entries: list[dict] = []

    av = ROOT / "tests" / "attack_vectors" / "traces"
    for path in sorted(av.glob("*.json")):
        rec = load_json(path)
        cid = path.stem
        base = cid.split("_")[0] if cid.startswith("CE-") else cid
        th = CE_THEOREM.get(base, rec.get("future_theorem") or "validation_soundness")
        layer = rec.get("expected_rejection_point") or rec.get("rejection_layer") or ""
        if isinstance(rec.get("after"), dict):
            layer = rec["after"].get("rejection_layer") or layer
        cls = "B" if base == "CE-003" else "A"
        entries.append(
            row(
                f"/{cid}",
                "tests/attack_vectors/",
                cls,
                str(path.relative_to(ROOT)),
                str(layer),
                str(th),
            )
        )

    svdir = ROOT / "tests" / "serialization_vectors" / "cases"
    for path in sorted(svdir.glob("SV-*.json")):
        rec = load_json(path)
        cid = rec.get("case_id") or path.stem
        rule = rec.get("validation_rule") or {}
        entries.append(
            row(
                f"/{cid}",
                "tests/serialization_vectors/",
                "A",
                str(path.relative_to(ROOT)),
                str(rule.get("function") or rule.get("layer") or ""),
                sv_theorem(rec),
            )
        )

    entries.append(
        row(
            "/kernel_candidate_traces",
            "audit/formal_bridge/kernel_candidate_dataset/",
            "A",
            "audit/formal_bridge/kernel_candidate_dataset/traces.jsonl",
            "ConnectBlock",
            "node_determinism",
        )
    )
    kcds = BRIDGE / "kernel_candidate_dataset"
    for path in sorted(kcds.glob("CE-*/kernel_candidate.json")):
        ce = path.parent.name
        key = ce[:6] if ce.startswith("CE-") else ce
        th = CE_THEOREM.get(key, "validation_soundness")
        entries.append(
            row(
                f"/{ce}",
                "audit/formal_bridge/kernel_candidate_dataset/",
                "B" if key == "CE-003" else "A",
                str(path.relative_to(ROOT)),
                "generate_kernel_candidate",
                th,
            )
        )
    for path in sorted((kcds / "live_connectblock").glob("*/kernel_candidate.json")):
        entries.append(
            row(
                f"/live/{path.parent.name}",
                "audit/formal_bridge/kernel_candidate_dataset/",
                "A",
                str(path.relative_to(ROOT)),
                "ConnectBlock",
                "pow_validation_sound" if "reject" in path.parent.name else "node_determinism",
            )
        )

    trial = BRIDGE / "local_trial_dataset"
    for path in sorted(trial.glob("*.json")):
        if path.name == "index.json":
            continue
        rec = load_json(path)
        cls = EV_CLASS.get(str(rec.get("class")), "A")
        if path.name.startswith("A_"):
            cls = "A"
        elif path.name.startswith("B_"):
            cls = "B"
        elif path.name.startswith("C_"):
            cls = "C"
        th = rec.get("future_theorem") or (
            "chain_selection"
            if cls == "B" and "reorg" in path.name
            else "validation_soundness"
            if cls == "A"
            else "state_transition_correctness"
            if cls == "B"
            else "network assumption"
        )
        if cls == "C":
            th = "network assumption"
        layer = rec.get("rejection_layer") or rec.get("operation") or ""
        entries.append(
            row(
                f"/{path.stem}",
                "audit/formal_bridge/local_trial_dataset/",
                cls,
                rec.get("evidence_id") or str(path.relative_to(ROOT)),
                str(layer),
                str(th),
            )
        )
    stdir = BRIDGE / "state_transition_dataset"
    st_index = load_json(stdir / "index.json")
    st_th = {
        "ST-001": "connect_disconnect_inverse",
        "ST-002": "connect_disconnect_inverse",
        "ST-003": "connect_disconnect_inverse",
        "ST-004": "pow_validation_sound",
        "ST-005": "persistence_correctness",
    }
    for case in st_index.get("cases", []):
        sid = case["id"]
        entries.append(
            row(
                f"/{sid}",
                "audit/formal_bridge/state_transition_dataset/",
                "B",
                sid,
                str(case.get("direction") or ""),
                st_th.get(sid, "state_transition_correctness"),
            )
        )

    cases = BRIDGE / "differential_dataset" / "cases.jsonl"
    for line in cases.read_text().splitlines():
        if not line.strip():
            continue
        rec = json.loads(line)
        sc = rec.get("scenario") or ""
        kind = rec.get("kind")
        cls = DIFF_CLASS.get(sc, "A")
        th = rec.get("future_theorem") or DIFF_THEOREM.get(sc, "node_determinism")
        if sc in DIFF_THEOREM:
            th = DIFF_THEOREM[sc]
        layer = rec.get("reject_layer_a") or rec.get("node_a_decision") or ""
        if kind == "valid" and sc in {"normal_block", "multi_tx_block"}:
            layer = "ConnectBlock"
        entries.append(
            row(
                f"/{rec.get('case_id')}",
                "audit/formal_bridge/differential_dataset/",
                cls,
                rec.get("input_hash") or rec.get("case_id"),
                str(layer),
                str(th),
            )
        )

    ids = [e["id"] for e in entries]
    if len(ids) != len(set(ids)):
        raise SystemExit("duplicate registry id")

    return {
        "schema_version": "formal_dataset_registry_v1",
        "status": "OBSERVED",
        "note": (
            "Unified observation registry. Not KernelInput. Not a proof. "
            "CPP_KERNELINPUT_EQUIVALENCE = NOT PROVED. "
            "KernelInputCandidate != KernelInput. Evidence != Proof. Dataset != Theorem."
        ),
        "meta": {
            "dataset_hash": dataset_hash(),
            "schema_hash": schema_hash(),
            "n_entries": len(entries),
            "sources": [
                " attack vectors",
                " serialization vectors",
                " bridge dataset",
                " evidence dataset",
                " differential dataset",
            ],
        },
        "entries": entries,
    }


def main() -> int:
    data = build()
    OUT.write_text(json.dumps(data, indent=2) + "\n")
    print(f"wrote {OUT.relative_to(ROOT)} n={data['meta']['n_entries']}")
    print(f"dataset_hash={data['meta']['dataset_hash']}")
    print(f"schema_hash={data['meta']['schema_hash']}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

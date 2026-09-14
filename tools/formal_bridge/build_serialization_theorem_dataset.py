#!/usr/bin/env python3
"""Build serialization theorem dataset + CE serialization views. Not a proof."""
from __future__ import annotations

import hashlib
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BRIDGE = ROOT / "audit" / "formal_bridge"
OUT = BRIDGE / "serialization_theorem_dataset_v1.json"
CE_DIR = BRIDGE / "serialization_counterexamples_v1"

CE_FUTURE = {
    "CE-001": "transaction_object_sound",
    "CE-002": "transaction_object_sound",
    "CE-003": "transaction_object_sound",
    "CE-004": "transaction_object_sound",
    "CE-005": "transaction_object_sound",
    "CE-006": "witness_validation_sound",
    "CE-007": "block_object_sound",
    "CE-008": "canonical_decode_unique",
    "CE-009": "transaction_object_sound",
    "CE-010": "witness_validation_sound",
    "CE-011": "block_object_sound",
    "CE-012": "block_object_sound",
    "CE-013": "transaction_object_sound",
}
CE_LAYER = {
    "CE-001": "transaction",
    "CE-002": "transaction",
    "CE-003": "identity",
    "CE-004": "transaction",
    "CE-005": "transaction",
    "CE-006": "witness",
    "CE-007": "decode",
    "CE-008": "identity",
    "CE-009": "transaction",
    "CE-010": "witness",
    "CE-011": "decode",
    "CE-012": "decode",
    "CE-013": "transaction",
}
SE_FUTURE = {
    "SE-001": "compactsize_canonical",
    "SE-002": "canonical_decode_unique",
    "SE-003": "txid_witness_independence",
    "SE-004": "canonical_decode_unique",
    "SE-005": "merkle_root_correct",
}

DIFF_LAYER = {
    "invalid_witness": "witness",
    "overflow_amount": "transaction",
    "invalid_script": "transaction",
    "double_spend": "transaction",
    "missing_input": "transaction",
    "bad_merkle": "identity",
    "invalid_prev_hash": "identity",
    "orphan_block": "identity",
    "competing_fork": "identity",
    "reorg": "identity",
    "normal_block": "identity",
    "multi_tx_block": "identity",
    "connect_disconnect": "identity",
    "restart_after_block": "identity",
    "bad_timestamp": "decode",
    "bad_nbits": "decode",
}
DIFF_TH = {
    "invalid_witness": "witness_validation_sound",
    "overflow_amount": "transaction_object_sound",
    "invalid_script": "transaction_object_sound",
    "double_spend": "transaction_object_sound",
    "missing_input": "transaction_object_sound",
    "bad_merkle": "merkle_root_correct",
    "invalid_prev_hash": "block_object_sound",
    "orphan_block": "block_object_sound",
    "competing_fork": "block_object_sound",
    "reorg": "block_object_sound",
    "normal_block": "encode_decode_identity",
    "multi_tx_block": "encode_decode_identity",
    "connect_disconnect": "encode_decode_identity",
    "restart_after_block": "encode_decode_identity",
    "bad_timestamp": "block_object_sound",
    "bad_nbits": "block_object_sound",
}


def sha256_hex(s: str) -> str:
    return hashlib.sha256(s.encode("utf-8")).hexdigest()


def sv_map(rec: dict) -> tuple[str, str, str]:
    cid = rec.get("case_id") or ""
    dec = rec.get("decode_result") or {}
    status = dec.get("status") or rec.get("consensus_result")
    fut = rec.get("future_theorem") or ""
    if "COMPACTSIZE" in cid:
        return "decode", str(status), "compactsize_canonical"
    if "TRAILING" in cid:
        return "identity", str(status), "canonical_decode_unique"
    if "TRUNCATED" in cid or cid.startswith("SV-C1"):
        return "decode", str(status), "canonical_decode_unique"
    if "SUPERFLUOUS" in cid or "WITNESS" in cid or cid.startswith("SV-B3"):
        return "witness", str(status), "txid_witness_independence"
    if "merkle" in cid.lower():
        return "identity", str(rec.get("consensus_result")), "merkle_root_correct"
    if rec.get("kind") == "tx":
        obs = rec.get("consensus_result") or status
        if dec.get("status") == "ok":
            return "transaction", str(obs), "encode_decode_identity"
        return "decode", str(obs), "canonical_decode_unique"
    obs = rec.get("consensus_result") or status
    if dec.get("roundtrip_equal") is True:
        return "identity", str(obs), "encode_decode_identity"
    return "decode", str(obs), fut or "canonical_decode_unique"


def rec_row(eid: str, payload: str, layer: str, observed: str, theorem: str) -> dict:
    return {
        "id": eid,
        "input_hash": sha256_hex(payload),
        "layer": layer,
        "observed_result": observed,
        "future_theorem": theorem,
    }


def write_ce_views() -> None:
    CE_DIR.mkdir(parents=True, exist_ok=True)
    src_dir = BRIDGE / "counterexamples"
    for i in range(1, 14):
        ce = f"CE-{i:03d}"
        src = json.loads((src_dir / f"{ce}.json").read_text())
        ser = src.get("serialization_layer") or {}
        val = src.get("validation_layer") or {}
        raw = src.get("raw_input") or {}
        hexes = raw.get("hexes") or raw.get("hex") or ""
        if isinstance(hexes, list):
            blob = "|".join(str(x) for x in hexes)
        else:
            blob = str(hexes or json.dumps(raw, sort_keys=True))
        decode = ser.get("status") or "ok"
        cpp = val.get("decision") or src.get("failure_reason") or ""
        layer = CE_LAYER[ce]
        row = {
            "id": ce,
            "title": src.get("attack_type") or ce,
            "input_bytes": blob[:4000],
            "source": f"audit/formal_bridge/counterexamples/{ce}.json",
            "decode_result": str(decode),
            "cpp_behavior": f"{val.get('function') or src.get('cpp_validation_function')}: {cpp}",
            "future_theorem_target": CE_FUTURE[ce],
            "layer": layer,
            "note": "Serialization view of an existing CE. Not a theorem.",
        }
        (CE_DIR / f"{ce}.json").write_text(json.dumps(row, indent=2) + "\n")


def build() -> dict:
    write_ce_views()
    entries: list[dict] = []

    svdir = ROOT / "tests" / "serialization_vectors" / "cases"
    for path in sorted(svdir.glob("SV-*.json")):
        rec = json.loads(path.read_text())
        hx = rec.get("raw_block_hex") or rec.get("raw_tx_hex") or rec.get("case_id")
        layer, obs, th = sv_map(rec)
        entries.append(rec_row(f"SV/{rec.get('case_id') or path.stem}", str(hx), layer, obs, th))

    for i in range(1, 14):
        ce = f"CE-{i:03d}"
        view = json.loads((CE_DIR / f"{ce}.json").read_text())
        src = json.loads((BRIDGE / "counterexamples" / f"{ce}.json").read_text())
        val = src.get("validation_layer") or {}
        obs = val.get("decision") or view.get("decode_result")
        entries.append(
            rec_row(
                f"CE/{ce}",
                view.get("input_bytes") or ce,
                CE_LAYER[ce],
                str(obs),
                CE_FUTURE[ce],
            )
        )

    for i in range(1, 6):
        se = f"SE-{i:03d}"
        view = json.loads((CE_DIR / f"{se}.json").read_text())
        entries.append(
            rec_row(
                f"SE/{se}",
                view.get("input_bytes") or se,
                view.get("layer") or "decode",
                view.get("decode_result") or "",
                SE_FUTURE[se],
            )
        )

    cases = BRIDGE / "differential_dataset" / "cases.jsonl"
    for line in cases.read_text().splitlines():
        if not line.strip():
            continue
        rec = json.loads(line)
        sc = rec.get("scenario") or ""
        cid = rec.get("case_id")
        layer = DIFF_LAYER.get(sc, "identity")
        th = DIFF_TH.get(sc, "encode_decode_identity")
        obs = rec.get("node_a_decision") or ""
        payload = rec.get("input_hash") or cid
        entries.append(rec_row(f"DIFF/{cid}", str(payload), layer, str(obs), th))

    layers = {e["layer"] for e in entries}
    return {
        "schema_version": "serialization_theorem_dataset_v1",
        "status": "REGISTERED",
        "note": (
            "Observed codec/validation rows. Not theorems. "
            "KernelInputCandidate != KernelInput. "
            "CPP_KERNELINPUT_EQUIVALENCE = NOT PROVED"
        ),
        "n_entries": len(entries),
        "layers": sorted(layers),
        "sources": [
            "TASK-005 serialization vectors",
            "TASK-008/011 CE dataset",
            "TASK-012 differential dataset",
            "TASK-014 SE-001..SE-005",
        ],
        "entries": entries,
    }


def write_index() -> None:
    ids = [f"CE-{i:03d}" for i in range(1, 14)] + [f"SE-{i:03d}" for i in range(1, 6)]
    (CE_DIR / "index.json").write_text(
        json.dumps(
            {
                "status": "COMPLETE",
                "ids": ids,
                "note": "Each file is an observation. Not a Coq theorem.",
            },
            indent=2,
        )
        + "\n"
    )


def main() -> int:
    data = build()
    write_index()
    OUT.write_text(json.dumps(data, indent=2) + "\n")
    print(f"wrote {OUT.relative_to(ROOT)} n={data['n_entries']} layers={data['layers']}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""Regenerate the twelve formal_verification issuance golden JSONs from RFC-020.

Numeric SSOT: tools/res_continuous_price_schedule.py
"""
from __future__ import annotations

import json
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "tools"))
from res_continuous_price_schedule import (  # noqa: E402
    block_subsidy_atoms,
    destroy_cost_atoms,
)

FV = REPO / "formal_verification"


def row(height: int) -> dict:
    return {
        "height": height,
        "epoch": height // 210000,
        "oru": block_subsidy_atoms(height),
        "oru_subsidy": block_subsidy_atoms(height),
        "res_destroy_cost": destroy_cost_atoms(height),
        "res_schedule": destroy_cost_atoms(height),
        "res_issued": 0,
    }


def dump(path: Path, doc: dict) -> None:
    path.write_text(json.dumps(doc, indent=2) + "\n", encoding="utf-8")
    print(f"wrote {path.relative_to(REPO)}")


def pin_issuance_list(items: list, height_key: str = "height") -> None:
    for item in items:
        if not isinstance(item, dict) or height_key not in item:
            continue
        h = int(item[height_key])
        r = row(h)
        if "oru" in item:
            item["oru"] = r["oru"]
        if "oru_subsidy" in item:
            item["oru_subsidy"] = r["oru_subsidy"]
        if "res_destroy_cost" in item:
            item["res_destroy_cost"] = r["res_destroy_cost"]
        if "res_schedule" in item:
            item["res_schedule"] = r["res_schedule"]
        if "ResDestroyCost" in item:
            item["ResDestroyCost"] = r["res_destroy_cost"]
        if "res_issued" in item:
            item["res_issued"] = 0
        if "epoch" in item:
            item["epoch"] = r["epoch"]


def main() -> int:
    # 1. alignment
    p = FV / "consensus_kernel_alignment_vectors.json"
    doc = json.loads(p.read_text(encoding="utf-8"))
    doc["note"] = (
        "RFC-020 alignment vectors. Display destroy_cost(h) is not actual issuance. "
        "Does not modify phase3_golden_vectors.json."
    )
    by_id = {v["id"]: v for v in doc["vectors"]}
    by_id["P049-G0-RES-SCHEDULE"]["fact"] = (
        "Genesis RES destroy cost is destroy_cost(0), not actual issuance"
    )
    by_id["P049-H1-ORU"]["get_block_subsidy_oru"] = 128
    by_id["P049-H1-ORU"]["fact"] = "height 1 ORU issuance is the flat 128 ORU subsidy"
    dump(p, doc)

    # 2. bridge
    p = FV / "consensus_kernel_bridge_vectors.json"
    doc = json.loads(p.read_text(encoding="utf-8"))
    pin_issuance_list(doc["vectors"])
    dump(p, doc)

    # 3. C ABI
    p = FV / "consensus_kernel_c_abi_vectors.json"
    doc = json.loads(p.read_text(encoding="utf-8"))
    pin_issuance_list(doc["issuance"])
    dump(p, doc)

    # 4. refinement
    p = FV / "consensus_kernel_refinement_vectors.json"
    doc = json.loads(p.read_text(encoding="utf-8"))
    pin_issuance_list(doc["issuance"])
    for item in doc["issuance"]:
        item["res_issued"] = 0
        item["schedule_is_not_issuance"] = True
    dump(p, doc)

    # 5. adversarial
    p = FV / "adversarial_security_vectors.json"
    doc = json.loads(p.read_text(encoding="utf-8"))
    doc["refinement_proved"] = False
    pin_issuance_list(doc["issuance"])
    dump(p, doc)

    # 6. utxo state
    p = FV / "utxo_state_transition_vectors.json"
    doc = json.loads(p.read_text(encoding="utf-8"))
    doc["refinement_proved"] = False
    doc["genesis"]["res_schedule"] = destroy_cost_atoms(0)
    pin_issuance_list(doc["epoch"])
    dump(p, doc)

    # 7. ledger
    p = FV / "ledger_transition_vectors.json"
    doc = json.loads(p.read_text(encoding="utf-8"))
    doc["refinement_proved"] = False
    doc["genesis"]["res_schedule"] = destroy_cost_atoms(0)
    dump(p, doc)

    # 8. kernel migration
    p = FV / "kernel_migration_vectors.json"
    doc = json.loads(p.read_text(encoding="utf-8"))
    doc["refinement_proved"] = False
    doc["genesis"]["res_schedule"] = destroy_cost_atoms(0)
    dump(p, doc)

    # 9. canary
    p = FV / "kernel_canary_vectors.json"
    doc = json.loads(p.read_text(encoding="utf-8"))
    doc["refinement_proved"] = False
    doc["formal_refinement_status"] = "NOT_PROVED"
    doc["genesis"]["res_destroy_cost"] = destroy_cost_atoms(0)
    dump(p, doc)

    # 10. genesis consensus
    p = FV / "genesis_consensus_vectors.json"
    doc = json.loads(p.read_text(encoding="utf-8"))
    doc["note"] = (
        "Frozen genesis consensus pins for RFC-020 continuous destroy_cost(h). "
        "Does not prove C++ = Coq = Python. No automatic RES mint."
    )
    doc["genesis"]["ResDestroyCost"] = destroy_cost_atoms(0)
    pin_issuance_list(doc["height"])
    dump(p, doc)

    # 11. freeze
    p = FV / "consensus_kernel_freeze_vectors.json"
    doc = json.loads(p.read_text(encoding="utf-8"))
    doc["genesis"]["res_destroy_cost"] = destroy_cost_atoms(0)
    pin_issuance_list(doc["epoch"])
    for item in doc["epoch"]:
        item["res_issued"] = 0
    dump(p, doc)

    # 12. release: RFC-020 destroy_cost, not lottery W(h)
    p = FV / "release_consensus_vectors.json"
    doc = json.loads(p.read_text(encoding="utf-8"))
    doc["genesis"].pop("res_schedule", None)
    doc["genesis"]["res_destroy_cost"] = destroy_cost_atoms(0)
    for item in doc.get("epoch") or []:
        if isinstance(item, dict):
            item.pop("res_schedule", None)
            item["res_destroy_cost"] = destroy_cost_atoms(int(item["height"]))
    dump(p, doc)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())

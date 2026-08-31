#!/usr/bin/env python3
"""P049 alignment inventory checks. Does not reimplement issuance or draw.

Fails if honesty flags are flipped, if schedule is collapsed into actual
issuance, or if the alignment JSON claims equivalence.
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
ALIGN = REPO / "formal_verification" / "consensus_kernel_alignment.json"
VECTORS = REPO / "formal_verification" / "consensus_kernel_alignment_vectors.json"


def main() -> int:
    align = json.loads(ALIGN.read_text(encoding="utf-8"))
    vecs = json.loads(VECTORS.read_text(encoding="utf-8"))
    errors: list[str] = []

    for key in ("refinement_proved", "equivalence_proved", "equivalence_claimed"):
        if align.get(key) is True:
            errors.append(f"{ALIGN.name}: {key} must stay false")
        if vecs.get(key) is True:
            errors.append(f"{VECTORS.name}: {key} must stay false")

    if align.get("cpp_production_modified") is True:
        errors.append("cpp_production_modified must be false")

    genesis = align.get("genesis") or {}
    if genesis.get("schedule_vs_issuance") != "DISTINGUISHED":
        errors.append("genesis.schedule_vs_issuance must be DISTINGUISHED")
    if genesis.get("status") == "DISCREPANCY":
        errors.append("genesis.status is DISCREPANCY")

    required = (
        "epoch",
        "oru_issuance",
        "res_issuance",
        "genesis",
        "maturity",
        "draw",
        "classification",
        "supply_delta",
        "coinbase_layout",
    )
    for name in required:
        block = align.get(name) or {}
        if block.get("status") not in ("ALIGNED", "DISCREPANCY"):
            errors.append(f"{name}.status missing or invalid")
        if "equivalence" in block:
            errors.append(f"{name} must not contain an equivalence field")

    by_id = {v["id"]: v for v in vecs.get("vectors", [])}
    for vid in (
        "P049-G0-ORU",
        "P049-G0-RES-ACTUAL",
        "P049-G0-RES-SCHEDULE",
        "P049-G0-DRAW",
        "P049-G0-DELTA",
        "P049-H1-ORU",
        "P049-H100-MATURITY",
        "P049-H101-MATURITY",
        "P049-H209999-EPOCH",
        "P049-H210000-EPOCH",
        "P049-ORU-RES-EPOCH",
    ):
        if vid not in by_id:
            errors.append(f"missing vector {vid}")

    g0s = by_id.get("P049-G0-RES-SCHEDULE", {})
    if g0s.get("get_res_mint_reward") != 40000000:
        errors.append("P049-G0-RES-SCHEDULE schedule value drifted")
    if g0s.get("res_issued") != 0:
        errors.append("P049-G0-RES-SCHEDULE actual issuance must stay 0")
    if not g0s.get("schedule_is_not_actual"):
        errors.append("schedule_is_not_actual must be true")

    if by_id.get("P049-G0-ORU", {}).get("get_block_subsidy") != 0:
        errors.append("P049-G0-ORU must be 0")
    if by_id.get("P049-H209999-EPOCH", {}).get("epoch") != 0:
        errors.append("epoch(209999) must be 0")
    if by_id.get("P049-H210000-EPOCH", {}).get("epoch") != 1:
        errors.append("epoch(210000) must be 1")
    if by_id.get("P049-H100-MATURITY", {}).get("mature") is not False:
        errors.append("height 100 must be immature for height-1 coinbase")
    if by_id.get("P049-H101-MATURITY", {}).get("mature") is not True:
        errors.append("height 101 must be mature for height-1 coinbase")

    if errors:
        print("FAIL: consensus kernel alignment inventory")
        for e in errors:
            print(f"  - {e}")
        return 1
    print("PASS: consensus kernel alignment inventory (not a refinement)")
    return 0


if __name__ == "__main__":
    sys.exit(main())

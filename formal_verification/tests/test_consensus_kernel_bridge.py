#!/usr/bin/env python3
"""P052 bridge inventory + vector pins. Not a refinement.

If extracted CLIs exist, compares OCaml ↔ C++ on the bridge heights.
Python is not consensus authority.
"""

from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
FV = REPO / "formal_verification"
BRIDGE = FV / "consensus_kernel_cpp_bridge.json"
VECTORS = FV / "consensus_kernel_bridge_vectors.json"
BOUNDARY = FV / "kernel_bridge_boundary.json"
OCAML = FV / "extracted" / "consensus_kernel_cli"
CPP = FV / "extracted" / "cpp_consensus_kernel_cli"

HEIGHTS = [0, 1, 99, 100, 101, 209999, 210000, 210001, 419999, 420000]


def load(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def run(cli: Path, *args: str) -> str:
    r = subprocess.run([str(cli), *args], check=True, capture_output=True, text=True)
    return r.stdout.strip()


def main() -> int:
    errors: list[str] = []
    for path in (BRIDGE, VECTORS, BOUNDARY):
        if not path.is_file():
            print(f"FAIL: missing {path}", file=sys.stderr)
            return 1
        doc = load(path)
        for key in ("refinement_proved", "equivalence_proved"):
            if doc.get(key) is True:
                errors.append(f"{path.name}: {key} must stay false")

    br = load(BRIDGE)
    if br.get("bridge_status") != "CLOSED":
        errors.append("bridge_status must be CLOSED")
    if br.get("production_consensus_modified") is not False:
        errors.append("production_consensus_modified must be false")
    if br.get("issuance", {}).get("status") != "mapped":
        errors.append("issuance.status")
    if br.get("supply", {}).get("status") != "layer_aligned":
        errors.append("supply.status must be layer_aligned")
    if br.get("supply", {}).get("identical") is True:
        errors.append("supply layers must not be claimed IDENTICAL")
    if br.get("classification", {}).get("ClassifyRigorSpendPredicate") != "DOES_NOT_EXIST":
        errors.append("ClassifyRigorSpendPredicate must not exist")
    if br.get("duplicate_kernel_found") is not False:
        errors.append("duplicate_kernel_found")

    vecs = load(VECTORS)
    if vecs.get("heights") != HEIGHTS:
        errors.append(f"heights drifted: {vecs.get('heights')}")
    by_id = {v["id"]: v for v in vecs.get("vectors", [])}
    if by_id["P052-G0-ORU"]["res_schedule"] != 40000000:
        errors.append("genesis schedule")
    if by_id["P052-G0-ORU"]["res_issued"] != 0:
        errors.append("genesis actual RES")
    if by_id["P052-G0-ORU"]["oru_subsidy"] != 0:
        errors.append("genesis ORU")
    if by_id["P052-H209999-EPOCH"]["epoch"] != 0:
        errors.append("epoch 209999")
    if by_id["P052-H210000-EPOCH"]["epoch"] != 1:
        errors.append("epoch 210000")
    if by_id["P052-H100-MATURITY"]["mature"] is not False:
        errors.append("maturity 100")
    if by_id["P052-H101-MATURITY"]["mature"] is not True:
        errors.append("maturity 101")
    layer = by_id["P052-SUPPLY-LAYER"]
    if layer["cpp_oru_circulating_delta"] != layer["ledger_oru_delta"]:
        errors.append("cpp circulating must map to ledger")
    if layer["cpp_oru_circulating_delta"] == layer["coq_circulating_delta"]:
        errors.append("layers must differ when burn > 0")
    if layer["layers_identical"] is not False:
        errors.append("layers_identical")

    src = REPO / "core/src"
    for p in src.rglob("*"):
        if p.suffix not in {".h", ".cpp"}:
            continue
        if "ClassifyRigorSpendPredicate" in p.read_text(encoding="utf-8", errors="replace"):
            errors.append(f"ClassifyRigorSpendPredicate appeared in {p}")

    if OCAML.is_file() and CPP.is_file():
        for h in HEIGHTS:
            oe, ce = run(OCAML, "epoch", str(h)), run(CPP, "epoch", str(h))
            if oe != ce:
                errors.append(f"epoch({h}) ocaml={oe} cpp={ce}")
            oo, co = run(OCAML, "oru", str(h)), run(CPP, "oru", str(h))
            if oo != co:
                errors.append(f"oru({h}) ocaml={oo} cpp={co}")
            ors, crs = run(OCAML, "res-schedule", str(h)), run(CPP, "res-schedule", str(h))
            if ors != crs:
                errors.append(f"res-schedule({h}) ocaml={ors} cpp={crs}")
        if run(OCAML, "res-schedule", "0") != "40000000":
            errors.append("GetResMintReward(0)")
        if run(OCAML, "res-issued", "0") != "0":
            errors.append("res_issued_at_height(0)")
        if run(OCAML, "draw-empty") != "NONE":
            errors.append("empty F")
        if run(OCAML, "mature", "100", "1") != "false":
            errors.append("immature at 100")
        if run(OCAML, "mature", "101", "1") != "true":
            errors.append("mature at 101")
        if run(OCAML, "classify", "-") != "INVALID":
            errors.append("empty classify")
        if run(OCAML, "ledger-delta", "100", "10") != "90":
            errors.append("ledger delta")
    else:
        print("note: extracted CLIs absent; inventory pins only")

    if errors:
        print("FAIL: consensus kernel cpp bridge")
        for e in errors:
            print(f"  - {e}")
        return 1
    print("PASS: Consensus Kernel Coq↔C++ bridge mapping (not a refinement)")
    return 0


if __name__ == "__main__":
    sys.exit(main())

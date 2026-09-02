#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""P053: compare extracted Coq kernel vs C++ Consensus Kernel on domain V.

Python is an oracle, not consensus authority. Does not rewrite vectors.
Does not reimplement issuance / draw / classify / supply.
"""

from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
FV = REPO / "formal_verification"
VECTORS = FV / "consensus_kernel_refinement_vectors.json"
OCAML = FV / "extracted" / "consensus_kernel_cli"
CPP = FV / "extracted" / "consensus_kernel_refinement_cli"

FORBIDDEN_DUPES = (
    "coq_formula_copy.cpp",
    "extracted_formula_copy.cpp",
    "adapter_formula.cpp",
)


def load_vectors() -> dict:
    return json.loads(VECTORS.read_text(encoding="utf-8"))


def run(cli: Path, *args: str) -> str:
    r = subprocess.run(
        [str(cli), *args],
        check=True,
        capture_output=True,
        text=True,
    )
    return r.stdout.strip()


def parse_json(s: str) -> dict:
    return json.loads(s)


def ocaml_issuance(h: int) -> dict:
    epoch, oru, sched = run(OCAML, "issuance", str(h)).split()
    issued = run(OCAML, "res-issued", str(h))
    return {
        "height": h,
        "epoch": int(epoch),
        "oru_subsidy": int(oru),
        "res_schedule": int(sched),
        "res_issued": int(issued),
    }


def cpp_issuance(h: int) -> dict:
    return parse_json(run(CPP, "issuance", str(h)))


def scan_duplicate_kernel() -> list[str]:
    hits: list[str] = []
    for name in FORBIDDEN_DUPES:
        for p in REPO.rglob(name):
            if ".git" in p.parts:
                continue
            hits.append(str(p.relative_to(REPO)))
    return hits


def main() -> int:
    if not VECTORS.is_file():
        print(f"FAIL: missing {VECTORS}", file=sys.stderr)
        return 1
    if not OCAML.is_file() or not CPP.is_file():
        print(
            "FAIL: build extracted CLI and consensus_kernel_refinement_cli first",
            file=sys.stderr,
        )
        return 1

    vec = load_vectors()
    errors: list[str] = []

    if vec.get("refinement_proved") or vec.get("equivalence_proved"):
        errors.append("vectors flipped honesty flags")

    dupes = scan_duplicate_kernel()
    if dupes:
        print("FAIL: duplicate kernel files present")
        for d in dupes:
            print(f"  - {d}")
        return 1

    # --- issuance ---
    for pin in vec["issuance"]:
        h = int(pin["height"])
        o = ocaml_issuance(h)
        c = cpp_issuance(h)
        canon = {
            "height": h,
            "epoch": o["epoch"],
            "oru_subsidy": o["oru_subsidy"],
            "res_schedule": o["res_schedule"],
            "res_issued": o["res_issued"],
        }
        cpp_canon = {
            "height": c["height"],
            "epoch": c["epoch"],
            "oru_subsidy": c["oru_subsidy"],
            "res_schedule": c["res_schedule"],
            "res_issued": c["res_issued"],
        }
        if canon != cpp_canon:
            errors.append(f"issuance({h}) ocaml={canon} cpp={cpp_canon}")
        for key in ("epoch", "oru_subsidy", "res_schedule", "res_issued"):
            if key in pin and pin[key] != canon[key]:
                errors.append(f"{pin['id']} {key} pin={pin[key]} got={canon[key]}")
        if h == 0:
            if canon["res_schedule"] != 40000000:
                errors.append("height 0 RES schedule drifted")
            if canon["res_issued"] != 0:
                errors.append("height 0 actual RES issuance drifted")
            if canon["res_schedule"] == canon["res_issued"]:
                errors.append("FORBIDDEN: schedule collapsed to issuance at height 0")

    # --- maturity ---
    for pin in vec["maturity"]:
        if pin.get("genesis_maturity_source") is False:
            if pin.get("source_height") != 0:
                errors.append(f"{pin['id']} genesis source height")
            continue
        if pin.get("genesis_source_exists") is False:
            continue
        spend, coin = int(pin["spend"]), int(pin["coin"])
        om = run(OCAML, "mature", str(spend), str(coin))
        cm = parse_json(run(CPP, "mature", str(spend), str(coin)))
        want = bool(pin.get("oru_mature", pin.get("mature")))
        if (om == "true") != want or bool(cm["oru_mature"]) != want:
            errors.append(f"{pin['id']} mature ocaml={om} cpp={cm}")
        if cm["oru_mature"] != cm["res_mature"]:
            errors.append(f"{pin['id']} ORU/RES maturity split")
        if pin.get("res_mature") is not None and bool(cm["res_mature"]) != bool(pin["res_mature"]):
            errors.append(f"{pin['id']} res_mature pin")

    # --- classify ---
    for pin in vec["classify"]:
        script = pin["script"]
        oa = run(OCAML, "classify", script)
        ca = parse_json(run(CPP, "classify", script))["asset"]
        if oa != ca or oa != pin["asset"]:
            errors.append(f"{pin['id']} classify ocaml={oa} cpp={ca} pin={pin['asset']}")

    # --- draw ---
    dcfg = vec["draw"]
    h, c_hex, b_hex = str(dcfg["h"]), dcfg["c_hex"], dcfg["b_hex"]
    empty_o = run(OCAML, "draw-empty")
    empty_c = parse_json(run(CPP, "draw-empty"))
    if empty_o != "NONE" or empty_c.get("draw") != "NONE":
        errors.append(f"empty F ocaml={empty_o} cpp={empty_c}")

    winners: dict[str, str] = {}
    for case in dcfg["cases"]:
        kind = case["kind"]
        if kind == "empty":
            continue
        cmd = "draw-t0" if kind == "t0" else "draw"
        spec = case["spec"]
        oj = parse_json(run(OCAML, cmd, h, c_hex, b_hex, spec))
        cj = parse_json(run(CPP, cmd, h, c_hex, b_hex, spec))
        o_key = {"draw": oj["draw"], "txid": oj.get("txid")}
        c_key = {"draw": cj["draw"], "txid": cj.get("txid")}
        if o_key != c_key:
            errors.append(f"{case['id']} draw ocaml={o_key} cpp={c_key}")
        winners[case["id"]] = json.dumps(o_key, sort_keys=True)

    if dcfg.get("equal_sigma_collision_constructed"):
        errors.append("do not claim a constructed SHA256 collision")

    w_eq = winners.get("P053-DRAW-EQ-WEIGHT")
    w_swap = winners.get("P053-DRAW-EQ-WEIGHT-SWAP")
    if w_eq and w_swap and w_eq != w_swap:
        errors.append(f"tie-break / order independence failed {w_eq} vs {w_swap}")
    if winners.get("P053-DRAW-T0") != json.dumps({"draw": "NONE", "txid": None}, sort_keys=True):
        errors.append(f"T=0 must be NONE, got {winners.get('P053-DRAW-T0')}")
    if winners.get("P053-DRAW-SINGLE"):
        single = json.loads(winners["P053-DRAW-SINGLE"])
        if single.get("draw") != "WINNER":
            errors.append(f"single saturating candidate must win, got {single}")

    # --- supply: ledger vs circulating stay distinct ---
    for pin in vec["supply_delta"]:
        kind = pin["kind"]
        if kind == "reject_coq":
            got = run(OCAML, "apply-delta", *pin["apply_delta"])
            if got != "REJECT":
                errors.append(f"{pin['id']} apply-delta want REJECT got {got}")
            continue

        args = [
            str(pin["height"]),
            str(pin["fees"]),
            str(pin["burn"]),
            str(pin["converge"]),
            str(pin["mint"]),
            str(pin["has_winner"]),
            str(pin["card"]),
            str(pin["oru_cb"]),
            str(pin["res_cb"]),
            str(pin["boot"]),
        ]
        cj = parse_json(run(CPP, "supply", *args))
        if cj["status"] != pin["expect_status"]:
            errors.append(f"{pin['id']} cpp status {cj['status']} want {pin['expect_status']}")
            continue
        if pin["expect_status"] == "REJECT":
            want_reason = pin.get("reject_reason")
            if want_reason and cj.get("reason") != want_reason:
                errors.append(
                    f"{pin['id']} cpp reason {cj.get('reason')} want {want_reason}"
                )
            continue

        sub = int(run(OCAML, "oru", str(pin["height"])))
        ledger = int(run(OCAML, "ledger-delta", str(sub), str(pin["converge"])))
        claim = int(
            run(OCAML, "claimable", str(pin["fees"]), str(pin["burn"]), str(sub))
        )
        if cj["oru_circulating_delta"] != ledger:
            errors.append(
                f"{pin['id']} C++ circulating {cj['oru_circulating_delta']} "
                f"!= Coq ledger {ledger}"
            )
        if cj["oru_claimable"] != claim:
            errors.append(f"{pin['id']} claimable cpp={cj['oru_claimable']} ocaml={claim}")
        if cj["oru_subsidy"] != sub:
            errors.append(f"{pin['id']} subsidy cpp={cj['oru_subsidy']} ocaml={sub}")

        ad = run(
            OCAML,
            "apply-delta",
            str(pin["height"]),
            str(sub),
            str(pin["mint"]),
            str(pin["converge"]),
            str(pin["burn"]),
            "0" if pin["mint"] else "1",
            "1" if pin["mint"] else "0",
            "0",
        )
        if ad == "REJECT":
            errors.append(f"{pin['id']} apply-delta REJECT on OK supply vector")
        else:
            _led, circ, res, _burn_acc = (int(x) for x in ad.split())
            cpp_circ = int(cj["oru_circulating_delta"])
            if pin["burn"] == 0:
                if circ != cpp_circ:
                    errors.append(
                        f"{pin['id']} burn=0 but apply circulating {circ} != cpp {cpp_circ}"
                    )
            else:
                if circ == cpp_circ:
                    errors.append(
                        f"{pin['id']} burn>0 but layers collapsed ({circ})"
                    )
                if pin.get("layers_identical"):
                    errors.append(f"{pin['id']} must not mark layers identical")
            if pin["kind"] == "res_only" and res != pin["mint"]:
                errors.append(f"{pin['id']} apply res {res} != mint {pin['mint']}")

    if errors:
        print("FAIL: consensus kernel refinement execution")
        for e in errors:
            print(f"  - {e}")
        return 1

    print("PASS: extracted Coq execution = C++ Kernel on finite domain V")
    print("VECTOR_DOMAIN=PASS EXTRACTION_EXECUTION=PASS CPP_EXECUTION=PASS BRIDGE_COMPARE=PASS")
    print("DUPLICATE_KERNEL=PASS")
    print("REFINEMENT_PROVED=false EQUIVALENCE_PROVED=false")
    print("Python is an oracle. Not a ∀-refinement.")
    return 0


if __name__ == "__main__":
    sys.exit(main())

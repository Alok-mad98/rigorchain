#!/usr/bin/env python3
"""P050 tripartite: extracted OCaml ↔ C++ Kernel. Python is an oracle, not authority.

Not a refinement. Does not reimplement issuance / draw / classify.
"""

from __future__ import annotations

import json
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OCAML = ROOT / "extracted" / "consensus_kernel_cli"
CPP = ROOT / "extracted" / "cpp_consensus_kernel_cli"
VECTORS = ROOT / "consensus_kernel_extraction_vectors.json"
ALIGN_VEC = ROOT / "consensus_kernel_alignment_vectors.json"

HEIGHTS = [0, 1, 2, 99, 100, 101, 209999, 210000, 210001, 419999, 420000, 630000]


def run(cli: Path, *args: str) -> str:
    r = subprocess.run(
        [str(cli), *args], check=True, capture_output=True, text=True
    )
    return r.stdout.strip()


def expect(cli: Path, args: list[str], want: str, label: str) -> None:
    got = run(cli, *args)
    if got != want:
        raise SystemExit(f"FAIL {label}: {cli.name} {args} -> {got!r} want {want!r}")


def main() -> int:
    if not OCAML.is_file() or not CPP.is_file():
        print("FAIL: build extract_consensus_kernel.sh first", file=sys.stderr)
        return 1

    errors: list[str] = []

    # --- epoch / issuance / genesis (OCaml = C++) ---
    t0 = time.perf_counter()
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
        oi, ci = run(OCAML, "issuance", str(h)), run(CPP, "issuance", str(h))
        if oi != ci:
            errors.append(f"issuance({h}) ocaml={oi} cpp={ci}")
    print(f"issuance_compare_seconds={time.perf_counter() - t0:.3f}")

    expect(OCAML, ["epoch", "0"], "0", "epoch0")
    expect(OCAML, ["epoch", "1"], "0", "epoch1")
    expect(OCAML, ["epoch", "209999"], "0", "epoch209999")
    expect(OCAML, ["epoch", "210000"], "1", "epoch210000")
    expect(OCAML, ["epoch", "210001"], "1", "epoch210001")
    expect(OCAML, ["oru", "0"], "0", "genesis oru")
    expect(OCAML, ["res-issued", "0"], "0", "genesis res actual")
    sched0 = run(OCAML, "res-schedule", "0")
    if sched0 != "102400000000":
        errors.append(f"ResDestroyCostAtHeight(0) drifted: {sched0}")
    if sched0 == "0":
        errors.append("FORBIDDEN: RATE at 0 collapsed to actual 0")
    expect(OCAML, ["oru", "1"], "12800000000", "height1 oru")

    t1 = time.perf_counter()
    s209999 = run(OCAML, "sum-oru", "1", "209999")
    s210000 = run(OCAML, "sum-oru", "1", "210000")
    print(f"sum_oru_seconds={time.perf_counter() - t1:.3f}")
    if s209999 != "2687987200000000":
        errors.append(f"Σ 1..209999 = {s209999}")
    if s210000 != "2688000000000000":
        errors.append(f"Σ 1..210000 = {s210000}")
    expect(OCAML, ["sum-oru", "0", "0"], "0", "genesis sum")

    # --- maturity (issued ≠ matured) ---
    expect(OCAML, ["maturity-const"], "100", "maturity const")
    expect(CPP, ["maturity-const"], "100", "cpp maturity")
    expect(OCAML, ["mature", "100", "1"], "false", "h1 immature at 100")
    expect(OCAML, ["mature", "101", "1"], "true", "h1 mature at 101")
    expect(CPP, ["mature", "100", "1"], "false", "cpp immature")
    expect(CPP, ["mature", "101", "1"], "true", "cpp mature")

    # --- classify ---
    p2wpkh = "0014" + ("11" * 20)
    res_v1 = "5120" + ("aa" * 32)
    expect(OCAML, ["classify", "-"], "INVALID", "empty fail-closed")
    expect(CPP, ["classify", "-"], "INVALID", "cpp empty")
    expect(OCAML, ["classify", p2wpkh], "ORU", "p2wpkh")
    expect(CPP, ["classify", p2wpkh], "ORU", "cpp p2wpkh")
    expect(OCAML, ["classify", res_v1], "RES", "native res")
    expect(CPP, ["classify", res_v1], "RES", "cpp native res")
    expect(OCAML, ["classify", "76a914" + ("00" * 20) + "88ac"], "INVALID", "p2pkh")
    expect(CPP, ["classify", "76a914" + ("00" * 20) + "88ac"], "INVALID", "cpp p2pkh")
    expect(OCAML, ["classify", "0020" + ("00" * 32)], "INVALID", "v0/32")
    expect(CPP, ["classify", "0020" + ("00" * 32)], "INVALID", "cpp v0/32")

    # --- supply / coinbase predicate ---
    expect(OCAML, ["res-coinbase", "0", "0"], "0", "res cb")
    expect(CPP, ["res-coinbase", "0", "0"], "0", "cpp res cb")
    expect(OCAML, ["res-coinbase", "5", "40"], "45", "res cb fees+bootstrap")
    expect(CPP, ["res-coinbase", "5", "40"], "45", "cpp res cb fees+bootstrap")
    sub1 = run(OCAML, "oru", "1")
    expect(OCAML, ["claimable", "100", "10", sub1], str(100 - 10 + int(sub1)), "claimable")
    expect(CPP, ["claimable", "100", "10", "1"], str(100 - 10 + int(sub1)), "cpp claimable")
    expect(OCAML, ["ledger-delta", sub1, "2"], str(int(sub1) - 2), "ledger delta")
    expect(CPP, ["ledger-delta", "1", "2"], str(int(sub1) - 2), "cpp ledger")

    # P049 vector file still distinguishes RATE vs actual issuance
    av = json.loads(ALIGN_VEC.read_text(encoding="utf-8"))
    by_id = {v["id"]: v for v in av.get("vectors", [])}
    if by_id["P049-G0-RES-SCHEDULE"]["get_res_mint_reward"] != 102400000000:
        errors.append("alignment vector RATE drifted")
    if by_id["P049-G0-RES-SCHEDULE"]["res_issued"] != 0:
        errors.append("alignment vector actual drifted")

    ev = json.loads(VECTORS.read_text(encoding="utf-8"))
    if ev.get("refinement_proved") or ev.get("equivalence_proved"):
        errors.append("extraction vectors flipped honesty flags")

    if errors:
        print("FAIL: consensus kernel extraction differential")
        for e in errors:
            print(f"  - {e}")
        return 1
    print("PASS: extracted OCaml = C++ Kernel on issuance/epoch/classify/supply/maturity")
    print("Python is an oracle, not consensus authority. Not a refinement.")
    return 0


if __name__ == "__main__":
    sys.exit(main())

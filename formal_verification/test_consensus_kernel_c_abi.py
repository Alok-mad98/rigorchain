#!/usr/bin/env python3
"""P053 C ABI triple: extracted OCaml ↔ C ABI ↔ C++ Kernel.

Python is an oracle. Does not recode consensus. Not a refinement.
"""

from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path

FV = Path(__file__).resolve().parent
VECTORS = FV / "consensus_kernel_c_abi_vectors.json"
OCAML = FV / "extracted" / "consensus_kernel_c_cli"
CABI = FV / "extracted" / "consensus_kernel_c_abi_cli"
CPP = FV / "extracted" / "cpp_consensus_kernel_cli"


def run(cli: Path, *args: str) -> str:
    r = subprocess.run([str(cli), *args], check=True, capture_output=True, text=True)
    return r.stdout.strip()


def jrun(cli: Path, *args: str) -> dict:
    return json.loads(run(cli, *args))


def main() -> int:
    if not VECTORS.is_file():
        print(f"FAIL: missing {VECTORS}", file=sys.stderr)
        return 1
    if not OCAML.is_file() or not CABI.is_file():
        print("FAIL: run formal_verification/extract_consensus_kernel_c.sh first", file=sys.stderr)
        return 1

    vec = json.loads(VECTORS.read_text(encoding="utf-8"))
    if vec.get("refinement_proved") or vec.get("equivalence_proved"):
        print("FAIL: honesty flags flipped", file=sys.stderr)
        return 1

    errors: list[str] = []

    for pin in vec["issuance"]:
        h = str(pin["height"])
        o = jrun(OCAML, "issuance", h)
        c = jrun(CABI, "issuance", h)
        if o != c:
            errors.append(f"issuance({h}) ocaml={o} cabi={c}")
        for key in ("epoch", "oru_subsidy", "res_destroy_cost"):
            if o.get(key) != pin[key]:
                errors.append(f"issuance({h}) {key} pin={pin[key]} got={o.get(key)}")
        if CPP.is_file():
            epoch = run(CPP, "epoch", h)
            oru = run(CPP, "oru", h)
            price = run(CPP, "res-price", h)
            if int(epoch) != o["epoch"] or int(oru) != o["oru_subsidy"] or int(price) != o["res_destroy_cost"]:
                errors.append(f"issuance({h}) cpp kernel mismatch")
        if pin["height"] == 0 and o["res_destroy_cost"] == 0:
            errors.append("FORBIDDEN: RATE at height 0 collapsed")

    for pin in vec["maturity"]:
        args = [str(pin["spend"]), str(pin["coin"])]
        o = jrun(OCAML, "mature", *args)
        c = jrun(CABI, "mature", *args)
        if o != c or o["eligible"] != pin["eligible"]:
            errors.append(f"mature {args} ocaml={o} cabi={c} pin={pin['eligible']}")
        if CPP.is_file():
            cpp = run(CPP, "mature", *args)
            if (cpp == "true") != pin["eligible"]:
                errors.append(f"mature {args} cpp={cpp}")

    for pin in vec["classify"]:
        o = jrun(OCAML, "classify", pin["script"])
        c = jrun(CABI, "classify", pin["script"])
        if o != c or o["asset"] != pin["asset"]:
            errors.append(f"classify {pin['id']} ocaml={o} cabi={c}")
        if CPP.is_file() and pin["script"] != "deadbeef":
            cpp = run(CPP, "classify", pin["script"])
            if cpp != pin["asset"]:
                errors.append(f"classify {pin['id']} cpp={cpp}")

    dcfg = vec["draw"]
    h, c_hex, b_hex = str(dcfg["h"]), dcfg["c_hex"], dcfg["b_hex"]
    empty_o = jrun(OCAML, "draw-empty")
    empty_c = jrun(CABI, "draw-empty")
    if empty_o != empty_c or empty_o.get("has_winner") is not False:
        errors.append(f"draw empty ocaml={empty_o} cabi={empty_c}")

    winners: dict[str, dict] = {}
    for case in dcfg["cases"]:
        if case["kind"] == "empty":
            continue
        cmd = case["kind"]
        spec = case["spec"]
        oj = jrun(OCAML, cmd, h, c_hex, b_hex, spec)
        cj = jrun(CABI, cmd, h, c_hex, b_hex, spec)
        if oj != cj:
            errors.append(f"draw {case['id']} ocaml={oj} cabi={cj}")
        winners[case["id"]] = oj
    for case_id, result in winners.items():
        if result.get("has_winner") is not False:
            errors.append(f"RFC-019 retired the lottery; {case_id} must be empty: {result}")

    for pin in vec["supply"]:
        args = [
            str(pin["height"]),
            str(pin["fees"]),
            str(pin["burn"]),
            str(pin["converge"]),
            str(pin["mint"]),
            str(pin["has_winner"]),
            str(pin["card"]),
            str(pin["oru_cb"]),
            "0",
        ]
        cabi = jrun(CABI, "supply", *args)
        if cabi.get("ok") != pin["ok"]:
            errors.append(f"supply {pin['id']} cabi={cabi} want ok={pin['ok']}")
            continue
        if pin["id"] == "reject":
            ad = jrun(OCAML, "apply-delta", "1", "0", "1", "0", "0", "1", "0", "0")
            if ad.get("ok") is not False:
                errors.append(f"supply reject ocaml apply-delta={ad}")
            continue
        if pin["id"] == "overflow":
            continue
        sub = jrun(OCAML, "issuance", str(pin["height"]))["oru_subsidy"]
        led = jrun(OCAML, "supply-ledger", str(sub), str(pin["converge"]))
        if cabi.get("ok") and led.get("delta") != cabi.get("delta"):
            errors.append(
                f"supply {pin['id']} ledger {led.get('delta')} != cabi {cabi.get('delta')}"
            )
        if pin.get("layers_identical"):
            errors.append(f"supply {pin['id']} must not claim layers identical")

    if errors:
        print("FAIL: consensus kernel C ABI differential")
        for e in errors:
            print(f"  - {e}")
        return 1
    print("PASS: extracted OCaml = C ABI = C++ Kernel on the C ABI vector domain")
    print("EXTRACTED_KERNEL_C_ABI=PASS CPP_ADAPTER=PASS")
    print("REFINEMENT_PROVED=false EQUIVALENCE_PROVED=false EXTRACTED_TO_BINARY=NOT-LINKED")
    return 0


if __name__ == "__main__":
    sys.exit(main())

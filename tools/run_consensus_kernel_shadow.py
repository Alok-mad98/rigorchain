#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""P055: C++ Kernel vs extracted oracle on prepared Kernel inputs.

Python is an oracle driver. Does not recode consensus. On mismatch: STOP.
Does not auto-fix. Not a refinement. Not a production switch.
"""

from __future__ import annotations

import json
import os
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
FV = REPO / "formal_verification"
SRC = REPO / "core/src"
OUT = FV / "extracted"
TOOLS_FV = FV / "tools"
VECTORS = FV / "consensus_kernel_c_abi_vectors.json"
OCAML = OUT / "consensus_kernel_c_cli"
CABI = OUT / "consensus_kernel_c_abi_cli"
SHADOW = OUT / "test_consensus_kernel_shadow"


def run(cli: Path, *args: str) -> str:
    r = subprocess.run(
        [str(cli), *args],
        check=True,
        capture_output=True,
        text=True,
    )
    return r.stdout.strip()


def jrun(cli: Path, *args: str) -> dict:
    return json.loads(run(cli, *args))


def stop(input_vec, cpp, ext) -> None:
    print("CONSENSUS_KERNEL_SHADOW_DISCREPANCY_FOUND", file=sys.stderr)
    print(f"input: {input_vec}", file=sys.stderr)
    print(f"cpp: {cpp}", file=sys.stderr)
    print(f"extracted: {ext}", file=sys.stderr)


def build_shadow() -> None:
    cxx = os.environ.get("CXX") or (
        subprocess.check_output(["bash", "-lc", "command -v g++ || command -v c++"], text=True).strip()
    )
    cmd = [
        cxx,
        "-O2",
        "-std=c++17",
        "-ffunction-sections",
        "-fdata-sections",
        "-DRIGOR_KERNEL_SHADOW_VERIFY_ONLY",
        "-DRIGOR_KERNEL_ABI_VERIFY_ONLY",
        f"-I{SRC}",
        "-o",
        str(SHADOW),
        str(SRC / "test/test_consensus_kernel_shadow.cpp"),
        str(SRC / "consensus/shadow/shadow_executor.cpp"),
        str(REPO / "tools/consensus_kernel_refinement_log_stub.cpp"),
        str(SRC / "consensus/bridge/consensus_kernel_c_api.cpp"),
        str(SRC / "consensus/bridge/kernel_marshalling.cpp"),
        str(SRC / "consensus/bridge/issuance_bridge.cpp"),
        str(SRC / "consensus/bridge/classify_bridge.cpp"),
        str(SRC / "consensus/bridge/draw_bridge.cpp"),
        str(SRC / "consensus/bridge/supply_bridge.cpp"),
        str(SRC / "consensus/rigor_issuance.cpp"),
        str(SRC / "consensus/rigor_supply.cpp"),
        str(SRC / "script/rigor_classify.cpp"),
        str(SRC / "script/script.cpp"),
        str(SRC / "uint256.cpp"),
        str(SRC / "util/strencodings.cpp"),
        str(SRC / "consensus/convergence_logic.cpp"),
        str(SRC / "consensus_rigor/sigma_hash.cpp"),
        str(SRC / "crypto/sha256.cpp"),
        str(SRC / "arith_uint256.cpp"),
        "-Wl,--gc-sections",
    ]
    subprocess.run(cmd, check=True)


def build_cabi() -> None:
    cxx = os.environ.get("CXX") or (
        subprocess.check_output(["bash", "-lc", "command -v g++ || command -v c++"], text=True).strip()
    )
    cmd = [
        cxx,
        "-O2",
        "-std=c++17",
        "-ffunction-sections",
        "-fdata-sections",
        "-DRIGOR_KERNEL_ABI_VERIFY_ONLY",
        f"-I{SRC}",
        "-o",
        str(CABI),
        str(TOOLS_FV / "consensus_kernel_c_abi_cli.cpp"),
        str(REPO / "tools/consensus_kernel_refinement_log_stub.cpp"),
        str(SRC / "consensus/bridge/consensus_kernel_c_api.cpp"),
        str(SRC / "consensus/bridge/kernel_marshalling.cpp"),
        str(SRC / "consensus/bridge/issuance_bridge.cpp"),
        str(SRC / "consensus/bridge/classify_bridge.cpp"),
        str(SRC / "consensus/bridge/draw_bridge.cpp"),
        str(SRC / "consensus/bridge/supply_bridge.cpp"),
        str(SRC / "consensus/rigor_issuance.cpp"),
        str(SRC / "consensus/rigor_supply.cpp"),
        str(SRC / "script/rigor_classify.cpp"),
        str(SRC / "script/script.cpp"),
        str(SRC / "uint256.cpp"),
        str(SRC / "util/strencodings.cpp"),
        str(SRC / "consensus/convergence_logic.cpp"),
        str(SRC / "consensus_rigor/sigma_hash.cpp"),
        str(SRC / "crypto/sha256.cpp"),
        str(SRC / "arith_uint256.cpp"),
        "-Wl,--gc-sections",
    ]
    subprocess.run(cmd, check=True)


def ensure_extracted_clis() -> None:
    if not OCAML.is_file():
        ml = OUT / "consensus_kernel_c.ml"
        if not ml.is_file():
            print("FAIL: missing extracted kernel; run extract_consensus_kernel_c.sh", file=sys.stderr)
            sys.exit(1)
        subprocess.run(
            ["ocamlopt", "-w", "-3", "-o", str(OCAML), str(ml), str(TOOLS_FV / "consensus_kernel_c_cli.ml")],
            check=True,
            cwd=str(OUT),
        )
    if not CABI.is_file():
        build_cabi()


def main() -> int:
    if not VECTORS.is_file():
        print(f"FAIL: missing {VECTORS}", file=sys.stderr)
        return 1

    OUT.mkdir(parents=True, exist_ok=True)
    build_shadow()
    r = subprocess.run([str(SHADOW)], capture_output=True, text=True)
    if r.returncode != 0:
        sys.stderr.write(r.stderr)
        sys.stdout.write(r.stdout)
        return 1
    print(r.stdout.strip())

    lint = subprocess.run(
        [sys.executable, str(REPO / "tools/lint-consensus-kernel-shadow.py")],
        capture_output=True,
        text=True,
    )
    sys.stdout.write(lint.stdout)
    if lint.returncode != 0:
        sys.stderr.write(lint.stderr)
        return 1

    ensure_extracted_clis()
    vec = json.loads(VECTORS.read_text(encoding="utf-8"))
    if vec.get("refinement_proved") or vec.get("equivalence_proved"):
        print("FAIL: honesty flags flipped", file=sys.stderr)
        return 1

    for pin in vec["issuance"]:
        h = str(pin["height"])
        o = jrun(OCAML, "issuance", h)
        c = jrun(CABI, "issuance", h)
        if o != c:
            stop({"surface": "issuance", "height": pin["height"]}, c, o)
            return 1
        if pin["height"] == 0 and o["res_reward_schedule"] == 0:
            stop({"height": 0}, c, o)
            return 1
        actual = 0 if pin["height"] == 0 else o["res_reward_schedule"]
        if pin["height"] == 0 and actual != 0:
            stop({"height": 0, "actual": actual}, c, o)
            return 1

    for pin in vec["classify"]:
        o = jrun(OCAML, "classify", pin["script"])
        c = jrun(CABI, "classify", pin["script"])
        if o != c or o["asset"] != pin["asset"]:
            stop({"surface": "classify", "id": pin["id"]}, c, o)
            return 1

    dcfg = vec["draw"]
    h, c_hex, b_hex = str(dcfg["h"]), dcfg["c_hex"], dcfg["b_hex"]
    empty_o = jrun(OCAML, "draw-empty")
    empty_c = jrun(CABI, "draw-empty")
    if empty_o != empty_c or empty_o.get("has_winner") is not False:
        stop({"surface": "draw", "F": "empty"}, empty_c, empty_o)
        return 1
    winners = {}
    for case in dcfg["cases"]:
        if case["kind"] == "empty":
            continue
        oj = jrun(OCAML, case["kind"], h, c_hex, b_hex, case["spec"])
        cj = jrun(CABI, case["kind"], h, c_hex, b_hex, case["spec"])
        if oj != cj:
            stop({"surface": "draw", "id": case["id"]}, cj, oj)
            return 1
        winners[case["id"]] = oj
    if winners.get("boundary_target", {}).get("has_winner") is not False:
        stop({"surface": "draw", "id": "boundary_target"}, winners.get("boundary_target"), {"has_winner": False})
        return 1
    if winners.get("two") and winners.get("same_target"):
        if winners["two"].get("winner") != winners["same_target"].get("winner"):
            stop({"surface": "draw", "same_sigma_different_txid": True}, winners["two"], winners["same_target"])
            return 1

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
            stop({"surface": "supply", "id": pin["id"]}, cabi, pin)
            return 1
        if pin["id"] in ("reject", "overflow"):
            continue
        sub = jrun(OCAML, "issuance", str(pin["height"]))["oru_subsidy"]
        led = jrun(OCAML, "supply-ledger", str(sub), str(pin["converge"]))
        if cabi.get("ok") and led.get("delta") != cabi.get("delta"):
            stop(
                {
                    "surface": "supply",
                    "id": pin["id"],
                    "note": "LedgerDeltaBridge: cpp circulating <-> Coq ledger_oru_delta",
                },
                cabi,
                led,
            )
            return 1

    print("EXTRACTED_KERNEL_EXECUTION=PASS")
    print("SHADOW_COMPARE=PASS")
    print("DISCREPANCY=NONE")
    print("PRODUCTION_KERNEL_SWITCHED=NO")
    print("REFINEMENT_PROVED=false")
    print("EQUIVALENCE_PROVED=false")
    return 0


if __name__ == "__main__":
    sys.exit(main())

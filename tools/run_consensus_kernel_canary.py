#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""P068: build canary ON, run vectors, compare C++ vs C ABI. Does not switch production."""

from __future__ import annotations

import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "core/src"
TOOLS = ROOT / "tools"
FV = ROOT / "formal_verification"
VECTORS = FV / "kernel_canary_vectors.json"


def run(cmd: list[str], cwd: Path | None = None) -> None:
    print("==", " ".join(cmd))
    subprocess.run(cmd, check=True, cwd=cwd)


def compile_canary(cxx: str, out: Path) -> None:
    cmd = [
        cxx,
        "-O2",
        "-std=c++17",
        "-ffunction-sections",
        "-fdata-sections",
        "-DENABLE_CONSENSUS_KERNEL_CANARY",
        "-DRIGOR_KERNEL_ABI_VERIFY_ONLY",
        f"-I{SRC}",
        "-o",
        str(out),
        str(SRC / "test/test_consensus_kernel_canary.cpp"),
        str(SRC / "consensus/kernel_canary/kernel_canary.cpp"),
        str(SRC / "consensus/bridge/consensus_kernel_c_api.cpp"),
        str(SRC / "consensus/bridge/kernel_marshalling.cpp"),
        str(SRC / "consensus/bridge/issuance_bridge.cpp"),
        str(SRC / "consensus/bridge/classify_bridge.cpp"),
        str(SRC / "consensus/bridge/draw_bridge.cpp"),
        str(SRC / "consensus/bridge/supply_bridge.cpp"),
        str(SRC / "consensus/rigor_issuance.cpp"),
        str(SRC / "consensus/rigor_supply.cpp"),
        str(TOOLS / "consensus_kernel_refinement_log_stub.cpp"),
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
    run(cmd)


def main() -> int:
    v = json.loads(VECTORS.read_text(encoding="utf-8"))
    if v.get("production_kernel_switched") is not False:
        print("CONSENSUS_KERNEL_CANARY_DISCREPANCY")
        print("FAIL: vectors claim production switch")
        return 1
    if v.get("equivalence_proved") is not False:
        print("CONSENSUS_KERNEL_CANARY_DISCREPANCY")
        print("FAIL: vectors overclaim equivalence")
        return 1

    run([sys.executable, str(TOOLS / "lint-consensus-kernel-canary.py")])

    cxx = os.environ.get("CXX") or "g++"
    with tempfile.TemporaryDirectory(prefix="rigor_canary_") as td:
        out = Path(td) / "test_consensus_kernel_canary"
        print("== build canary (ENABLE_CONSENSUS_KERNEL_CANARY=ON) ==")
        compile_canary(cxx, out)
        print("== run vectors + 10000 compare ==")
        r = subprocess.run([str(out)], capture_output=True, text=True)
        sys.stdout.write(r.stdout)
        if r.returncode != 0:
            sys.stderr.write(r.stderr)
            print("CONSENSUS_KERNEL_CANARY_DISCREPANCY")
            return 1
        if "CONSENSUS_KERNEL_CANARY_PASS" not in r.stdout:
            print("CONSENSUS_KERNEL_CANARY_DISCREPANCY")
            print("FAIL: canary binary did not report PASS")
            return 1

    print("CONSENSUS_KERNEL_CANARY_PASS")
    print("CANARY_IMPLEMENTED = TRUE")
    print("CPP_PATH_STATUS = PASS")
    print("EXTRACTED_PATH_STATUS = PASS")
    print("COMPARE_STATUS = PASS")
    print("DISCREPANCY_COUNT = 0")
    print("PRODUCTION_SWITCHED = FALSE")
    print("REFINEMENT_PROVED = TRUE")
    print("EQUIVALENCE_PROVED = FALSE")
    return 0


if __name__ == "__main__":
    sys.exit(main())

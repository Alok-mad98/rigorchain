#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""R2: every Kernel Invalid() reason has a REJECT vector or connect-reject pin."""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SUPPLY = ROOT / "core/src/consensus/rigor_supply.cpp"
GENESIS = ROOT / "core/src/consensus/kernel/genesis_authority.cpp"
VECTORS = ROOT / "formal_verification/consensus_kernel_refinement_vectors.json"
TEST_GENESIS = ROOT / "core/src/test/test_genesis_consensus_authority.cpp"
COQ_KERNEL = ROOT / "formal_verification/coq/KernelResultApplication.v"
LOCK = ROOT / "core/src/consensus/rigor_consensus_lock.h"

INVALID_RE = re.compile(
    r'Invalid\s*\(\s*[^,]+,\s*"([a-z0-9-]+)"'
)


def reasons_in(path: Path) -> list[str]:
    text = path.read_text(encoding="utf-8")
    found: list[str] = []
    for m in INVALID_RE.finditer(text):
        r = m.group(1)
        if r not in found:
            found.append(r)
    return found


def main() -> int:
    errors: list[str] = []
    vec = json.loads(VECTORS.read_text(encoding="utf-8"))

    supply_reasons = reasons_in(SUPPLY)
    if not supply_reasons:
        errors.append("no Invalid() reasons in rigor_supply.cpp")
    covered = {
        pin.get("reject_reason")
        for pin in vec.get("supply_delta") or []
        if pin.get("expect_status") == "REJECT" and pin.get("reject_reason")
    }
    for r in supply_reasons:
        if r not in covered:
            errors.append(f"supply reason {r} has no supply_delta REJECT vector")

    connect_reasons = reasons_in(GENESIS)
    if "kernel-negative-height" not in connect_reasons:
        errors.append("EvaluateConnect missing kernel-negative-height")
    if "kernel-connect-input" not in connect_reasons:
        errors.append("EvaluateConnect missing kernel-connect-input")
    pins = {p.get("reason") for p in vec.get("kernel_connect_reject") or []}
    test = TEST_GENESIS.read_text(encoding="utf-8")
    for r in connect_reasons:
        if r not in pins:
            errors.append(f"connect reason {r} missing from kernel_connect_reject")
        if r not in test:
            errors.append(f"connect reason {r} missing from {TEST_GENESIS.name}")

    coq = COQ_KERNEL.read_text(encoding="utf-8")
    for name in (
        "KR_WinnerMismatch",
        "KR_TriggerUnderflow",
        "KR_WithoutWinner",
        "KR_ResAmount",
        "KR_BadCbAmount",
        "KR_BadCbResAmount",
        "kernel_supply_reject",
    ):
        if name not in coq:
            errors.append(f"Coq kernel missing {name}")

    lock = LOCK.read_text(encoding="utf-8")
    m = re.search(r"CONVERGENCE_DRAW_MIN_TXS_FOR_RES\s*=\s*(\d+)", lock)
    cm = re.search(r"CONVERGENCE_DRAW_MIN_TXS_FOR_RES_Z\s*:\s*Z\s*:=\s*(\d+)", coq)
    if not m or not cm or m.group(1) != cm.group(1):
        errors.append("CONVERGENCE_DRAW_MIN_TXS_FOR_RES Coq/C++ mismatch")

    if errors:
        print("FAIL: kernel reject-reason coverage")
        for e in errors:
            print(f"  - {e}")
        return 1

    print("KERNEL_REJECT_COVERAGE=PASS")
    print(f"supply_reasons={len(supply_reasons)} connect_reasons={len(connect_reasons)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

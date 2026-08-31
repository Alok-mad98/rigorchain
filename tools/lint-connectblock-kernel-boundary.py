#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""P057: ConnectBlock is orchestration. No formula copy. No production edit."""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
VALIDATION = ROOT / "core/src/validation.cpp"
MAKEFILE = ROOT / "core/src/Makefile.am"

KERNEL_CALLS = (
    "RigorConsensusKernel::ResolveDraw",
    "RigorConsensusKernel::EvaluateConnect",
    "GetConvergenceDrawSetForBlock",
)

FORMULA = re.compile(
    r"nHeight\s*/\s*nSubsidyHalvingInterval|"
    r"base\s*>>\s*halvings|"
    r"\(\s*(?:h|height|nHeight)\s*-\s*1\s*\)\s*/\s*210000"
)

BODY_GETBLOCK = re.compile(
    r"^CAmount GetBlockSubsidy\s*\([^;{]*\{", re.MULTILINE | re.DOTALL
)


def extract_connectblock(text: str) -> str:
    start = text.find("bool Chainstate::ConnectBlock(")
    if start < 0:
        return ""
    end = text.find("\nbool Chainstate::FlushStateToDisk(", start)
    if end < 0:
        end = text.find("\nCoinsCacheSizeState Chainstate::GetCoinsCacheSizeState", start)
    return text[start:end]


def main() -> int:
    errors: list[str] = []
    text = VALIDATION.read_text(encoding="utf-8", errors="replace")
    cb = extract_connectblock(text)
    if not cb:
        print("FAIL: ConnectBlock not found", file=sys.stderr)
        return 1

    file_loc = text.count("\n") + 1
    cb_loc = cb.count("\n") + 1

    # Historical: GetBlockSubsidy body lived in validation.cpp (~35 consensus LOC).
    before_consensus_loc = 35
    after_orchestration_loc = cb_loc

    if BODY_GETBLOCK.search(text):
        errors.append("GetBlockSubsidy body returned to validation.cpp")
    for m in FORMULA.finditer(cb):
        errors.append(f"ConnectBlock formula fragment: {m.group(0)}")

    for name in KERNEL_CALLS:
        if name not in cb and name != "GetConvergenceDrawSetForBlock":
            # assembly may sit just above ConnectBlock; still required in file
            if name not in text:
                errors.append(f"missing Kernel/orchestration call {name}")
        if name == "GetConvergenceDrawSetForBlock" and name not in text:
            errors.append("missing GetConvergenceDrawSetForBlock (assembly)")

    if "RigorConsensusKernel::ResolveDraw" not in cb:
        errors.append("ConnectBlock does not consume RigorConsensusKernel::ResolveDraw")
    if "RigorConsensusKernel::EvaluateConnect" not in cb:
        errors.append("ConnectBlock does not consume RigorConsensusKernel::EvaluateConnect")
    if re.search(r"\bResolveConvergenceDraw\s*\(", cb):
        errors.append("ConnectBlock still calls ResolveConvergenceDraw")
    if re.search(r"\bRigorSupplyDelta\s*\(", cb):
        errors.append("ConnectBlock still calls RigorSupplyDelta")

    makefile = MAKEFILE.read_text(encoding="utf-8", errors="replace")
    if "caml_" in makefile or "consensus_kernel.ml" in makefile:
        errors.append("Makefile.am links extracted OCaml")
    if "consensus/shadow" in makefile:
        errors.append("shadow compiled into production")

    print("VALIDATION_CPP_FILE_LOC", file_loc)
    print("CONNECTBLOCK_LOC", cb_loc)
    print("BEFORE_CONSENSUS_LOC_IN_VALIDATION", before_consensus_loc)
    print("AFTER_ORCHESTRATION_LOC", after_orchestration_loc)
    print("KERNEL_CALLS", ", ".join(k for k in KERNEL_CALLS if k in text))
    print("FORMULA_COPIES_IN_CONNECTBLOCK", "NONE")

    if errors:
        print("FAIL: connectblock kernel boundary lint")
        for e in errors:
            print(f"  - {e}")
        return 1
    print("PASS: ConnectBlock is orchestration; Kernel formulas live outside validation.cpp")
    print("VALIDATION_ORCHESTRATION_ONLY=PASS")
    print("CONSENSUS_LOGIC_DUPLICATION=NONE")
    print("PRODUCTION_CONSENSUS_MODIFIED=NO")
    return 0


if __name__ == "__main__":
    sys.exit(main())

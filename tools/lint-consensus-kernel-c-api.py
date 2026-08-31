#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""P053-C-ABI: bridge adapters must call the Kernel, not recode it."""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BRIDGE = ROOT / "core/src/consensus/bridge"

FORBIDDEN_INCLUDES = re.compile(
    r'#\s*include\s*[<"](?:validation\.h|chain\.h|coins\.h|txdb|wallet/|rpc/|miner\.h|miner/)',
)

# consensus/validation.h is BlockValidationState (Kernel reject channel), not ConnectBlock.
OK_CONSENSUS_VALIDATION = re.compile(r'#\s*include\s*<consensus/validation\.h>')

FORMULA = re.compile(
    r"GetBlockSubsidy\s*\(|GetResMintReward\s*\(|"
    r"/\s*210000|nHeight\s*/\s*nSubsidyHalvingInterval|"
    r"base\s*>>\s*halvings"
)

REQUIRED = {
    "issuance_bridge.cpp": "ComputeRigorIssuance",
    "classify_bridge.cpp": "ClassifyRigorAsset",
    "draw_bridge.cpp": "ResolveConvergenceDraw",
    "supply_bridge.cpp": "RigorSupplyDelta",
}

CPP_TYPES = re.compile(r"\bstd::(vector|string)\b|CTransaction|CBlockIndex|CCoinsView")


def main() -> int:
    errors: list[str] = []
    if not BRIDGE.is_dir():
        print("FAIL: missing core/src/consensus/bridge", file=sys.stderr)
        return 1

    for path in sorted(BRIDGE.iterdir()):
        if path.suffix not in {".h", ".cpp"}:
            continue
        text = path.read_text(encoding="utf-8", errors="replace")
        rel = path.relative_to(ROOT)

        for m in FORBIDDEN_INCLUDES.finditer(text):
            frag = m.group(0)
            if "consensus/validation.h" in frag:
                continue
            errors.append(f"{rel}: forbidden include {frag}")

        if path.suffix == ".cpp" and FORMULA.search(text):
            errors.append(f"{rel}: formula fragment (must call Kernel, not recode)")

        if path.name == "consensus_kernel_c_api.h" and CPP_TYPES.search(text):
            errors.append(f"{rel}: C ABI header is not POD-only")
        if path.name == "kernel_types.h" and CPP_TYPES.search(text):
            errors.append(f"{rel}: kernel_types.h is not POD-only")

        req = REQUIRED.get(path.name)
        if req and req not in text:
            errors.append(f"{rel}: must call {req}")

    makefile = (ROOT / "core/src/Makefile.am").read_text(encoding="utf-8", errors="replace")
    if "consensus/bridge/" in makefile:
        errors.append("Makefile.am lists bridge/ — production would compile the ABI")
    if "caml_" in makefile or "consensus_kernel.ml" in makefile:
        errors.append("Makefile.am links extracted OCaml")

    connect = (ROOT / "core/src/validation.cpp").read_text(encoding="utf-8", errors="replace")
    if "consensus/bridge" in connect or "rigor_compute_issuance" in connect:
        errors.append("ConnectBlock references the C ABI (production switch is forbidden)")

    if errors:
        print("FAIL: consensus kernel C ABI lint")
        for e in errors:
            print(f"  - {e}")
        return 1
    print("PASS: C ABI adapters call Kernel functions; no duplicate formula; ConnectBlock unchanged")
    return 0


if __name__ == "__main__":
    sys.exit(main())

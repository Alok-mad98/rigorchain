#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""P055: shadow executor may use bridge / C ABI / extracted oracle only."""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SHADOW = ROOT / "core/src/consensus/shadow"

FORBIDDEN_INCLUDES = re.compile(
    r'#\s*include\s*[<"](?:validation\.h|chain\.h|coins\.h|txdb|'
    r'wallet/|rpc/|miner\.h|miner/|net\.h|net_processing)',
)

OK_CONSENSUS_VALIDATION = "consensus/validation.h"

FORMULA = re.compile(
    r"/\s*210000|nHeight\s*/\s*nSubsidyHalvingInterval|base\s*>>\s*halvings"
)

REQUIRED_CALLS = (
    "ComputeRigorIssuance",
    "ClassifyRigorAsset",
    "ResolveConvergenceDraw",
    "RigorSupplyDelta",
    "LedgerDeltaBridge",
    "rigor_compute_issuance",
)


def main() -> int:
    errors: list[str] = []
    if not SHADOW.is_dir():
        print("FAIL: missing core/src/consensus/shadow", file=sys.stderr)
        return 1

    texts: dict[Path, str] = {}
    for path in sorted(SHADOW.iterdir()):
        if path.suffix not in {".h", ".cpp"}:
            continue
        text = path.read_text(encoding="utf-8", errors="replace")
        texts[path] = text
        rel = path.relative_to(ROOT)
        for m in FORBIDDEN_INCLUDES.finditer(text):
            frag = m.group(0)
            if OK_CONSENSUS_VALIDATION in frag:
                continue
            errors.append(f"{rel}: forbidden include {frag}")
        if FORMULA.search(text):
            errors.append(f"{rel}: formula fragment (must call Kernel, not recode)")
        if "validation.cpp" in text or "GetConvergenceDrawSetForBlock" in text:
            errors.append(f"{rel}: state assembly / ConnectBlock leakage")

    joined = "\n".join(texts.values())
    if "ConsensusKernelShadowExecutor" not in joined:
        errors.append("missing ConsensusKernelShadowExecutor")
    if "ConsensusKernelInput" not in joined:
        errors.append("missing ConsensusKernelInput")
    if "LedgerDeltaBridge" not in joined:
        errors.append("missing LedgerDeltaBridge")
    for req in REQUIRED_CALLS:
        if req not in joined:
            errors.append(f"shadow must call/mention {req}")
    if "consensus/bridge" not in joined:
        errors.append("shadow must be allowed to use consensus/bridge")

    makefile = (ROOT / "core/src/Makefile.am").read_text(encoding="utf-8", errors="replace")
    if "consensus/shadow" in makefile:
        errors.append("Makefile.am lists shadow/ — production would compile the oracle")
    if "caml_" in makefile:
        errors.append("Makefile.am links OCaml")

    connect = (ROOT / "core/src/validation.cpp").read_text(encoding="utf-8", errors="replace")
    if "consensus/shadow" in connect or "ConsensusKernelShadowExecutor" in connect:
        errors.append("ConnectBlock references shadow (production switch is forbidden)")

    test_inc = (ROOT / "core/src/Makefile.test.include").read_text(encoding="utf-8", errors="replace")
    if "test_consensus_kernel_shadow.cpp" in test_inc:
        errors.append("shadow test must not join test_rigor (verify-only flags)")

    if errors:
        print("FAIL: consensus kernel shadow lint")
        for e in errors:
            print(f"  - {e}")
        return 1
    print("PASS: shadow uses prepared Kernel inputs only; ConnectBlock unchanged")
    return 0


if __name__ == "__main__":
    sys.exit(main())

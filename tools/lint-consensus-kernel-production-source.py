#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""P051 static audit: production Consensus Kernel execution source.

Fails if ConnectBlock would call extracted OCaml, if a second formula
body appears in production, or if (height-1)/210000 appears in Kernel
files. Not a refinement proof. Does not rewrite production consensus.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

CONNECT_BLOCK = ROOT / "core/src/validation.cpp"

KERNEL_FORMULA_FILES = [
    ROOT / "core/src/consensus/rigor_issuance.h",
    ROOT / "core/src/consensus/rigor_issuance.cpp",
    ROOT / "core/src/consensus/rigor_supply.h",
    ROOT / "core/src/consensus/rigor_supply.cpp",
    ROOT / "core/src/consensus/rigor_consensus_lock.h",
    ROOT / "core/src/consensus/convergence_logic.cpp",
    ROOT / "core/src/script/rigor_classify.cpp",
]

PRODUCTION_SCAN = [
    ROOT / "core/src/consensus",
    ROOT / "core/src/validation.cpp",
    ROOT / "core/src/connect_block_rigor_checks.cpp",
    ROOT / "core/src/script/rigor_classify.cpp",
    ROOT / "core/src/script/standard.cpp",
    ROOT / "core/src/Makefile.am",
]

FORBIDDEN_LINK = re.compile(
    r"consensus_kernel\.ml|ExtractConsensusKernel|caml_startup|libasmrun|"
    r"EXTRACTED_CONSENSUS_KERNEL",
    re.MULTILINE,
)

MINUS_ONE_EPOCH = re.compile(
    r"\(\s*(?:h|height|nHeight)\s*-\s*1\s*\)\s*/\s*210000",
)

# Opening of a definition (body `{`), not a `;` declaration and not a call.
BODY = {
    "GetBlockSubsidy": (
        re.compile(r"^CAmount GetBlockSubsidy\s*\([^;{]*\{", re.MULTILINE | re.DOTALL),
        ROOT / "core/src/consensus/rigor_issuance.cpp",
    ),
    "RigorIssuanceEpoch": (
        re.compile(r"inline int RigorIssuanceEpoch\s*\([^;{]*\{", re.MULTILINE | re.DOTALL),
        ROOT / "core/src/consensus/rigor_issuance.h",
    ),
    "GetResMintReward": (
        re.compile(r"inline constexpr CAmount GetResMintReward\s*\([^;{]*\{", re.MULTILINE | re.DOTALL),
        ROOT / "core/src/consensus/rigor_consensus_lock.h",
    ),
    "ResRewardForEpoch": (
        re.compile(r"inline constexpr CAmount ResRewardForEpoch\s*\([^;{]*\{", re.MULTILINE | re.DOTALL),
        ROOT / "core/src/consensus/rigor_consensus_lock.h",
    ),
    "ResolveConvergenceDraw": (
        re.compile(r"^ConvergenceDrawOutcome ResolveConvergenceDraw\s*\([^;{]*\{", re.MULTILINE | re.DOTALL),
        ROOT / "core/src/consensus/convergence_logic.cpp",
    ),
    "ClassifyRigorTxout": (
        re.compile(r"^TxoutType ClassifyRigorTxout\s*\([^;{]*\{", re.MULTILINE | re.DOTALL),
        ROOT / "core/src/script/rigor_classify.cpp",
    ),
    "RigorSupplyDelta": (
        re.compile(r"^bool RigorSupplyDelta\s*\([^;{]*\{", re.MULTILINE | re.DOTALL),
        ROOT / "core/src/consensus/rigor_supply.cpp",
    ),
    "RigorResCoinbaseExpected": (
        re.compile(r"inline CAmount RigorResCoinbaseExpected\s*\([^;{]*\{", re.MULTILINE | re.DOTALL),
        ROOT / "core/src/consensus/rigor_supply.h",
    ),
}


def iter_prod_files() -> list[Path]:
    out: list[Path] = []
    for p in PRODUCTION_SCAN:
        if p.is_file():
            out.append(p)
        elif p.is_dir():
            out.extend(
                q
                for q in p.rglob("*")
                if q.is_file()
                and q.suffix in {".h", ".cpp", ".am"}
                and "bridge" not in q.parts
                and "shadow" not in q.parts
                and "kernel_migration" not in q.parts
                and "kernel_canary" not in q.parts
            )
    return out


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    errors: list[str] = []

    if args.self_test:
        fake = "(nHeight - 1) / 210000"
        if not MINUS_ONE_EPOCH.search(fake):
            print("FAIL: self-test detector did not fire", file=sys.stderr)
            return 1
        print("PASS: self-test detector fires on (nHeight-1)/210000")
        return 0

    cb = CONNECT_BLOCK.read_text(encoding="utf-8", errors="replace")
    if "bool Chainstate::ConnectBlock" not in cb:
        errors.append("ConnectBlock definition missing")
    for needle in (
        "RigorConsensusKernel",
        "GetConvergenceDrawSetForBlock",
        "hashGenesisBlock",
    ):
        if needle not in cb:
            errors.append(f"ConnectBlock does not mention {needle}")
    if FORBIDDEN_LINK.search(cb):
        errors.append("ConnectBlock references extracted/OCaml linkage")

    for path in KERNEL_FORMULA_FILES:
        text = path.read_text(encoding="utf-8", errors="replace")
        if MINUS_ONE_EPOCH.search(text):
            errors.append(f"{path.relative_to(ROOT)} uses (height-1)/210000")

    for path in iter_prod_files():
        text = path.read_text(encoding="utf-8", errors="replace")
        if FORBIDDEN_LINK.search(text):
            errors.append(f"{path.relative_to(ROOT)} references extracted/OCaml")

    makefile = (ROOT / "core/src/Makefile.am").read_text(encoding="utf-8", errors="replace")
    if "consensus_kernel" in makefile or "caml_" in makefile:
        errors.append("Makefile.am links extracted OCaml")
    for src in (
        "consensus/rigor_issuance.cpp",
        "consensus/rigor_supply.cpp",
        "script/rigor_classify.cpp",
        "consensus/convergence_logic.cpp",
    ):
        if src not in makefile:
            errors.append(f"Makefile.am missing {src}")

    # Unique body sites. Scan production .h/.cpp excluding tests.
    scan_roots = [
        ROOT / "core/src/consensus",
        ROOT / "core/src/script",
        ROOT / "core/src/validation.cpp",
        ROOT / "core/src/connect_block_rigor_checks.cpp",
        ROOT / "core/src/node",
        ROOT / "core/src/rpc",
    ]
    files: list[Path] = []
    for p in scan_roots:
        if p.is_file():
            files.append(p)
        elif p.is_dir():
            files.extend(
                q
                for q in p.rglob("*")
                if q.is_file()
                and q.suffix in {".h", ".cpp"}
                and "test" not in q.parts
                and "fuzz" not in q.parts
            )

    for name, (pat, expected) in BODY.items():
        hits = [f for f in files if pat.search(f.read_text(encoding="utf-8", errors="replace"))]
        if expected not in hits:
            errors.append(f"{name} body not found at {expected.relative_to(ROOT)}")
        extras = [f for f in hits if f != expected]
        if extras:
            errors.append(
                f"{name} body also in "
                + ", ".join(str(f.relative_to(ROOT)) for f in extras)
            )

    if errors:
        print("FAIL: production consensus source audit")
        for e in errors:
            print(f"  - {e}")
        return 1
    print("PASS: production Consensus Kernel is handwritten C++; no extracted link")
    return 0


if __name__ == "__main__":
    sys.exit(main())

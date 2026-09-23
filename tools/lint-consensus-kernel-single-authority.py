#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""P054: single production Kernel path. Not a refinement. Does not switch ConnectBlock."""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from refinement_status import print_status, records_cpp_authority  # noqa: E402
STATUS = ROOT / "formal_verification/consensus_kernel_single_authority.json"
CONNECT = ROOT / "core/src/validation.cpp"
MAKEFILE = ROOT / "core/src/Makefile.am"
PHASE5 = ROOT / "formal_verification/coq/Phase5Equivalence/EquivalenceFramework.v"
LEGACY = ROOT / "formal_verification/coq/legacy_account_model/README.md"

BODY = {
    "GetBlockSubsidy": (
        re.compile(r"^CAmount GetBlockSubsidy\s*\([^;{]*\{", re.MULTILINE | re.DOTALL),
        ROOT / "core/src/consensus/rigor_issuance.cpp",
    ),
    "RigorIssuanceEpoch": (
        re.compile(r"inline int RigorIssuanceEpoch\s*\([^;{]*\{", re.MULTILINE | re.DOTALL),
        ROOT / "core/src/consensus/rigor_issuance.h",
    ),
    "CheckResMintArithmetic": (
        re.compile(r"inline constexpr bool CheckResMintArithmetic\s*\([^;{]*\{", re.MULTILINE | re.DOTALL),
        ROOT / "core/src/consensus/res_issuance.h",
    ),
    "ClassifyRigorTxout": (
        re.compile(r"^TxoutType ClassifyRigorTxout\s*\([^;{]*\{", re.MULTILINE | re.DOTALL),
        ROOT / "core/src/script/rigor_classify.cpp",
    ),
    "RigorSupplyDelta": (
        re.compile(r"^bool RigorSupplyDelta\s*\([^;{]*\{", re.MULTILINE | re.DOTALL),
        ROOT / "core/src/consensus/rigor_supply.cpp",
    ),
}

CONNECT_FORMULA = re.compile(
    r"nHeight\s*/\s*210000|nHeight\s*/\s*nSubsidyHalvingInterval|base\s*>>\s*halvings"
)

ASSEMBLY = re.compile(
    r"bool GetConvergenceDrawSetForBlock\s*\(.*?\n\}",
    re.MULTILINE | re.DOTALL,
)


def prod_files() -> list[Path]:
    files: list[Path] = []
    for p in (
        ROOT / "core/src/consensus",
        ROOT / "core/src/script",
        ROOT / "core/src/validation.cpp",
        ROOT / "core/src/connect_block_rigor_checks.cpp",
    ):
        if p.is_file():
            files.append(p)
        else:
            files.extend(
                q
                for q in p.rglob("*")
                if q.is_file()
                and q.suffix in {".h", ".cpp"}
                and "test" not in q.parts
                and "fuzz" not in q.parts
                and "bridge" not in q.parts
                and "shadow" not in q.parts
                and "kernel_migration" not in q.parts
                and "kernel_canary" not in q.parts
            )
    return files


def main() -> int:
    errors: list[str] = []
    st = json.loads(STATUS.read_text(encoding="utf-8"))
    if st.get("equivalence_proved"):
        errors.append("single-authority JSON flipped honesty flags")
    if not records_cpp_authority(st):
        errors.append("JSON must record C++ RigorConsensusKernel as production authority")
    if st.get("phase5_authority") != "NON_AUTHORITATIVE":
        errors.append("Phase5 must be NON_AUTHORITATIVE")

    if PHASE5.is_file() and "NON_AUTHORITATIVE" not in PHASE5.read_text(encoding="utf-8", errors="replace"):
        errors.append("Phase5 EquivalenceFramework.v missing NON_AUTHORITATIVE stamp")
    if LEGACY.is_file() and "DEAD_BUILD" not in LEGACY.read_text(encoding="utf-8", errors="replace"):
        errors.append("legacy_account_model README missing DEAD_BUILD registration")

    files = prod_files()
    for name, (pat, expected) in BODY.items():
        hits = [f for f in files if pat.search(f.read_text(encoding="utf-8", errors="replace"))]
        if expected not in hits:
            errors.append(f"{name} body missing at {expected.relative_to(ROOT)}")
        extras = [f for f in hits if f != expected]
        if extras:
            errors.append(f"{name} second body in " + ", ".join(str(x.relative_to(ROOT)) for x in extras))

    cb = CONNECT.read_text(encoding="utf-8", errors="replace")
    if "bool Chainstate::ConnectBlock" not in cb:
        errors.append("ConnectBlock missing")
    for needle in (
        "RigorConsensusKernel",
        "hashGenesisBlock",
    ):
        if needle not in cb:
            errors.append(f"ConnectBlock does not dispatch {needle}")
    if CONNECT_FORMULA.search(cb):
        errors.append("validation.cpp contains an issuance formula fragment")
    if "caml_startup" in cb or "consensus_kernel.ml" in cb:
        errors.append("ConnectBlock links extracted OCaml")

    makefile = MAKEFILE.read_text(encoding="utf-8", errors="replace")
    if "consensus/bridge/" in makefile or "caml_" in makefile:
        errors.append("Makefile.am would compile C ABI or OCaml into production")

    classify = (ROOT / "core/src/script/rigor_classify.cpp").read_text(encoding="utf-8")
    if "AssetType ClassifyRigorAsset" not in classify:
        errors.append("ClassifyRigorAsset wrapper missing")
    if "switch (ClassifyRigorTxout(scriptPubKey, sols))" not in classify:
        errors.append("AssetTypeFromScriptPubKey must call ClassifyRigorTxout")

    if errors:
        print("FAIL: consensus kernel single-authority audit")
        for e in errors:
            print(f"  - {e}")
        return 1
    print("PASS: production Kernel path is single; ConnectBlock is orchestration")
    print_status()
    return 0


if __name__ == "__main__":
    sys.exit(main())

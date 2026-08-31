#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""P061: release-build and extracted-oracle isolation audit.

Does not rewrite consensus. FAIL => EXTRACTED_KERNEL_LEAK_TO_PRODUCTION
or a ConnectBlock formula bypass.
"""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "core/src"
MAKEFILE = SRC / "Makefile.am"
VALIDATION = SRC / "validation.cpp"
INVENTORY = ROOT / "formal_verification/production_consensus_inventory.json"
MANIFEST = ROOT / "formal_verification/consensus_kernel_manifest.json"

REQUIRED = {
    "issuance": "GetBlockSubsidy",
    "res_schedule": "GetResMintReward",
    "supply": "RigorSupplyDelta",
    "draw": "ResolveConvergenceDraw",
    "classification": "ClassifyRigorTxout",
    "spend": "VerifyRigorPredicate",
    "pow": "PowBoundary",
}

LEAK = re.compile(
    r"consensus_kernel\.ml|consensus_kernel_c\.ml|caml_startup|\bcaml_|"
    r"libasmrun|EXTRACTED_CONSENSUS_KERNEL|"
    r"RIGOR_KERNEL_ABI_VERIFY_ONLY|RIGOR_KERNEL_SHADOW_VERIFY_ONLY|"
    r"consensus/shadow|consensus/bridge|consensus/txkernel|"
    r"consensus/kernel_migration|CONSENSUS_KERNEL_MIGRATION_VERIFY_ONLY|"
    r"consensus/kernel_canary|ENABLE_CONSENSUS_KERNEL_CANARY"
)

FORMULA = re.compile(
    r"nHeight\s*/\s*nSubsidyHalvingInterval|"
    r"base\s*>>\s*halvings"
)
WINNER = re.compile(r"\bSelectWinner\s*\(|\bRunSelection\s*\(|Mul256x256Full")
EVAL = re.compile(r"\bEvalScript\s*\(|\bVerifyScript\s*\(")


def strip_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    text = re.sub(r"//.*?$", "", text, flags=re.MULTILINE)
    return text


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

    inv = json.loads(INVENTORY.read_text(encoding="utf-8"))
    man = json.loads(MANIFEST.read_text(encoding="utf-8"))

    for domain, authority in REQUIRED.items():
        entry = inv.get(domain)
        if not isinstance(entry, dict) or entry.get("authority") != authority:
            errors.append(f"inventory {domain} authority must be {authority}")

    if inv.get("extracted_kernel", {}).get("role") != "VERIFICATION_ORACLE_ONLY":
        errors.append("extracted kernel is not VERIFICATION_ORACLE_ONLY")
    if inv.get("extracted_kernel", {}).get("linked_into_production") is not False:
        errors.append("inventory claims extracted kernel is linked")
    if inv.get("equivalence") is not False:
        errors.append("inventory must keep equivalence false")

    mapping = (
        ("issuance_authority", "GetBlockSubsidy"),
        ("res_authority", "GetResMintReward"),
        ("supply_authority", "RigorSupplyDelta"),
        ("draw_authority", "ResolveConvergenceDraw"),
        ("classify_authority", "ClassifyRigorTxout"),
        ("spend_authority", "VerifyRigorPredicate"),
    )
    for key, value in mapping:
        if man.get(key) != value:
            errors.append(f"manifest {key} drifted from {value}")

    makefile = MAKEFILE.read_text(encoding="utf-8", errors="replace")
    for m in LEAK.finditer(makefile):
        print("EXTRACTED_KERNEL_LEAK_TO_PRODUCTION")
        errors.append(f"Makefile.am leak: {m.group(0)}")

    validation = VALIDATION.read_text(encoding="utf-8", errors="replace")
    for m in LEAK.finditer(validation):
        print("EXTRACTED_KERNEL_LEAK_TO_PRODUCTION")
        errors.append(f"validation.cpp leak: {m.group(0)}")

    cb = extract_connectblock(validation)
    if not cb:
        errors.append("ConnectBlock not found")
    else:
        body = strip_comments(cb)
        for m in FORMULA.finditer(body):
            errors.append(f"ConnectBlock subsidy formula: {m.group(0)}")
        if WINNER.search(body):
            errors.append("ConnectBlock contains draw algorithm")
        if EVAL.search(body):
            errors.append("ConnectBlock contains predicate parser / EvalScript")
        if "IsWitnessProgram" in body and "ParseResMintScript" in body and "ParseConvergeScript" in body:
            errors.append("ConnectBlock contains classify parser copy")
        if "RigorConsensusKernel::ResolveDraw" not in cb:
            errors.append("ConnectBlock does not call RigorConsensusKernel::ResolveDraw")
        if "RigorConsensusKernel::EvaluateConnect" not in cb:
            errors.append("ConnectBlock does not call RigorConsensusKernel::EvaluateConnect")
        if re.search(r"\bResolveConvergenceDraw\s*\(", cb):
            errors.append("ConnectBlock bypasses Kernel draw")
        if re.search(r"\bRigorSupplyDelta\s*\(", cb):
            errors.append("ConnectBlock bypasses Kernel supply")

    if "VerifyRigorPredicate" not in validation:
        errors.append("validation.cpp does not call VerifyRigorPredicate")
    if "CheckProofOfWork" not in validation:
        errors.append("validation.cpp does not call CheckProofOfWork")

    if errors:
        print("FAIL: consensus release-build audit")
        for e in errors:
            print(f"  - {e}")
        return 1

    print("AUTHORITY_INVENTORY=PASS")
    print("EXTRACTED_KERNEL_ISOLATION=PASS")
    print("RELEASE_BUILD_AUDIT=PASS")
    print("PRODUCTION_KERNEL_SWITCHED=YES")
    print("extracted kernel remains VERIFICATION_ORACLE_ONLY")
    return 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""P067: migration shim is verify-only. No second authority. EXTRACTED_KERNEL_PRODUCTION stays false."""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from refinement_status import print_status, records_cpp_authority  # noqa: E402
SRC = ROOT / "core/src"
MIG = SRC / "consensus/kernel_migration"
MAKEFILE = SRC / "Makefile.am"
VALIDATION = SRC / "validation.cpp"
TEST_INC = SRC / "Makefile.test.include"
VECTORS = ROOT / "formal_verification/kernel_migration_vectors.json"

FORMULA = re.compile(
    r"nHeight\s*/\s*nSubsidyHalvingInterval|"
    r"base\s*>>\s*halvings|"
    r"Mul256x256Full|"
    r"oru_circulating_delta\s*="
)
SECOND_BODY = re.compile(
    r"^CAmount GetBlockSubsidy\s*\(|"
    r"inline constexpr CAmount GetResMintReward\s*\(|"
    r"^RigorIssuance ComputeRigorIssuance\s*\(|"
    r"^ConvergenceDrawOutcome ResolveConvergenceDraw\s*\(|"
    r"^TxoutType ClassifyRigorTxout\s*\(|"
    r"^bool VerifyRigorPredicate\s*\(|"
    r"^bool RigorSupplyDelta\s*\(",
    re.MULTILINE,
)
LEAK = re.compile(
    r"consensus_kernel\.ml|caml_startup|\bcaml_|libasmrun|"
    r"EXTRACTED_CONSENSUS_KERNEL"
)
REQUIRED_CALLS = (
    "GetBlockSubsidy",
    "ResDestroyCostAtHeight",
    "ComputeRigorIssuance",
    "ClassifyRigorTxout",
    "ClassifyRigorAsset",
    "RigorSupplyDelta",
    "VerifyRigorPredicateBoundary",
    "LedgerApply",
    "UTXOStateTransition",
)
GUARD = "CONSENSUS_KERNEL_MIGRATION_VERIFY_ONLY"


def fail(errors: list[str]) -> int:
    print("CONSENSUS_KERNEL_MIGRATION_DRIFT")
    print("FAIL: consensus kernel migration lint")
    for e in errors:
        print(f"  - {e}")
    return 1


def main() -> int:
    errors: list[str] = []
    if not MIG.is_dir():
        return fail(["missing core/src/consensus/kernel_migration"])

    texts = []
    for p in MIG.rglob("*"):
        if p.suffix in {".h", ".cpp"}:
            texts.append((p, p.read_text(encoding="utf-8", errors="replace")))

    if not texts:
        return fail(["empty kernel_migration directory"])

    joined = "\n".join(t for _, t in texts)
    for name in REQUIRED_CALLS:
        if name not in joined:
            errors.append(f"shim does not call existing authority {name}")

    for p, text in texts:
        if GUARD not in text:
            errors.append(f"{p.relative_to(ROOT)} missing {GUARD} guard")
        if SECOND_BODY.search(text):
            errors.append(f"{p.relative_to(ROOT)} reimplements a Kernel authority")
        if FORMULA.search(text):
            errors.append(f"{p.relative_to(ROOT)} contains a formula fragment")
        if LEAK.search(text):
            errors.append(f"{p.relative_to(ROOT)} references extracted OCaml")

    makefile = MAKEFILE.read_text(encoding="utf-8", errors="replace")
    if "consensus/kernel_migration" in makefile:
        errors.append("Makefile.am lists kernel_migration (production switch)")
    if "caml_" in makefile or "consensus_kernel.ml" in makefile:
        errors.append("Makefile.am links extracted OCaml")

    validation = VALIDATION.read_text(encoding="utf-8", errors="replace")
    if "kernel_migration" in validation or "KernelMigrationVerify" in validation:
        errors.append("validation.cpp references migration shim (production switch)")
    if GUARD in validation:
        errors.append("validation.cpp defines migration verify mode")

    if TEST_INC.is_file() and "test_kernel_migration_readiness.cpp" in TEST_INC.read_text(
        encoding="utf-8", errors="replace"
    ):
        errors.append("migration test must not join test_rigor")

    if not VECTORS.is_file():
        errors.append("missing formal_verification/kernel_migration_vectors.json")
    else:
        import json

        v = json.loads(VECTORS.read_text(encoding="utf-8"))
        if not records_cpp_authority(v):
            errors.append("vectors must record C++ production authority; extracted kernel is not production")
        if v.get("equivalence_proved") is not False:
            errors.append("vectors overclaim equivalence")

    if errors:
        return fail(errors)
    print("MIGRATION_BOUNDARY=READY")
    print("DUPLICATE_AUTHORITY=NONE")
    print_status()
    return 0


if __name__ == "__main__":
    sys.exit(main())

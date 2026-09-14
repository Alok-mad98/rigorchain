#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""P068: canary is verify-only. No second authority. EXTRACTED_KERNEL_PRODUCTION stays false."""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from refinement_status import print_status, records_cpp_authority  # noqa: E402
SRC = ROOT / "core/src"
CANARY = SRC / "consensus/kernel_canary"
MAKEFILE = SRC / "Makefile.am"
VALIDATION = SRC / "validation.cpp"
TEST_INC = SRC / "Makefile.test.include"
VECTORS = ROOT / "formal_verification/kernel_canary_vectors.json"

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
    r"EXTRACTED_CONSENSUS_KERNEL|ocamlopt"
)
REQUIRED_CPP = (
    "GetBlockSubsidy",
    "ResDestroyCostAtHeight",
    "ComputeRigorIssuance",
    "ClassifyRigorTxout",
    "ClassifyRigorAsset",
    "RigorSupplyDelta",
    "VerifyRigorPredicateBoundary",
)
REQUIRED_ABI = (
    "rigor_compute_issuance",
    "rigor_classify_script",
    "rigor_resolve_draw",
    "rigor_supply_delta",
    "rigor_coinbase_mature",
)
GUARD = "ENABLE_CONSENSUS_KERNEL_CANARY"


def fail(errors: list[str]) -> int:
    print("CONSENSUS_KERNEL_CANARY_DISCREPANCY")
    print("FAIL: consensus kernel canary lint")
    for e in errors:
        print(f"  - {e}")
    return 1


def main() -> int:
    errors: list[str] = []
    if not CANARY.is_dir():
        return fail(["missing core/src/consensus/kernel_canary"])

    texts = []
    for p in CANARY.rglob("*"):
        if p.suffix in {".h", ".cpp"}:
            texts.append((p, p.read_text(encoding="utf-8", errors="replace")))

    if not texts:
        return fail(["empty kernel_canary directory"])

    joined = "\n".join(t for _, t in texts)
    if "KernelCanaryExecutor" not in joined:
        errors.append("missing KernelCanaryExecutor")
    if "ConsensusKernelInput" not in joined:
        errors.append("missing ConsensusKernelInput")
    for name in REQUIRED_CPP:
        if name not in joined:
            errors.append(f"canary PATH_A does not call existing authority {name}")
    for name in REQUIRED_ABI:
        if name not in joined:
            errors.append(f"canary PATH_B does not call C ABI {name}")

    for p, text in texts:
        if GUARD not in text:
            errors.append(f"{p.relative_to(ROOT)} missing {GUARD} guard")
        if SECOND_BODY.search(text):
            errors.append(f"{p.relative_to(ROOT)} reimplements a Kernel authority")
        if FORMULA.search(text):
            errors.append(f"{p.relative_to(ROOT)} contains a formula fragment")
        if LEAK.search(text):
            errors.append(f"{p.relative_to(ROOT)} references extracted OCaml")
        if "GetConvergenceDrawSetForBlock" in text or "validation.cpp" in text:
            errors.append(f"{p.relative_to(ROOT)} leaks ConnectBlock / state assembly")

    makefile = MAKEFILE.read_text(encoding="utf-8", errors="replace")
    if "consensus/kernel_canary" in makefile:
        errors.append("Makefile.am lists kernel_canary (production switch)")
    if GUARD in makefile:
        errors.append("Makefile.am enables the canary in production")
    if "caml_" in makefile or "consensus_kernel.ml" in makefile:
        errors.append("Makefile.am links extracted OCaml")

    validation = VALIDATION.read_text(encoding="utf-8", errors="replace")
    if "kernel_canary" in validation or "KernelCanaryExecutor" in validation:
        errors.append("validation.cpp references canary (production switch)")
    if GUARD in validation:
        errors.append("validation.cpp enables the canary")

    if TEST_INC.is_file() and "test_consensus_kernel_canary.cpp" in TEST_INC.read_text(
        encoding="utf-8", errors="replace"
    ):
        errors.append("canary test must not join test_rigor")

    if not VECTORS.is_file():
        errors.append("missing formal_verification/kernel_canary_vectors.json")
    else:
        v = json.loads(VECTORS.read_text(encoding="utf-8"))
        if not records_cpp_authority(v):
            errors.append("vectors must record C++ production authority; extracted kernel is not production")
        if v.get("equivalence_proved") is not False:
            errors.append("vectors overclaim equivalence")
        if v.get("canary_default") != "OFF":
            errors.append("vectors must record canary default OFF")
        heights = set(v.get("height") or [])
        for h in (0, 1, 99, 100, 101):
            if h not in heights:
                errors.append(f"vectors missing height {h}")
        epochs = set(v.get("epoch") or [])
        for h in (209999, 210000, 419999, 420000):
            if h not in epochs:
                errors.append(f"vectors missing epoch {h}")
        tx = set(v.get("transaction") or [])
        for name in ("valid_spend", "invalid_spend", "bad_signature", "malformed_witness"):
            if name not in tx:
                errors.append(f"vectors missing transaction {name}")
        draw = set(v.get("draw") or [])
        for name in ("empty_F", "single_candidate", "equal_weight", "txid_tie_break", "zero_weight"):
            if name not in draw:
                errors.append(f"vectors missing draw {name}")
        supply = set(v.get("supply") or [])
        for name in ("mint", "burn", "fee", "invalid_delta"):
            if name not in supply:
                errors.append(f"vectors missing supply {name}")
        if v.get("deterministic_cases") != 10000:
            errors.append("vectors must require 10000 deterministic cases")

    if errors:
        return fail(errors)
    print("CANARY_IMPLEMENTED=TRUE")
    print("DUPLICATE_AUTHORITY=NONE")
    print_status()
    return 0


if __name__ == "__main__":
    sys.exit(main())

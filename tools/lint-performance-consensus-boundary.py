#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""P0-077: performance optimization must not grow a second consensus authority.

Scans the mainnet performance baseline artifacts and the verify-only kernels
they exercise. Production formulas stay unique. No consensus shortcut.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "core/src"
MAKEFILE = SRC / "Makefile.am"
VALIDATION = SRC / "validation.cpp"
LEDGER = SRC / "consensus/ledger/ledger_transition.cpp"
UTXO = SRC / "consensus/utxo/utxo_state_transition.h"
TXK = SRC / "consensus/txkernel/transaction_kernel_input.h"
KERNEL = SRC / "consensus/kernel/genesis_authority.cpp"
BENCH = ROOT / "benchmarks/rigor"
FUZZ = ROOT / "fuzz/performance_boundary"
TEST = SRC / "test/test_performance_consensus_regression.cpp"
RUNNER = ROOT / "tools/run_mainnet_performance_benchmark.py"
DOC = ROOT / "docs/performance/RIGOR_MAINNET_PERFORMANCE_BASELINE_P0_077.md"

SECOND_BODY = re.compile(
    r"^CAmount GetBlockSubsidy\s*\(|"
    r"inline constexpr CAmount GetResMintReward\s*\(|"
    r"^RigorIssuance ComputeRigorIssuance\s*\(|"
    r"^ConvergenceDrawOutcome ResolveConvergenceDraw\s*\(|"
    r"^AssetType ClassifyRigorAsset\s*\(|"
    r"^bool VerifyRigorPredicate\s*\(|"
    r"^bool RigorSupplyDelta\s*\(",
    re.MULTILINE,
)

FORMULA = re.compile(
    r"nHeight\s*/\s*nSubsidyHalvingInterval|"
    r"base\s*>>\s*halvings|"
    r"Mul256x256Full|"
    r"SIGHASH_ALL|"
    r"oru_circulating_delta\s*="
)

SHORTCUT = re.compile(
    r"skip[_ ]kernel|bypass[_ ]consensus|accept_without_kernel|"
    r"second[_ ]authority|approximate[_ ]issuance|fast[_ ]draw[_ ]winner",
    re.IGNORECASE,
)

FORBIDDEN_INC = re.compile(
    r'#\s*include\s*[<"](chain\.h|txdb\.h|wallet/|rpc/|validation\.cpp)'
)


def strip_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    text = re.sub(r"//.*?$", "", text, flags=re.MULTILINE)
    text = re.sub(r"#.*?$", "", text, flags=re.MULTILINE)
    return text


def iter_scan_files() -> list[Path]:
    out: list[Path] = []
    for root in (BENCH, FUZZ):
        if root.is_dir():
            out.extend(p for p in root.rglob("*") if p.suffix in {".h", ".cpp", ".py", ".sh"})
    for p in (TEST, RUNNER, LEDGER, UTXO, TXK, KERNEL):
        if p.is_file():
            out.append(p)
    return out


def main() -> int:
    errors: list[str] = []

    required = (BENCH, FUZZ, TEST, RUNNER, DOC, LEDGER, UTXO, TXK)
    for p in required:
        if not p.exists():
            errors.append(f"missing {p.relative_to(ROOT)}")

    benches = (
        "consensus_kernel_benchmark.cpp",
        "transaction_kernel_benchmark.cpp",
        "ledger_apply_benchmark.cpp",
        "utxo_state_transition_benchmark.cpp",
        "block_validation_benchmark.cpp",
    )
    if BENCH.is_dir():
        names = {p.name for p in BENCH.glob("*.cpp")}
        for b in benches:
            if b not in names:
                errors.append(f"missing benchmarks/rigor/{b}")

    for path in iter_scan_files():
        raw = path.read_text(encoding="utf-8", errors="replace")
        text = strip_comments(raw)
        rel = path.relative_to(ROOT)
        if path not in (LEDGER, UTXO, TXK, KERNEL) and SECOND_BODY.search(text):
            errors.append(f"second authority body in {rel}")
        if path != KERNEL and FORMULA.search(text):
            errors.append(f"formula fragment in {rel}")
        if SHORTCUT.search(text):
            errors.append(f"consensus shortcut token in {rel}")
        if path in (LEDGER, UTXO, TXK):
            for line in raw.splitlines():
                s = line.strip()
                if s.startswith("#include") and FORBIDDEN_INC.search(s):
                    errors.append(f"forbidden include in {rel}: {s}")

    ledger = Path(LEDGER).read_text(encoding="utf-8", errors="replace")
    if "LedgerKernelOutput LedgerApply" not in ledger:
        errors.append("LedgerApply unique entry missing")
    if "GetBlockSubsidy" in ledger or "ComputeRigorIssuance" in ledger:
        errors.append("LedgerApply grew an issuance authority")
    if "ResolveConvergenceDraw" in ledger:
        errors.append("LedgerApply grew a draw authority")

    utxo = Path(UTXO).read_text(encoding="utf-8", errors="replace")
    for name in (
        "ComputeRigorIssuance",
        "ResolveConvergenceDraw",
        "ClassifyRigorAsset",
        "VerifyRigorPredicateBoundary",
        "LedgerApply",
    ):
        if name not in utxo:
            errors.append(f"UTXOStateTransition does not call {name}")
    if SECOND_BODY.search(utxo):
        errors.append("UTXOStateTransition reimplements a Kernel authority")

    kernel = Path(KERNEL).read_text(encoding="utf-8", errors="replace")
    if "return ComputeRigorIssuance" not in kernel:
        errors.append("RigorConsensusKernel::Issuance is no longer the unique wrapper")
    if "return ClassifyRigorAsset" not in kernel:
        errors.append("RigorConsensusKernel::Classify is no longer the unique wrapper")

    makefile = Path(MAKEFILE).read_text(encoding="utf-8", errors="replace")
    if "consensus/ledger" in makefile:
        errors.append("Makefile.am compiles LedgerApply into production")
    if "consensus/utxo" in makefile:
        errors.append("Makefile.am compiles UTXOStateTransition into production")
    if "caml_" in makefile or "consensus_kernel.ml" in makefile:
        errors.append("Makefile.am links extracted OCaml")

    validation = Path(VALIDATION).read_text(encoding="utf-8", errors="replace")
    if "UTXOStateTransition" in validation or "consensus/utxo" in validation:
        errors.append("validation.cpp calls UTXOStateTransition")
    if "LedgerApply" in validation or "consensus/ledger" in validation:
        errors.append("validation.cpp calls LedgerApply")
    if re.search(r"nHeight\s*/\s*nSubsidyHalvingInterval|base\s*>>\s*halvings", validation):
        errors.append("validation.cpp gained an issuance formula fragment")

    if errors:
        print("FAIL: performance consensus boundary lint")
        for e in errors:
            print(f"  - {e}")
        return 1

    print("PERFORMANCE_CONSENSUS_BOUNDARY=PASS")
    print("SECOND_AUTHORITY=NO")
    print("FORMULA_DUPLICATION=NO")
    print("CONSENSUS_SHORTCUT=NO")
    print("PRODUCTION_CONSENSUS_MODIFIED=NO")
    return 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""P070: canary must not mutate consensus, replace authority, or touch DB."""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "core/src"
CANARY_DIR = SRC / "consensus/kernel_canary"
RUNTIME = SRC / "node/kernel_runtime_canary.cpp"
RUNTIME_H = SRC / "node/kernel_runtime_canary.h"
VALIDATION = SRC / "validation.cpp"
MAKEFILE = SRC / "Makefile.am"

MUTATION = re.compile(
    r"\bAbortNode\s*\(|"
    r"\bFlushStateToDisk\s*\(|"
    r"\bSetBestBlock\s*\(|"
    r"\bCCoinsView(?:Cache)?\b|"
    r"\bCDBWrapper\b|"
    r"\bCDBBatch\b|"
    r"leveldb::|"
    r"\bdb\.Write\s*\(|"
    r"\bWriteBatch\s*\("
)
SECOND_BODY = re.compile(
    r"^CAmount GetBlockSubsidy\s*\([^;{]*\{|"
    r"inline constexpr CAmount GetResMintReward\s*\([^;{]*\{|"
    r"^RigorIssuance ComputeRigorIssuance\s*\([^;{]*\{|"
    r"^ConvergenceDrawOutcome ResolveConvergenceDraw\s*\([^;{]*\{|"
    r"^bool RigorSupplyDelta\s*\([^;{]*\{",
    re.MULTILINE | re.DOTALL,
)


def fail(errors: list[str]) -> int:
    print("CONSENSUS_KERNEL_CANARY_NOT_READY")
    print("FAIL: production canary boundary")
    for e in errors:
        print(f"  - {e}")
    return 1


def main() -> int:
    errors: list[str] = []
    files = [RUNTIME, RUNTIME_H]
    if CANARY_DIR.is_dir():
        files.extend(sorted(p for p in CANARY_DIR.rglob("*") if p.suffix in {".h", ".cpp"}))
    for p in files:
        if not p.is_file():
            errors.append(f"missing {p.relative_to(ROOT)}")
            continue
        text = p.read_text(encoding="utf-8", errors="replace")
        if MUTATION.search(text):
            errors.append(f"{p.relative_to(ROOT)} mutates chainstate / DB / AbortNode")
        if SECOND_BODY.search(text):
            errors.append(f"{p.relative_to(ROOT)} replaces a Kernel authority")
        if "fallback candidate" in text.lower() or "auto repair" in text.lower() or "auto-repair" in text.lower():
            if "Never auto-repair" not in text and "never auto-repair" not in text.lower():
                errors.append(f"{p.relative_to(ROOT)} mentions fallback/repair")

    ctl = (CANARY_DIR / "canary_controller.h").read_text(encoding="utf-8", errors="replace")
    for needle in (
        "ConsensusKernelCanaryController",
        "Enable",
        "Disable",
        "Status",
        "CANARY_AVAILABLE",
        "CANARY_ENABLED_DEFAULT",
        "PRODUCTION_MAINNET_CANARY_ARMED",
        "OnDiscrepancy",
    ):
        if needle not in ctl:
            errors.append(f"controller missing {needle}")
    if "inline constexpr bool PRODUCTION_MAINNET_CANARY_ARMED = true" not in ctl:
        errors.append("mainnet canary must be armed for P071 compare-only")
    if "inline constexpr bool CANARY_ENABLED_DEFAULT = false" not in ctl:
        errors.append("CANARY_ENABLED default must be FALSE")

    metrics = (CANARY_DIR / "canary_metrics.h").read_text(encoding="utf-8", errors="replace")
    for needle in (
        "ConsensusKernelCanaryMetrics",
        "blocks_checked",
        "comparisons",
        "discrepancies",
        "runtime_ms_cpp",
        "runtime_ms_candidate",
        "memory_usage",
    ):
        if needle not in metrics:
            errors.append(f"metrics missing {needle}")

    makefile = MAKEFILE.read_text(encoding="utf-8", errors="replace")
    if "consensus/kernel_canary" in makefile:
        errors.append("Makefile.am lists kernel_canary (production switch)")
    if "kernel_runtime_canary.cpp" in makefile:
        errors.append("Makefile.am still compiles runtime canary")

    validation = VALIDATION.read_text(encoding="utf-8", errors="replace")
    if "kernel_canary" in validation or "KernelCanaryExecutor" in validation:
        errors.append("ConnectBlock orchestration references canary TU")
    if re.search(r"AbortNode\s*\([^)]*KERNEL_RUNTIME_CANARY", validation):
        errors.append("ConnectBlock still AbortNode on canary discrepancy")
    if "RigorObserveConnectBlockRuntimeCanary" in validation:
        errors.append("ConnectBlock still has canary dual-path")

    runtime = RUNTIME.read_text(encoding="utf-8", errors="replace")
    if "return true;" not in runtime:
        errors.append("runtime canary must fail-open to C++ authority")
    if "OnDiscrepancy" not in runtime:
        errors.append("runtime canary does not disable on discrepancy")

    if errors:
        return fail(errors)
    print("CANARY_BOUNDARY=PASS")
    print("CANARY_AVAILABLE=TRUE")
    print("CANARY_ENABLED=FALSE")
    print("ROLLBACK_READY=TRUE")
    print("PRODUCTION_KERNEL_SWITCHED=FALSE")
    return 0


if __name__ == "__main__":
    sys.exit(main())

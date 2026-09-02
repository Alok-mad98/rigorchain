#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""P073: genesis Kernel is the unique production authority. No OCaml. No dual-path."""

from __future__ import annotations

import json
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from refinement_status import print_status, records_cpp_authority  # noqa: E402
SRC = ROOT / "core/src"
VALIDATION = SRC / "validation.cpp"
MAKEFILE = SRC / "Makefile.am"
INIT = SRC / "init.cpp"
KERNEL = SRC / "consensus/kernel/genesis_authority.cpp"
KERNEL_H = SRC / "consensus/kernel/genesis_authority.h"
MANIFEST = ROOT / "formal_verification/genesis_consensus_authority_manifest.json"

FORMULA = re.compile(
    r"nHeight\s*/\s*nSubsidyHalvingInterval|"
    r"base\s*>>\s*halvings"
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
    if not KERNEL.is_file() or not KERNEL_H.is_file():
        errors.append("missing genesis Kernel authority")

    header = KERNEL_H.read_text(encoding="utf-8", errors="replace")
    if 'PRODUCTION_CONSENSUS_AUTHORITY[] = "RigorConsensusKernel(C++)"' not in header:
        errors.append("PRODUCTION_CONSENSUS_AUTHORITY must be RigorConsensusKernel(C++)")
    if "inline constexpr bool EXTRACTED_KERNEL_PRODUCTION = false" not in header:
        errors.append("EXTRACTED_KERNEL_PRODUCTION must be false")
    if "inline constexpr bool COMPCERT_VST_DEFERRED = true" not in header:
        errors.append("COMPCERT_VST_DEFERRED must be true")
    if 'COMPCERT_VST_CANDIDATE[] = "MakeKernelInput"' not in header:
        errors.append("COMPCERT_VST_CANDIDATE must be MakeKernelInput")
    if "GENESIS_EQUIVALENCE_PROVED = false" not in header:
        errors.append("must not claim EQUIVALENCE_PROVED")

    body = KERNEL.read_text(encoding="utf-8", errors="replace")
    if FORMULA.search(body):
        errors.append("genesis Kernel recodes an issuance formula")
    for needle in (
        "ResolveConvergenceDraw",
        "ConnectEvaluateCoinbaseResMint",
        "RigorSupplyDelta",
        "ComputeRigorIssuance",
        "ClassifyRigorAsset",
    ):
        if needle not in body:
            errors.append(f"Kernel does not dispatch {needle}")

    makefile = MAKEFILE.read_text(encoding="utf-8", errors="replace")
    if "consensus/kernel/genesis_authority.cpp" not in makefile:
        errors.append("genesis Kernel not compiled into production")
    tracked_logs = subprocess.run(
        ["git", "ls-files", "logs/"],
        cwd=str(ROOT),
        capture_output=True,
        text=True,
        check=False,
    )
    if tracked_logs.returncode != 0:
        errors.append("cannot verify logs/ is uncommitted (git ls-files failed)")
    leftover = [ln.strip() for ln in tracked_logs.stdout.splitlines() if ln.strip()]
    if leftover:
        errors.append(f"logs/ must not be committed: {leftover}")
    if "kernel_runtime_canary.cpp" in makefile:
        errors.append("production still compiles runtime canary")
    if "caml_" in makefile or "consensus_kernel.ml" in makefile:
        errors.append("Makefile.am links extracted OCaml")
    if "consensus/kernel_canary" in makefile:
        errors.append("kernel_canary compiled into production")
    if "consensus/kernel_migration" in makefile:
        errors.append("kernel_migration compiled into production")
    if "consensus/shadow" in makefile:
        errors.append("shadow compiled into production")
    if "consensus/bridge/" in makefile:
        errors.append("C ABI compiled into production")

    init = INIT.read_text(encoding="utf-8", errors="replace")
    if "enableconsensuskernelcanary" in init:
        errors.append("production still registers canary flag")
    if "RigorKernelRuntimeCanaryStartupCheck" in init:
        errors.append("production still starts canary")

    validation = VALIDATION.read_text(encoding="utf-8", errors="replace")
    cb = extract_connectblock(validation)
    if not cb:
        errors.append("ConnectBlock not found")
    else:
        if "RigorConsensusKernel::ResolveDraw" not in cb:
            errors.append("ConnectBlock does not call Kernel ResolveDraw")
        if "RigorConsensusKernel::EvaluateConnect" not in cb:
            errors.append("ConnectBlock does not call Kernel EvaluateConnect")
        if re.search(r"\bResolveConvergenceDraw\s*\(", cb):
            errors.append("ConnectBlock still calls ResolveConvergenceDraw")
        if re.search(r"\bRigorSupplyDelta\s*\(", cb):
            errors.append("ConnectBlock still computes supply delta")
        if re.search(r"\bConnectEvaluateCoinbaseResMint\s*\(", cb):
            errors.append("ConnectBlock still evaluates mint directly")
        if "RigorObserveConnectBlockRuntimeCanary" in cb:
            errors.append("ConnectBlock still dual-path canary")
        if FORMULA.search(cb):
            errors.append("ConnectBlock contains an issuance formula")
        if "caml_startup" in cb or "consensus_kernel.ml" in cb:
            errors.append("ConnectBlock links extracted OCaml")

    if not MANIFEST.is_file():
        errors.append("missing genesis_consensus_authority_manifest.json")
    else:
        man = json.loads(MANIFEST.read_text(encoding="utf-8"))
        if not records_cpp_authority(man):
            errors.append("manifest must record C++ production authority")
        if man.get("EQUIVALENCE_PROVED") is not False:
            errors.append("manifest overclaims equivalence")
        if man.get("REFINEMENT_PROVED") is not True:
            errors.append("manifest dropped refinement")
        if man.get("extracted_kernel_role", {}).get("ocaml_in_production") is not False:
            errors.append("manifest allows OCaml in production")

    if errors:
        print("FAIL: genesis consensus authority")
        for e in errors:
            print(f"  - {e}")
        return 1
    print("CONSENSUS_KERNEL_GENESIS_AUTHORITY = FINALIZED")
    print("SINGLE_PRODUCTION_AUTHORITY = TRUE")
    print_status()
    print("MAINNET_READY = TRUE")
    return 0


if __name__ == "__main__":
    sys.exit(main())

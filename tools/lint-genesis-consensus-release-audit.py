#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""P075: genesis consensus release audit. No migration. No production canary."""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from refinement_status import print_status  # noqa: E402
SRC = ROOT / "core/src"
VALIDATION = SRC / "validation.cpp"
MAKEFILE = SRC / "Makefile.am"
INIT = SRC / "init.cpp"
KERNEL_H = SRC / "consensus/kernel/genesis_authority.h"
AUDIT = ROOT / "formal_verification/genesis_consensus_release_audit.json"
VECTORS = ROOT / "formal_verification/genesis_consensus_release_vectors.json"
ADVERSARIAL = SRC / "test/test_consensus_kernel_adversarial.cpp"
LEDGER_TEST = SRC / "test/test_ledger_transition_refinement.cpp"


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

    makefile = MAKEFILE.read_text(encoding="utf-8", errors="replace")
    if "consensus/kernel/genesis_authority.cpp" not in makefile:
        errors.append("production does not compile RigorConsensusKernel")
    for needle, label in (
        ("kernel_runtime_canary.cpp", "runtime canary"),
        ("consensus/kernel_canary", "kernel_canary"),
        ("consensus/kernel_migration", "kernel_migration"),
        ("consensus/shadow", "shadow"),
        ("consensus/bridge/", "extracted C ABI"),
        ("consensus/ledger", "LedgerApply TU"),
        ("caml_", "OCaml runtime"),
        ("consensus_kernel.ml", "extracted OCaml kernel"),
    ):
        if needle in makefile:
            errors.append(f"production Makefile.am still links {label}")

    init = INIT.read_text(encoding="utf-8", errors="replace")
    if "enableconsensuskernelcanary" in init:
        errors.append("production still registers canary flag")

    header = KERNEL_H.read_text(encoding="utf-8", errors="replace")
    if 'PRODUCTION_CONSENSUS_AUTHORITY[] = "RigorConsensusKernel(C++)"' not in header:
        errors.append("PRODUCTION_CONSENSUS_AUTHORITY must be RigorConsensusKernel(C++)")
    if "inline constexpr bool EXTRACTED_KERNEL_PRODUCTION = false" not in header:
        errors.append("EXTRACTED_KERNEL_PRODUCTION must be false")
    if "GENESIS_EQUIVALENCE_PROVED = false" not in header:
        errors.append("must not claim EQUIVALENCE_PROVED")

    validation = VALIDATION.read_text(encoding="utf-8", errors="replace")
    if "CheckProofOfWork(" not in validation:
        errors.append("CheckProofOfWork missing from validation.cpp")
    if "VerifyRigorPredicate(" not in validation:
        errors.append("Transaction Kernel missing from validation.cpp")
    cb = extract_connectblock(validation)
    if not cb:
        errors.append("ConnectBlock not found")
    else:
        if "RigorConsensusKernel::EvaluateConnect" not in cb:
            errors.append("ConnectBlock does not call RigorConsensusKernel")
        if "RigorConsensusKernel::ResolveDraw" not in cb:
            errors.append("ConnectBlock does not call Kernel ResolveDraw")
        if re.search(r"\bResolveConvergenceDraw\s*\(", cb):
            errors.append("ConnectBlock still contains draw formula call")
        if re.search(r"\bRigorSupplyDelta\s*\(", cb):
            errors.append("ConnectBlock still contains supply formula call")
        if re.search(r"\bConnectEvaluateCoinbaseResMint\s*\(", cb):
            errors.append("ConnectBlock still contains mint formula call")
        if "RigorObserveConnectBlockRuntimeCanary" in cb:
            errors.append("ConnectBlock still contains canary infrastructure")
        if "LedgerApply(" in cb:
            errors.append("ConnectBlock must not embed LedgerApply")
        if "caml_startup" in cb:
            errors.append("ConnectBlock links extracted oracle runtime")

    if "AddCoins(" not in validation or "SpendCoin(" not in validation:
        errors.append("UTXO mutation (AddCoins/SpendCoin) missing")

    if not AUDIT.is_file() or not VECTORS.is_file():
        errors.append("missing genesis release audit JSON")
    else:
        audit = json.loads(AUDIT.read_text(encoding="utf-8"))
        if audit.get("GENESIS_CONSENSUS_READY") is not True:
            errors.append("GENESIS_CONSENSUS_READY")
        if audit.get("REFINEMENT_PROVED") is not True:
            errors.append("REFINEMENT_PROVED")
        if audit.get("EQUIVALENCE_PROVED") is not False:
            errors.append("EQUIVALENCE_PROVED overclaim")
        vec = json.loads(VECTORS.read_text(encoding="utf-8"))
        g0 = vec.get("height_0") or {}
        if g0.get("oru_actual_issuance") != 0:
            errors.append("height 0 ORU actual")
        if g0.get("res_actual_issuance") != 0:
            errors.append("height 0 RES actual")
        if g0.get("res_schedule") != 40000000:
            errors.append("height 0 RES schedule")
        if g0.get("draw") != "none":
            errors.append("height 0 draw")
        by_h = {row["height"]: row for row in vec.get("heights") or []}
        for h in (0, 1, 99, 100, 101, 210000):
            if h not in by_h:
                errors.append(f"missing vector height {h}")
        if by_h.get(1, {}).get("oru_subsidy") != 100000000000:
            errors.append("height 1 ORU subsidy")
        if by_h.get(210000, {}).get("oru_subsidy") != 50000000000:
            errors.append("height 210000 ORU subsidy")
        if by_h.get(210000, {}).get("res_schedule") != 14142196:
            errors.append("height 210000 RES schedule")
        if by_h.get(101, {}).get("maturity_spend_of_height_1") != "mature":
            errors.append("height 101 maturity")
        if by_h.get(100, {}).get("maturity_spend_of_height_1") != "immature":
            errors.append("height 100 maturity")

    adv = ADVERSARIAL.read_text(encoding="utf-8", errors="replace") if ADVERSARIAL.is_file() else ""
    for needle in (
        "transaction_kernel_adversarial_fail_closed",
        "classification_adversarial",
        "draw_kernel_adversarial",
        "maturity_adversarial_early_spend",
        "supply_delta_adversarial",
    ):
        if needle not in adv:
            errors.append(f"adversarial suite missing {needle}")

    led = LEDGER_TEST.read_text(encoding="utf-8", errors="replace") if LEDGER_TEST.is_file() else ""
    for needle in ("double spend", "overflow", "unknown asset", "maturity 99", "maturity 100", "maturity 101"):
        if needle not in led:
            errors.append(f"ledger attack pin missing {needle}")

    if errors:
        print("FAIL: genesis consensus release audit")
        for e in errors:
            print(f"  - {e}")
        return 1
    print("GENESIS_CONSENSUS_READY = TRUE")
    print_status()
    print("PRODUCTION_ISOLATION = PASS")
    print("DUPLICATE_FORMULA = NONE")
    print("CANARY_LINKED = FALSE")
    print("SHADOW_LINKED = FALSE")
    print("MIGRATION_LINKED = FALSE")
    print("EXTRACTED_ORACLE_LINKED = FALSE")
    return 0


if __name__ == "__main__":
    sys.exit(main())

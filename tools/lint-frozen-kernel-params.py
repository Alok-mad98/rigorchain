#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""P1-3: live Coq issuance constants must match C++ ORU_FLAT_* / epoch."""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
COQ = ROOT / "formal_verification/coq/ResSchedule.v"
ISSUANCE = ROOT / "formal_verification/rigor_v01/IssuanceHeightBoundary.v"
LOCK = ROOT / "core/src/consensus/rigor_consensus_lock.h"
AMOUNT = ROOT / "core/src/consensus/amount.h"
CHAIN = ROOT / "core/src/kernel/chainparams.cpp"


def cpp_int(path: Path, name: str) -> int:
    text = path.read_text(encoding="utf-8")
    m = re.search(rf"inline\s+constexpr\s+\w+\s+{name}\s*=\s*([^;]+);", text)
    if not m:
        raise SystemExit(f"FAIL: missing {name} in {path}")
    expr = m.group(1).strip()
    coin = re.search(
        r"static\s+constexpr\s+CAmount\s+COIN\s*=\s*(\d+)",
        AMOUNT.read_text(encoding="utf-8"),
    )
    if not coin:
        raise SystemExit("FAIL: missing COIN")
    coin_v = int(coin.group(1))
    expr = expr.replace("LL", "").replace("ll", "")
    expr = re.sub(r"\bCOIN\b", str(coin_v), expr)
    expr = re.sub(r"\s+", "", expr)
    if not re.fullmatch(r"[0-9*+]+", expr):
        raise SystemExit(f"FAIL: cannot evaluate {name} = {expr}")
    return int(eval(expr, {"__builtins__": {}}, {}))  # noqa: S307 — digits and * only


def coq_z(path: Path, name: str) -> int:
    text = path.read_text(encoding="utf-8")
    m = re.search(rf"Definition\s+{name}\s*:\s*Z\s*:=\s*([^%.]+)", text)
    if not m:
        raise SystemExit(f"FAIL: missing {name} in {path}")
    expr = m.group(1).strip().rstrip(".")
    coin = re.search(
        r"Definition\s+COIN\s*:\s*Z\s*:=\s*(\d+)",
        path.read_text(encoding="utf-8"),
    )
    coin_v = int(coin.group(1)) if coin else 100000000
    expr = re.sub(r"\bCOIN\b", str(coin_v), expr)
    expr = re.sub(r"\s+", "", expr)
    if not re.fullmatch(r"[0-9*+]+", expr):
        raise SystemExit(f"FAIL: cannot evaluate Coq {name} = {expr}")
    return int(eval(expr, {"__builtins__": {}}, {}))  # noqa: S307 — digits and * only


def main() -> int:
    errors: list[str] = []
    cpp_base = cpp_int(LOCK, "ORU_FLAT_BLOCK_SUBSIDY")
    cpp_interval = cpp_int(LOCK, "RES_REWARD_EPOCH")
    cpp_genesis = cpp_int(LOCK, "ORU_GENESIS_BLOCK_SUBSIDY")
    coq_base = coq_z(COQ, "ORU_FLAT_BLOCK_SUBSIDY")
    coq_interval = coq_z(COQ, "RES_REWARD_EPOCH")
    issuance = ISSUANCE.read_text(encoding="utf-8")

    if coq_base != cpp_base:
        errors.append(f"ORU_FLAT_BLOCK_SUBSIDY Coq {coq_base} != C++ {cpp_base}")
    if coq_interval != cpp_interval:
        errors.append(f"RES_REWARD_EPOCH Coq {coq_interval} != C++ {cpp_interval}")
    if cpp_genesis != 0:
        errors.append(f"ORU_GENESIS_BLOCK_SUBSIDY {cpp_genesis} != 0")
    if "if h <=? 0 then 0 else ORU_FLAT_SATS" not in issuance:
        errors.append("IssuanceHeightBoundary must be genesis-0 / flat base")

    chain = CHAIN.read_text(encoding="utf-8")
    if "nSubsidyHalvingInterval" in chain or "nBlockSubsidyBase" in chain:
        errors.append("chainparams still assigns retired RFC-019 subsidy Params fields")
    if "nSubsidyHalvingInterval" in LOCK.read_text(encoding="utf-8"):
        errors.append("lock header still names nSubsidyHalvingInterval")

    if errors:
        print("FAIL: FrozenKernelParams drifted from C++ constants")
        for e in errors:
            print(f"  - {e}")
        return 1

    print("FROZEN_KERNEL_PARAMS=PASS")
    print(f"base={coq_base} interval={coq_interval} genesis={cpp_genesis}")
    print("observational calendar RES_REWARD_EPOCH is outside GetBlockSubsidy")
    return 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""P1-3: FrozenKernelParams literals must match C++ ORU_FLAT_* constants."""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
COQ = ROOT / "formal_verification/coq/ConsensusKernelRefinement.v"
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


def coq_frozen(path: Path) -> tuple[int, int, int]:
    text = path.read_text(encoding="utf-8")
    m = re.search(
        r"Definition\s+FrozenKernelParams\s*:.*?cpp_base\s*:=\s*(\d+)\s*;"
        r"\s*cpp_interval\s*:=\s*(\d+)\s*;"
        r"\s*cpp_max_shifts\s*:=\s*(\d+)",
        text,
        re.DOTALL,
    )
    if not m:
        raise SystemExit("FAIL: FrozenKernelParams not found")
    return int(m.group(1)), int(m.group(2)), int(m.group(3))


def genesis_const(path: Path) -> int:
    text = path.read_text(encoding="utf-8")
    m = re.search(
        r"Definition\s+ORU_GENESIS_BLOCK_SUBSIDY_Z\s*:\s*Z\s*:=\s*(\d+)\s*\.",
        text,
    )
    if not m:
        raise SystemExit("FAIL: ORU_GENESIS_BLOCK_SUBSIDY_Z missing")
    return int(m.group(1))


def main() -> int:
    errors: list[str] = []
    base, interval, shifts = coq_frozen(COQ)
    cpp_base = cpp_int(LOCK, "ORU_FLAT_BLOCK_SUBSIDY")
    cpp_interval = cpp_int(LOCK, "RES_REWARD_EPOCH")
    cpp_genesis = cpp_int(LOCK, "ORU_GENESIS_BLOCK_SUBSIDY")
    coq_genesis = genesis_const(COQ)
    coq_text = COQ.read_text(encoding="utf-8")

    if base != cpp_base:
        errors.append(f"cpp_base {base} != ORU_FLAT_BLOCK_SUBSIDY {cpp_base}")
    if interval != cpp_interval:
        errors.append(f"cpp_interval {interval} != RES_REWARD_EPOCH {cpp_interval}")
    if shifts != 0:
        errors.append(f"cpp_max_shifts {shifts} != 0 (RFC-020 has no halvings)")
    if coq_genesis != cpp_genesis:
        errors.append(
            f"ORU_GENESIS_BLOCK_SUBSIDY_Z {coq_genesis} != C++ {cpp_genesis}"
        )
    if "if h <=? 0 then ORU_GENESIS_BLOCK_SUBSIDY_Z" not in coq_text:
        errors.append("cpp_GetBlockSubsidy must be genesis-0 / flat base")
    if "Definition refinement_proved : bool := false" not in coq_text:
        errors.append("RFC-020 requires refinement_proved = false; do not flip the kernel-wide flag")

    chain = CHAIN.read_text(encoding="utf-8")
    if "nSubsidyHalvingInterval" in chain or "nBlockSubsidyBase" in chain:
        errors.append("chainparams still assigns retired RFC-019 subsidy Params fields")
    if "nSubsidyHalvingInterval" in LOCK.read_text(encoding="utf-8"):
        errors.append("lock header still names nSubsidyHalvingInterval")
    if "FrozenKernelParams does not apply to regtest" not in coq_text:
        errors.append("Coq must declare regtest observational calendar out of FrozenKernelParams scope")

    if errors:
        print("FAIL: FrozenKernelParams drifted from C++ constants")
        for e in errors:
            print(f"  - {e}")
        return 1

    print("FROZEN_KERNEL_PARAMS=PASS")
    print(f"base={base} interval={interval} max_shifts={shifts} genesis={coq_genesis}")
    print("observational calendar RES_REWARD_EPOCH is outside GetBlockSubsidy")
    return 0


if __name__ == "__main__":
    sys.exit(main())

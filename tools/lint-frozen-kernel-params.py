#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""P1-3: FrozenKernelParams literals must match C++ ORU_* constants."""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
COQ = ROOT / "formal_verification/coq/ConsensusKernelRefinement.v"
LOCK = ROOT / "core/src/consensus/rigor_consensus_lock.h"
AMOUNT = ROOT / "core/src/consensus/amount.h"
CHAIN = ROOT / "core/src/kernel/chainparams.cpp"

REGTEST_INTERVAL_RE = re.compile(
    r"nSubsidyHalvingInterval\s*=\s*150\s*;"
)


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
    cpp_base = cpp_int(LOCK, "ORU_INITIAL_BLOCK_SUBSIDY")
    cpp_interval = cpp_int(LOCK, "ORU_SUBSIDY_HALVING_INTERVAL")
    cpp_shifts = cpp_int(LOCK, "ORU_SUBSIDY_MAX_SHIFTS")
    cpp_genesis = cpp_int(LOCK, "ORU_GENESIS_BLOCK_SUBSIDY")
    coq_genesis = genesis_const(COQ)

    if base != cpp_base:
        errors.append(f"cpp_base {base} != ORU_INITIAL_BLOCK_SUBSIDY {cpp_base}")
    if interval != cpp_interval:
        errors.append(f"cpp_interval {interval} != ORU_SUBSIDY_HALVING_INTERVAL {cpp_interval}")
    if shifts != cpp_shifts:
        errors.append(f"cpp_max_shifts {shifts} != ORU_SUBSIDY_MAX_SHIFTS {cpp_shifts}")
    if coq_genesis != cpp_genesis:
        errors.append(
            f"ORU_GENESIS_BLOCK_SUBSIDY_Z {coq_genesis} != C++ {cpp_genesis}"
        )

    cpp_min = cpp_base >> cpp_shifts
    lock_text = LOCK.read_text(encoding="utf-8")
    if "ORU_MIN_BLOCK_SUBSIDY = (ORU_INITIAL_BLOCK_SUBSIDY >> ORU_SUBSIDY_MAX_SHIFTS)" not in lock_text.replace(" ", ""):
        # allow whitespace variants
        if not re.search(
            r"ORU_MIN_BLOCK_SUBSIDY\s*=\s*\(\s*ORU_INITIAL_BLOCK_SUBSIDY\s*>>\s*ORU_SUBSIDY_MAX_SHIFTS\s*\)",
            lock_text,
        ):
            errors.append("C++ ORU_MIN_BLOCK_SUBSIDY is not derived from base >> max_shifts")
    coq_text = COQ.read_text(encoding="utf-8")
    if "Z.shiftr (cpp_base FrozenKernelParams) (cpp_max_shifts FrozenKernelParams)" not in coq_text:
        errors.append("ORU_MIN_BLOCK_SUBSIDY_Z must be Z.shiftr of FrozenKernelParams")
    if "cpp_interval FrozenKernelParams * cpp_max_shifts FrozenKernelParams" not in coq_text:
        errors.append("ORU_SUBSIDY_CAP_HEIGHT must be interval * max_shifts")
    expected_cap = cpp_interval * cpp_shifts
    if expected_cap != 630000:
        errors.append(f"cap height {expected_cap} != 210000*3")
    if cpp_min != 12500000000:
        errors.append(f"ORU_MIN {cpp_min} drifted")

    chain = CHAIN.read_text(encoding="utf-8")
    if not REGTEST_INTERVAL_RE.search(chain):
        errors.append("regtest interval=150 marker missing; update out-of-scope note")
    if "FrozenKernelParams does not apply to regtest" not in COQ.read_text(encoding="utf-8"):
        errors.append("Coq must declare regtest interval out of FrozenKernelParams scope")

    if errors:
        print("FAIL: FrozenKernelParams drifted from C++ constants")
        for e in errors:
            print(f"  - {e}")
        return 1

    print("FROZEN_KERNEL_PARAMS=PASS")
    print(f"base={base} interval={interval} max_shifts={shifts} genesis={coq_genesis}")
    print("regtest interval=150 is outside FrozenKernelParams")
    return 0


if __name__ == "__main__":
    sys.exit(main())

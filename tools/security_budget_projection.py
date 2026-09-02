#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""P0-076 protocol-native security budget projection.

Simulates miner ORU issuance over 0-10 years and 10-50 years.
Does not predict price, fees, hashrate, or adoption.
Does not reimplement GetBlockSubsidy / GetResMintReward / RigorSupplyDelta.
"""

from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from economic_model_simulator import (  # noqa: E402
    BLOCKS_PER_YEAR,
    TARGET_BLOCK_TIME_S,
    issued_oru,
    load_frozen_schedule,
    oru_reward_at,
    snapshot_at,
)

HORIZON_A_YEARS = 10
HORIZON_B_YEARS = 50


def _year_height(year: int) -> int:
    return year * BLOCKS_PER_YEAR


def _annual_issuance(year: int, sched) -> int:
    """ORU issued during calendar year `year` (1-indexed year of chain life).

    Year 1 covers heights 1 .. BLOCKS_PER_YEAR.
    """
    if year <= 0:
        return 0
    hi = _year_height(year)
    lo = _year_height(year - 1)
    return issued_oru(hi, sched) - issued_oru(lo, sched)


def _inflation_rate(year: int, sched) -> float | None:
    """Annual issuance / circulating supply at the start of the year. None at genesis."""
    start_h = _year_height(year - 1)
    start = snapshot_at(start_h, sched)
    annual = _annual_issuance(year, sched)
    if start.circulating_supply <= 0:
        return None
    return annual / start.circulating_supply


def _print_year_row(year: int, sched) -> None:
    h = _year_height(year)
    annual = _annual_issuance(year, sched)
    tip = snapshot_at(h, sched)
    per_block = oru_reward_at(h, sched)
    miner_reward = per_block  # protocol-native: subsidy is the security expenditure per block
    inf = _inflation_rate(year, sched)
    inf_s = "n/a" if inf is None else f"{inf * 100:.6f}%"
    print(
        f"{year:6d} {h:12d} {annual:22d} {miner_reward:16d} {inf_s:>14} "
        f"{tip.circulating_supply:22d} {tip.locked_supply:22d}"
    )


def main() -> int:
    sched = load_frozen_schedule()
    print("SECURITY_BUDGET_MODEL=protocol-native ORU subsidy")
    print("PRICE_PREDICTION=FALSE")
    print("TARGET_BLOCK_TIME_S", TARGET_BLOCK_TIME_S)
    print("BLOCKS_PER_YEAR", BLOCKS_PER_YEAR)
    print("HALVING_INTERVAL", sched.interval)
    print()

    header = (
        f"{'year':>6} {'height':>12} {'annual_issuance':>22} {'miner_reward/blk':>16} "
        f"{'inflation':>14} {'circulating':>22} {'locked':>22}"
    )
    print("=== years 0-10 ===")
    print(header)
    for year in range(0, HORIZON_A_YEARS + 1):
        _print_year_row(year, sched)

    print()
    print("=== years 10-50 (decadal) ===")
    print(header)
    for year in range(HORIZON_A_YEARS, HORIZON_B_YEARS + 1, 10):
        _print_year_row(year, sched)

    # Structural checks: subsidy never reaches zero; tail year still pays the last pinned rate.
    y50 = _year_height(HORIZON_B_YEARS)
    tail = oru_reward_at(y50, sched)
    last_pin = sched.oru_by_epoch[max(sched.oru_by_epoch)]
    errors: list[str] = []
    if tail <= 0:
        errors.append("tail miner reward is zero (protocol tail must remain positive)")
    if tail != last_pin:
        errors.append(f"year-{HORIZON_B_YEARS} reward {tail} != last frozen ORU pin {last_pin}")
    a10 = _annual_issuance(HORIZON_A_YEARS, sched)
    a50 = _annual_issuance(HORIZON_B_YEARS, sched)
    if a10 <= 0 or a50 <= 0:
        errors.append("annual issuance vanished on a simulated horizon")

    if errors:
        print("FAIL: security budget projection")
        for e in errors:
            print(f"  - {e}")
        return 1

    print()
    print("ANNUAL_ISSUANCE=COMPUTED")
    print("MINER_REWARD=COMPUTED")
    print("INFLATION_RATE=COMPUTED")
    print("SECURITY_BUDGET=COMPUTED")
    print("PRICE_MODEL=NONE")
    print("SECURITY_BUDGET_PROJECTION=PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())

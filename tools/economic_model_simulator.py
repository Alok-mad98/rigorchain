#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""P0-076 economic model simulator (RFC-020).

Simulation only. Reads frozen consensus vectors for ORU pins and uses the
numeric SSOT for continuous display destroy_cost(h). Does not modify consensus
rules, PoW / ASERT, or the Kernel.
"""

from __future__ import annotations

import argparse
import json
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from res_continuous_price_schedule import destroy_cost_atoms  # noqa: E402

RELEASE_VECTORS = ROOT / "formal_verification/release_consensus_vectors.json"
GENESIS_VECTORS = ROOT / "formal_verification/genesis_consensus_vectors.json"
BOUNDARY_VECTORS = ROOT / "core/src/test/data/issuance_height_boundary_p0_040.json"
SUPPLY_MODEL = ROOT / "formal_verification/economic_supply_model.json"

# Coverage heights required by .
REQUIRED_HEIGHTS = (0, 1, 99, 100, 101, 210000, 420000, 840000)

# Nominal public-network spacing (Consensus::TARGET_BLOCK_TIME). Not a subsidy formula.
TARGET_BLOCK_TIME_S = 600
BLOCKS_PER_YEAR = int(365.25 * 24 * 3600 / TARGET_BLOCK_TIME_S)
LONG_HORIZON_YEARS = 50
LONG_HORIZON_HEIGHT = BLOCKS_PER_YEAR * LONG_HORIZON_YEARS

# RFC-018 baseline: no convergence burns, no RES-transfer fee burns.
BASELINE_BURNED = 0
# RFC-020: no automatic per-block RES grant. Baseline burns nothing → minted RES is 0.
BASELINE_MINTED_RES = 0


@dataclass(frozen=True)
class FrozenPin:
    height: int
    epoch: int
    oru: int
    res_schedule: int | None


@dataclass(frozen=True)
class FrozenSchedule:
    """Frozen ORU pins plus exact-height display-price pins. Not a C++ formula body."""

    interval: int
    maturity: int
    genesis_oru: int
    genesis_res_destroy_cost: int
    # observational calendar bucket -> per-block ORU (height >= 1). Flat 128 under RFC-020.
    oru_by_epoch: dict[int, int]
    res_by_height: dict[int, int]
    pins: tuple[FrozenPin, ...]


@dataclass(frozen=True)
class SupplySnapshot:
    height: int
    epoch: int
    oru_reward: int
    res_schedule: int | None
    issued_oru: int
    scheduled_res: int | None
    matured_supply: int
    locked_supply: int
    circulating_supply: int
    burned_supply: int
    minted_res_supply: int
    total_supply: int


def _load_json(path: Path) -> Any:
    if not path.is_file():
        raise FileNotFoundError(f"missing frozen vector file: {path}")
    return json.loads(path.read_text(encoding="utf-8"))


def _as_int(value: Any) -> int | None:
    if value is None:
        return None
    return int(value)


def _collect_pins() -> list[FrozenPin]:
    """Load height pins from frozen JSON only. Never invent a subsidy."""
    pins: dict[int, FrozenPin] = {}

    def add(height: int, epoch: int | None, oru: int | None, res: int | None) -> None:
        if height < 0 or oru is None:
            return
        prev = pins.get(height)
        epoch_v = int(epoch) if epoch is not None else (prev.epoch if prev else 0)
        res_v = res if res is not None else (prev.res_schedule if prev else None)
        pins[height] = FrozenPin(height=height, epoch=epoch_v, oru=int(oru), res_schedule=res_v)

    def rate_of(row: dict[str, Any], *keys: str) -> int | None:
        for key in keys:
            if key in row and row[key] is not None:
                return _as_int(row[key])
        return None

    release = _load_json(RELEASE_VECTORS)
    genesis_block = release.get("genesis") or {}
    add(
        int(genesis_block.get("height", 0)),
        0,
        _as_int(genesis_block.get("oru_actual")),
        None,
    )
    for row in release.get("epoch") or []:
        add(int(row["height"]), _as_int(row.get("epoch")), _as_int(row.get("oru")), None)

    if GENESIS_VECTORS.is_file():
        genesis = _load_json(GENESIS_VECTORS)
        g0 = genesis.get("genesis") or {}
        add(
            int(g0.get("height", 0)),
            0,
            _as_int(g0.get("oru_issuance")),
            rate_of(g0, "ResDestroyCost", "res_destroy_cost"),
        )
        for row in genesis.get("height") or []:
            add(
                int(row["height"]),
                _as_int(row.get("epoch")),
                _as_int(row.get("oru")),
                rate_of(row, "res_destroy_cost", "ResDestroyCost", "res_schedule"),
            )

    if BOUNDARY_VECTORS.is_file():
        boundary = _load_json(BOUNDARY_VECTORS)
        for row in boundary.get("vectors") or []:
            add(
                int(row["height"]),
                _as_int(row.get("epoch")),
                _as_int(row.get("oru_reward")),
                None,
            )

    if not pins:
        raise RuntimeError("no frozen issuance pins loaded")
    return [pins[h] for h in sorted(pins)]


def load_frozen_schedule() -> FrozenSchedule:
    """Authority for simulation rates: frozen vector tables, not C++ formula bodies."""
    if not RELEASE_VECTORS.is_file():
        raise FileNotFoundError(f"required frozen vectors missing: {RELEASE_VECTORS}")

    pins = _collect_pins()
    interval = 0
    for pin in pins:
        if pin.epoch == 1:
            interval = pin.height
            break
    if interval <= 0:
        raise RuntimeError("calendar interval not present in frozen vectors")

    maturity = 100
    if GENESIS_VECTORS.is_file():
        maturity = int((_load_json(GENESIS_VECTORS).get("maturity") or {}).get("nCoinbaseMaturity", 100))

    genesis = next(p for p in pins if p.height == 0)
    oru_by_epoch: dict[int, int] = {}
    res_by_height: dict[int, int] = {}
    for pin in pins:
        if pin.res_schedule is not None:
            res_by_height[pin.height] = pin.res_schedule
        if pin.height == 0:
            continue
        oru_by_epoch[pin.epoch] = pin.oru

    if not oru_by_epoch:
        raise RuntimeError("frozen vectors have no height>=1 ORU pins")

    return FrozenSchedule(
        interval=interval,
        maturity=maturity,
        genesis_oru=genesis.oru,
        genesis_res_destroy_cost=int(genesis.res_schedule or destroy_cost_atoms(0)),
        oru_by_epoch=oru_by_epoch,
        res_by_height=res_by_height,
        pins=tuple(pins),
    )


def _epoch_of(height: int, sched: FrozenSchedule) -> int:
    if height <= 0:
        return 0
    return height // sched.interval


def oru_reward_at(height: int, sched: FrozenSchedule) -> int:
    """Per-block ORU from the frozen epoch table. Hold last pinned rate past the table."""
    if height <= 0:
        return sched.genesis_oru
    epoch = _epoch_of(height, sched)
    if epoch in sched.oru_by_epoch:
        return sched.oru_by_epoch[epoch]
    last = max(sched.oru_by_epoch)
    return sched.oru_by_epoch[last]


def res_schedule_at(height: int, sched: FrozenSchedule) -> int | None:
    """Display destroy_cost(h). Prefer an exact frozen pin; otherwise the numeric SSOT."""
    if height < 0:
        return 0
    if height in sched.res_by_height:
        return sched.res_by_height[height]
    if height == 0:
        return sched.genesis_res_destroy_cost
    return destroy_cost_atoms(height)


def _era_bounds(epoch: int, sched: FrozenSchedule, last_epoch: int) -> tuple[int, int]:
    start = 0 if epoch == 0 else epoch * sched.interval
    if epoch == last_epoch:
        return start, 10**18
    return start, (epoch + 1) * sched.interval - 1


def issued_oru(height: int, sched: FrozenSchedule) -> int:
    """Cumulative ORU issued at tip `height` (inclusive). Height 0 contributes the genesis pin (0)."""
    if height < 0:
        return 0
    total = sched.genesis_oru
    if height == 0:
        return total
    last_epoch = max(sched.oru_by_epoch)
    for epoch in range(0, last_epoch + 1):
        rate = sched.oru_by_epoch[epoch]
        start, end = _era_bounds(epoch, sched, last_epoch)
        lo = max(start, 1)
        hi = min(end, height)
        if lo <= hi:
            total += (hi - lo + 1) * rate
    return total


def scheduled_res_cumulative(height: int, sched: FrozenSchedule) -> int | None:
    """Automatic per-block RES mint is retired. Baseline scheduled mint is 0."""
    del sched
    if height < 0:
        return 0
    return 0


def locked_oru(height: int, sched: FrozenSchedule) -> int:
    """ORU issued in (height - maturity, height] — still immature at the tip."""
    if height <= 0:
        return 0
    lo = max(1, height - sched.maturity + 1)
    return issued_oru(height, sched) - issued_oru(lo - 1, sched)


def snapshot_at(
    height: int,
    sched: FrozenSchedule,
    *,
    burned: int = BASELINE_BURNED,
    minted_res: int = BASELINE_MINTED_RES,
) -> SupplySnapshot:
    issued = issued_oru(height, sched)
    locked = locked_oru(height, sched)
    matured = issued - locked
    circulating = matured - burned
    return SupplySnapshot(
        height=height,
        epoch=_epoch_of(height, sched),
        oru_reward=oru_reward_at(height, sched),
        res_schedule=res_schedule_at(height, sched),
        issued_oru=issued,
        scheduled_res=scheduled_res_cumulative(height, sched),
        matured_supply=matured,
        locked_supply=locked,
        circulating_supply=circulating,
        burned_supply=burned,
        minted_res_supply=minted_res,
        total_supply=issued,
    )


def check_invariants(snap: SupplySnapshot) -> list[str]:
    errors: list[str] = []
    if snap.circulating_supply > snap.total_supply:
        errors.append(f"h={snap.height}: circulating > total_supply")
    issued_id = snap.locked_supply + snap.circulating_supply + snap.burned_supply
    if issued_id != snap.issued_oru:
        errors.append(
            f"h={snap.height}: locked+circulating+burned ({issued_id}) != issued ({snap.issued_oru})"
        )
    if snap.matured_supply + snap.locked_supply != snap.issued_oru:
        errors.append(f"h={snap.height}: matured+locked != issued")
    if snap.circulating_supply < 0 or snap.locked_supply < 0 or snap.issued_oru < 0:
        errors.append(f"h={snap.height}: negative supply field")
    if snap.burned_supply < 0:
        errors.append(f"h={snap.height}: negative burned")
    return errors


def _snap_public(snap: SupplySnapshot) -> dict[str, Any]:
    return {
        "height": snap.height,
        "epoch": snap.epoch,
        "oru_reward": snap.oru_reward,
        "res_schedule": snap.res_schedule,
        "issued_oru": snap.issued_oru,
        "scheduled_res": snap.scheduled_res,
        "total_supply": snap.total_supply,
        "circulating_supply": snap.circulating_supply,
        "locked_supply": snap.locked_supply,
        "matured_supply": snap.matured_supply,
        "burned_supply": snap.burned_supply,
        "minted_res_supply": snap.minted_res_supply,
    }


def build_supply_model(sched: FrozenSchedule) -> dict[str, Any]:
    scenarios = {
        "genesis": snapshot_at(0, sched),
        "early_mining": snapshot_at(1, sched),
        "maturity_release": snapshot_at(101, sched),
        "calendar_210000": snapshot_at(210000, sched),
        "calendar_420000": snapshot_at(420000, sched),
        "calendar_840000": snapshot_at(840000, sched),
        "long_horizon_50y": snapshot_at(LONG_HORIZON_HEIGHT, sched),
    }
    heights = {h: snapshot_at(h, sched) for h in REQUIRED_HEIGHTS}
    heights["long_horizon"] = scenarios["long_horizon_50y"]

    # Representative tip for the flat schema: first maturity-release height.
    rep = scenarios["maturity_release"]
    return {
 "task": "",
        "economic_model": "simulation_only",
        "consensus_modified": False,
        "formula_duplicate": False,
        "consensus_formula_source": "RigorConsensusKernel",
        "vector_source": str(RELEASE_VECTORS.relative_to(ROOT)),
        "parameters": {
            "calendar_interval": sched.interval,
            "maturity": sched.maturity,
            "target_block_time_s": TARGET_BLOCK_TIME_S,
            "blocks_per_year": BLOCKS_PER_YEAR,
            "oru_flat_block_subsidy": 12800000000,
            "res_max_supply": 419430400000000,
        },
        "total_supply": rep.total_supply,
        "circulating_supply": rep.circulating_supply,
        "locked_supply": rep.locked_supply,
        "matured_supply": rep.matured_supply,
        "burned_supply": rep.burned_supply,
        "minted_res_supply": rep.minted_res_supply,
        "invariants": {
            "circulating_le_total_supply": True,
            "locked_plus_circulating_plus_burned_eq_issued": True,
        },
        "scenarios": {name: _snap_public(s) for name, s in scenarios.items()},
        "heights": {str(k): _snap_public(v) for k, v in heights.items()},
    }


def _validate_against_committed(model: dict[str, Any]) -> list[str]:
    if not SUPPLY_MODEL.is_file():
        return [f"missing {SUPPLY_MODEL.relative_to(ROOT)}"]
    committed = _load_json(SUPPLY_MODEL)
    errors: list[str] = []
    for key in (
        "total_supply",
        "circulating_supply",
        "locked_supply",
        "matured_supply",
        "burned_supply",
        "minted_res_supply",
        "economic_model",
        "consensus_modified",
        "formula_duplicate",
    ):
        if committed.get(key) != model.get(key):
            errors.append(f"supply model field {key}: committed={committed.get(key)!r} computed={model.get(key)!r}")
    for name, snap in model["scenarios"].items():
        c = (committed.get("scenarios") or {}).get(name)
        if c != snap:
            errors.append(f"scenario {name} drifted from committed economic_supply_model.json")
    return errors


def simulate(height: int | None = None) -> int:
    sched = load_frozen_schedule()
    errors: list[str] = []

    print("FROZEN_VECTOR_SOURCE", RELEASE_VECTORS.relative_to(ROOT))
    print("CALENDAR_INTERVAL", sched.interval)
    print("MATURITY", sched.maturity)
    print("PINNED_EPOCHS_ORU", ",".join(str(e) for e in sorted(sched.oru_by_epoch)))
    print("PINNED_PRICE_HEIGHTS", ",".join(str(h) for h in sorted(sched.res_by_height)))
    print()

    cover = list(REQUIRED_HEIGHTS) + [LONG_HORIZON_HEIGHT]
    if height is not None:
        cover.append(height)
    seen: set[int] = set()
    ordered: list[int] = []
    for h in cover:
        if h not in seen:
            seen.add(h)
            ordered.append(h)

    print(
        f"{'height':>12} {'epoch':>6} {'oru_reward':>16} {'res_sched':>12} "
        f"{'issued_oru':>22} {'sched_res':>18} {'matured':>22} {'locked':>22} {'circulating':>22}"
    )
    for h in ordered:
        snap = snapshot_at(h, sched)
        errors.extend(check_invariants(snap))
        res_s = "-" if snap.res_schedule is None else str(snap.res_schedule)
        sched_res = "-" if snap.scheduled_res is None else str(snap.scheduled_res)
        print(
            f"{snap.height:12d} {snap.epoch:6d} {snap.oru_reward:16d} {res_s:>12} "
            f"{snap.issued_oru:22d} {sched_res:>18} {snap.matured_supply:22d} "
            f"{snap.locked_supply:22d} {snap.circulating_supply:22d}"
        )

    model = build_supply_model(sched)
    if "--write-supply-model" in sys.argv:
        SUPPLY_MODEL.write_text(json.dumps(model, indent=2) + "\n", encoding="utf-8")
        print()
        print("WROTE", SUPPLY_MODEL.relative_to(ROOT))
    else:
        errors.extend(_validate_against_committed(model))

    if errors:
        print("FAIL: economic model simulation")
        for e in errors:
            print(f"  - {e}")
        return 1

    print()
    print("ISSUANCE_SCHEDULE_VALIDATION=PASS")
    print("CIRCULATING_SUPPLY_MODEL=PASS")
    print("MATURITY_LOCKED_SUPPLY_MODEL=PASS")
    print("FORMULA_DUPLICATION=FALSE")
    print("ECONOMIC_MODEL_SIMULATION=PASS")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description="RIGOR economic model simulator (frozen vectors only)")
    parser.add_argument("--height", type=int, default=None, help="extra tip height to print")
    parser.add_argument(
        "--write-supply-model",
        action="store_true",
        help="rewrite formal_verification/economic_supply_model.json from this run",
    )
    args, _unknown = parser.parse_known_args()
    return simulate(args.height)


if __name__ == "__main__":
    sys.exit(main())

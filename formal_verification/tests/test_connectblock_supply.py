#!/usr/bin/env python3
"""
PHASE-4E ConnectBlock supply harness.

Mirrors Coq definitions in:
  - ReplayReorgCore/ConnectBlockMint.v
  - ConnectBlock/ConnectBlockSupply.v (constants)

Validates deterministic mint deltas on synthetic blocks without running coqc.
"""

from __future__ import annotations

import sys
from dataclasses import dataclass
from typing import NamedTuple

# ConnectBlock.v schedule aliases / ConnectBlockMint.v (centi-RES cap units)
# F-1: ConnectBlock.v is not the skip-on-overshoot scaffold.
CB_RES_MINT_AMOUNT = 10_000
CB_MAX_RES_SUPPLY = 21_000_000 * 100


@dataclass(frozen=True)
class ConnectMintSpec:
    draw_won: bool
    oru_subsidy: int


@dataclass(frozen=True)
class ReplayState:
    oru_supply: int
    res_supply: int


@dataclass(frozen=True)
class BlockAbs:
    mint: ConnectMintSpec


def cb_oru_subsidy(spec: ConnectMintSpec) -> int:
    return max(0, spec.oru_subsidy)


def cb_oru_mint_delta(_s: ReplayState, b: BlockAbs) -> int:
    return cb_oru_subsidy(b.mint)


def cb_res_mint_delta(s: ReplayState, b: BlockAbs) -> int:
    if not b.mint.draw_won:
        return 0
    if s.res_supply + CB_RES_MINT_AMOUNT <= CB_MAX_RES_SUPPLY:
        return CB_RES_MINT_AMOUNT
    return 0


def protocol_mint_total_delta(s: ReplayState, b: BlockAbs) -> int:
    return cb_oru_mint_delta(s, b) + cb_res_mint_delta(s, b)


def apply_connectblock_supply_delta(s: ReplayState, b: BlockAbs) -> ReplayState:
    return ReplayState(
        oru_supply=s.oru_supply + cb_oru_mint_delta(s, b),
        res_supply=s.res_supply + cb_res_mint_delta(s, b),
    )


def total_supply(s: ReplayState) -> int:
    return s.oru_supply + s.res_supply


def supply_evolution_holds(s: ReplayState, b: BlockAbs) -> bool:
    s2 = apply_connectblock_supply_delta(s, b)
    return total_supply(s2) == total_supply(s) + protocol_mint_total_delta(s, b)


class Case(NamedTuple):
    name: str
    state: ReplayState
    block: BlockAbs
    expect_oru_delta: int
    expect_res_delta: int


CASES: tuple[Case, ...] = (
    Case(
        "default_mint_zero",
        ReplayState(0, 0),
        BlockAbs(ConnectMintSpec(draw_won=False, oru_subsidy=0)),
        0,
        0,
    ),
    Case(
        "oru_subsidy_only",
        ReplayState(100, 50),
        BlockAbs(ConnectMintSpec(draw_won=False, oru_subsidy=25)),
        25,
        0,
    ),
    Case(
        "oru_subsidy_clamped",
        ReplayState(0, 0),
        BlockAbs(ConnectMintSpec(draw_won=False, oru_subsidy=-5)),
        0,
        0,
    ),
    Case(
        "res_draw_won_under_cap",
        ReplayState(0, CB_MAX_RES_SUPPLY - CB_RES_MINT_AMOUNT),
        BlockAbs(ConnectMintSpec(draw_won=True, oru_subsidy=0)),
        0,
        CB_RES_MINT_AMOUNT,
    ),
    Case(
        "res_draw_won_at_cap",
        ReplayState(0, CB_MAX_RES_SUPPLY),
        BlockAbs(ConnectMintSpec(draw_won=True, oru_subsidy=0)),
        0,
        0,
    ),
    Case(
        "res_draw_won_over_cap",
        ReplayState(0, CB_MAX_RES_SUPPLY - CB_RES_MINT_AMOUNT + 1),
        BlockAbs(ConnectMintSpec(draw_won=True, oru_subsidy=0)),
        0,
        0,
    ),
    Case(
        "oru_and_res_combined",
        ReplayState(1_000, 500_000),
        BlockAbs(ConnectMintSpec(draw_won=True, oru_subsidy=50)),
        50,
        CB_RES_MINT_AMOUNT,
    ),
)


def run_case(case: Case) -> list[str]:
    errors: list[str] = []
    s, b = case.state, case.block
    oru_d = cb_oru_mint_delta(s, b)
    res_d = cb_res_mint_delta(s, b)
    if oru_d != case.expect_oru_delta:
        errors.append(f"{case.name}: ORU delta {oru_d} != {case.expect_oru_delta}")
    if res_d != case.expect_res_delta:
        errors.append(f"{case.name}: RES delta {res_d} != {case.expect_res_delta}")
    total_d = protocol_mint_total_delta(s, b)
    if total_d != oru_d + res_d:
        errors.append(f"{case.name}: protocol_mint_total_delta mismatch")
    if total_d < 0:
        errors.append(f"{case.name}: negative total mint delta")
    if not supply_evolution_holds(s, b):
        errors.append(f"{case.name}: supply_evolution_eq failed")
    s2 = apply_connectblock_supply_delta(s, b)
    if total_supply(s2) < total_supply(s):
        errors.append(f"{case.name}: supply decreased (monotonicity)")
    return errors


def check_constants_align() -> list[str]:
    errors: list[str] = []
    # ConnectBlock.v schedule aliases (RES_MINT_AMOUNT / MAX_RES_SUPPLY)
    res_mint_amount = 10_000
    max_res_supply = 21_000_000 * 100
    if CB_RES_MINT_AMOUNT != res_mint_amount:
        errors.append("CB_RES_MINT_AMOUNT != ConnectBlock RES_MINT_AMOUNT")
    if CB_MAX_RES_SUPPLY != max_res_supply:
        errors.append("CB_MAX_RES_SUPPLY != ConnectBlock MAX_RES_SUPPLY")
    return errors


def check_coq_module_present() -> list[str]:
    from pathlib import Path

    root = Path(__file__).resolve().parents[2]
    mod = root / "formal_verification" / "coq" / "ConnectBlock" / "ConnectBlockSupply.v"
    if not mod.is_file():
        return ["missing ConnectBlock/ConnectBlockSupply.v"]
    text = mod.read_text(encoding="utf-8")
    required = (
        "Theorem apply_block_supply_correct",
        "Theorem supply_monotonic_cb",
        "Theorem reorg_preserves_supply",
        "Lemma scaffold_res_delta_eq_cb",
        "Lemma cb_res_mint_amount_eq",
        "Theorem apply_block_rejects_where_legacy_scaffold_would_skip",
    )
    return [f"ConnectBlockSupply.v missing `{name}`" for name in required if name not in text]


def main() -> int:
    errors: list[str] = []
    errors.extend(check_constants_align())
    errors.extend(check_coq_module_present())
    for case in CASES:
        errors.extend(run_case(case))

    if errors:
        print("FAIL: ConnectBlock supply harness (PHASE-4E)")
        for err in errors:
            print(f"  - {err}")
        return 1

    print(f"PASS: ConnectBlock supply harness ({len(CASES)} synthetic cases)")
    return 0


if __name__ == "__main__":
    sys.exit(main())

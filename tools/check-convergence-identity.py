#!/usr/bin/env python3
# Copyright (c) 2026 The RigorChain developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Gate 1 — Mathematical identity vector test.

Conceptual cost and the integer predicate must accept or reject together.

    destroy_cost(h) = 1024 · (2^25 + h)^2 / 2^50
            ↓ exact integer-preserving equivalence
    R · (2^25 + h)^2 ≤ B << 40

Integers only. No float, no display floor, no epoch table, no price oracle.

Reproduce:
  python3 tools/check-convergence-identity.py
"""
from __future__ import annotations

import sys

POLE = 1 << 25
SHIFT = 40
P0 = 1024
COIN = 100_000_000
RES_COIN = COIN
INT32_MAX = 2_147_483_647
MAX_MONEY = 10_000_000_000 * COIN


def _int(x: int, name: str) -> int:
    if type(x) is not int:
        raise SystemExit(f"FAIL: {name} is not an int")
    return x


def conceptual_admits(res_atoms: int, burn_atoms: int, height: int) -> bool:
    """Exact mint condition with the display quotient uncleared: no division."""
    R = _int(res_atoms, "R")
    B = _int(burn_atoms, "B")
    h = _int(height, "h")
    if R < 0 or B < 0 or h < 0:
        return False
    if R == 0:
        return True
    pole_h = POLE + h
    # R · 1024 · (2^25+h)^2  ≤  B · 2^50
    return R * P0 * pole_h * pole_h <= B * (1 << 50)


def predicate_admits(res_atoms: int, burn_atoms: int, height: int) -> bool:
    """Consensus inequality. Multiply, square, shift, compare."""
    R = _int(res_atoms, "R")
    B = _int(burn_atoms, "B")
    h = _int(height, "h")
    if R < 0 or B < 0 or h < 0:
        return False
    if R == 0:
        return True
    pole_h = POLE + h
    return R * pole_h * pole_h <= (B << SHIFT)


def agree(res_atoms: int, burn_atoms: int, height: int) -> bool:
    return conceptual_admits(res_atoms, burn_atoms, height) is predicate_admits(
        res_atoms, burn_atoms, height
    )


def main() -> int:
    errors: list[str] = []

    def expect(h: int, R: int, B: int, want: bool, label: str) -> None:
        c = conceptual_admits(R, B, h)
        p = predicate_admits(R, B, h)
        if c is not p:
            errors.append(f"{label}: conceptual={c} predicate={p} (must match)")
        if p is not want:
            errors.append(f"{label}: predicate={p}, expected {want}")

    # Genesis whole-RES boundary. 1024 ORU buys 1 RES; one atom short fails.
    expect(0, RES_COIN, 1024 * COIN, True, "h=0 R=1RES B=1024ORU must_pass")
    expect(0, RES_COIN, 1024 * COIN - 1, False, "h=0 R=1RES B=required-1 must_fail")

    # One RES atom at genesis: 1024 ORU atoms (not 1024 whole ORU).
    expect(0, 1, 1024, True, "h=0 R=1atom B=1024atoms must_pass")
    expect(0, 1, 1023, False, "h=0 R=1atom B=1023 must_fail")

    # Plain destroy: R = 0 is unconstrained.
    expect(0, 0, 0, True, "h=0 R=0 B=0 must_pass")
    expect(INT32_MAX, 0, 0, True, "h=INT32_MAX R=0 must_pass")

    # Zero burn never mints a positive R.
    expect(0, 1, 0, False, "h=0 R=1 B=0 must_fail")
    expect(INT32_MAX, 1, 0, False, "h=INT32_MAX R=1 B=0 must_fail")

    heights = (0, 1, 210000, 420000, POLE, INT32_MAX)
    amounts_r = (0, 1, RES_COIN)
    amounts_b = (
        0,
        1,
        1023,
        1024,
        1024 * COIN - 1,
        1024 * COIN,
        2048 * COIN,
        MAX_MONEY,
    )
    for h in heights:
        for R in amounts_r:
            for B in amounts_b:
                if not agree(R, B, h):
                    errors.append(
                        f"identity drift at h={h} R={R} B={B}: "
                        f"conceptual={conceptual_admits(R, B, h)} "
                        f"predicate={predicate_admits(R, B, h)}"
                    )

    # At later heights the genesis burn is no longer enough for 1 RES.
    if predicate_admits(RES_COIN, 1024 * COIN, 210000):
        errors.append("h=210000: genesis 1024 ORU must not buy 1 RES")
    if predicate_admits(RES_COIN, 1024 * COIN, INT32_MAX):
        errors.append("h=INT32_MAX: genesis 1024 ORU must not buy 1 RES")
    if not predicate_admits(RES_COIN, MAX_MONEY, INT32_MAX):
        errors.append("h=INT32_MAX: MAX_MONEY must still buy 1 RES (no free-mint horizon)")

    if errors:
        print("CONVERGENCE_IDENTITY_FAILED")
        for e in errors[:40]:
            print(f"  - {e}")
        if len(errors) > 40:
            print(f"  … {len(errors) - 40} more")
        return 1

    print("CONVERGENCE_IDENTITY_TEST=PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())

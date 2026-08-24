#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor developers
# SPDX-License-Identifier: MIT
"""
Simulate genesis-anchored ASERT (aserti3-2d integer path) for visualization.

Polynomial coefficients and radix match Bitcoin Cash / Bitcoin ABC
(aserti32d.cpp, Nov 2020): P1=195766423245049, P2=971821376, P3=5127, bias=2^47.

This mirrors consensus/pow.cpp logic in Python (uint256 as int); use only for
plots / logs — consensus remains the C++ implementation.

The production rule uses the **parent** timestamp and the height-1 anchor
(TASK-012). This helper still takes an explicit (eval_height, eval_time) pair so
callers can plot either convention.

Example:
  python3 contrib/devtools/asert_retarget_curve.py --csv /tmp/asert_curve.csv
  python3 contrib/devtools/asert_retarget_curve.py --csv /tmp/asert.csv --plot /tmp/asert.png
"""

from __future__ import annotations

import argparse
import csv
import math
import sys
from typing import List, Tuple

ASERT_RADIX = 65536
ASERT_POLY_P1 = 195766423245049
ASERT_POLY_P2 = 971821376
ASERT_POLY_P3 = 5127
ASERT_POLY_BIAS = 1 << 47
ASERT_EXP_CLAMP = ASERT_RADIX * 64
# Rigor mainnet-style powLimit (uint256); matches consensus.powLimit in chainparams.
POW_LIMIT_ARDA_DEFAULT = int("0x7" + "f" * 63, 16)


def target_from_compact(bits: int) -> int:
    """Decode nBits to 256-bit target (same semantics as Bitcoin compact)."""
    exponent = bits >> 24
    mantissa = bits & 0xFFFFFF
    if exponent <= 3:
        mantissa >>= 8 * (3 - exponent)
        return mantissa
    return mantissa << (8 * (exponent - 3))


def asert_next_nbits(
    anchor_time: int,
    anchor_bits: int,
    eval_height: int,
    eval_time: int,
    spacing: int,
    half_life: int,
    pow_limit: int,
) -> Tuple[int, int]:
    """
    Return (nbits, target) for the next block; anchor target from genesis bits.
    """
    time_error = int(eval_time) - int(anchor_time) - int(eval_height) * int(spacing)
    numer = time_error * ASERT_RADIX
    exp_q = numer // half_life
    if exp_q > ASERT_EXP_CLAMP:
        exp_q = ASERT_EXP_CLAMP
    elif exp_q < -ASERT_EXP_CLAMP:
        exp_q = -ASERT_EXP_CLAMP

    num_shifts = exp_q >> 16
    exp_rem = exp_q - (num_shifts << 16)

    e = exp_rem
    poly = ASERT_POLY_P1 * e + ASERT_POLY_P2 * e * e + ASERT_POLY_P3 * e * e * e + ASERT_POLY_BIAS
    factor = (poly >> 48) + ASERT_RADIX
    if factor < 0 or factor > (1 << 64) - 1:
        factor = (1 << 64) - 1

    target_ref = target_from_compact(anchor_bits)
    v = target_ref * int(factor)
    if num_shifts < 0:
        shr = min(64, -num_shifts)
        v >>= shr
    elif num_shifts > 0:
        shl = min(64, int(num_shifts))
        v <<= shl
    v >>= 16
    if v > pow_limit:
        v = pow_limit
    next_target = v
    if next_target == 0:
        next_target = 1
    if next_target > pow_limit:
        next_target = pow_limit

    # Compact encode (minimal form for logging; matches GetCompact trend)
    nbits = target_to_compact_lossy(next_target)
    return nbits, next_target


def target_to_compact_lossy(target: int) -> int:
    """Encode target to nBits (sufficient for monotonic curve CSV)."""
    if target <= 0:
        return 0x02000000
    t = target
    exponent = (t.bit_length() + 7) // 8
    if exponent <= 3:
        mantissa = t << (8 * (3 - exponent))
        return (mantissa & 0xFFFFFF) | (exponent << 24)
    mantissa = t >> (8 * (exponent - 3))
    if mantissa >> 24:
        mantissa >>= 8
        exponent += 1
    return (mantissa & 0xFFFFFF) | (exponent << 24)


def run_scenario(
    anchor_time: int,
    anchor_bits: int,
    spacing: int,
    half_life: int,
    pow_limit: int,
) -> List[dict]:
    anchor_target = target_from_compact(anchor_bits)
    rows: List[dict] = []

    # Wall clock of last block; start on schedule at height 0.
    time_last = anchor_time
    fast_dt = max(1, spacing // 10)  # 10x hashrate
    slow_dt = spacing * 10  # 0.1x hashrate

    phases = [
        ("10x_hash", 200, fast_dt),
        ("0.1x_hash", 200, slow_dt),
        ("normal", 400, spacing),
    ]

    height = 0
    for phase_name, n_blocks, dt in phases:
        for _ in range(n_blocks):
            height += 1
            time_last = min(0xFFFFFFFF, time_last + dt)
            nbits, tgt = asert_next_nbits(
                anchor_time,
                anchor_bits,
                height,
                time_last,
                spacing,
                half_life,
                pow_limit,
            )
            rel = anchor_target / tgt if tgt else float("inf")
            rows.append(
                {
                    "phase": phase_name,
                    "height": height,
                    "block_time": time_last,
                    "inter_arrival_s": dt,
                    "nbits_hex": f"0x{nbits:08x}",
                    "target_hex": f"0x{tgt:x}",
                    "relative_target_vs_genesis": rel,
                    "log2_relative": math.log2(rel) if rel > 0 else float("nan"),
                }
            )
    return rows


def maybe_plot(rows: List[dict], path: str) -> None:
    try:
        import matplotlib.pyplot as plt
    except ImportError:
        print("matplotlib not installed; skip --plot. CSV still written.", file=sys.stderr)
        return
    xs = [r["height"] for r in rows]
    ys = [r["log2_relative"] for r in rows]
    plt.figure(figsize=(10, 4))
    plt.plot(xs, ys, linewidth=1.0)
    plt.axhline(0.0, color="gray", linestyle="--", linewidth=0.8)
    plt.xlabel("block height (simulated)")
    plt.ylabel("log2(target / genesis_target)  (easier ↑)")
    plt.title("ASERT response: 10× hash → 0.1× hash → nominal spacing")
    plt.grid(True, alpha=0.3)
    plt.tight_layout()
    plt.savefig(path, dpi=120)
    print(f"Wrote plot {path}", file=sys.stderr)


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--csv", metavar="PATH", help="write CSV log")
    p.add_argument("--plot", metavar="PATH", help="optional PNG (needs matplotlib)")
    p.add_argument("--spacing", type=int, default=600, help="nominal block spacing (s)")
    p.add_argument("--halflife", type=int, default=43200, help="ASERT half-life (s)")
    p.add_argument("--anchor-time", type=int, default=1_774_915_200, help="genesis anchor time")
    p.add_argument("--anchor-bits", type=lambda x: int(x, 0), default=0x207fffff, help="genesis nBits (compact)")
    p.add_argument(
        "--pow-limit",
        type=lambda x: int(x, 0),
        default=POW_LIMIT_ARDA_DEFAULT,
        help="pow limit as uint256 integer (default: Rigor 0x7fff…ff)",
    )
    args = p.parse_args()

    rows = run_scenario(
        args.anchor_time,
        args.anchor_bits,
        args.spacing,
        args.halflife,
        args.pow_limit,
    )

    if args.csv:
        with open(args.csv, "w", newline="") as f:
            w = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
            w.writeheader()
            w.writerows(rows)
        print(f"Wrote {len(rows)} rows to {args.csv}", file=sys.stderr)

    if args.plot:
        maybe_plot(rows, args.plot)

    # Short human-readable tail on stderr
    sample = rows[:: max(1, len(rows) // 8)]
    print("Sample (height, phase, log2_rel):", file=sys.stderr)
    for r in sample:
        print(f"  {r['height']:4d}  {r['phase']:12s}  {r['log2_relative']:+.4f}", file=sys.stderr)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())

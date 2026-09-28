#!/usr/bin/env python3
"""Build audit/formal_bridge/asert_dataset.csv. Not a Coq proof. Not C++ GetNextWorkASERT.

Rows are labeled:
  cpp_unit_test_pin     — expected_nbits from rigor_asert_vectors.json (C++ unit pin)
  formula_observation   — Python aserti3-2d sketch; not claimed C++ output
  regtest_live          — appended later by collect_task010_readiness.py (no-retarget)
"""
from __future__ import annotations

import csv
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "audit" / "formal_bridge" / "asert_dataset.csv"
PINS = ROOT / "formal_verification" / "vectors" / "rigor_asert_vectors.json"

ASERT_RADIX = 65536
ASERT_POLY_P1 = 195766423245049
ASERT_POLY_P2 = 971821376
ASERT_POLY_P3 = 5127
ASERT_POLY_BIAS = 1 << 47
ASERT_EXP_CLAMP = ASERT_RADIX * 64
MIN_ADJ_PPM = 875_000
MAX_ADJ_PPM = 1_125_000
PPM = 1_000_000
MAINNET_HALF_LIFE = 43200
MAINNET_SPACING = 600
MAINNET_POW_LIMIT = int("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffff", 16)
GENESIS_NBITS = 0x1D008000
GENESIS_TIME = 1792908000


def set_compact(nbits: int) -> int:
    size = nbits >> 24
    word = nbits & 0x007FFFFF
    if size <= 3:
        return word >> (8 * (3 - size))
    return word << (8 * (size - 3))


def get_compact(target: int) -> int:
    if target <= 0:
        return 0
    size = (target.bit_length() + 7) // 8
    if size <= 3:
        compact = target << (8 * (3 - size))
    else:
        compact = target >> (8 * (size - 3))
    if compact & 0x00800000:
        compact >>= 8
        size += 1
    return compact | (size << 24)


def toward_zero_div(numer: int, denom: int) -> int:
    if denom == 0:
        raise ZeroDivisionError
    q = abs(numer) // abs(denom)
    if (numer < 0) != (denom < 0):
        return -q
    return q


def asert_next(
    parent_height: int,
    parent_time: int,
    parent_nbits: int,
    *,
    half_life: int = MAINNET_HALF_LIFE,
    spacing: int = MAINNET_SPACING,
    pow_limit: int = MAINNET_POW_LIMIT,
    anchor_height: int = 1,
    anchor_time: int | None = None,
    anchor_nbits: int | None = None,
) -> tuple[int, int]:
    """Sketch of GetNextWorkASERT. formula_observation only."""
    if parent_height < anchor_height:
        anchor_height = parent_height
        anchor_time = parent_time
        anchor_nbits = parent_nbits
    if anchor_time is None:
        anchor_time = GENESIS_TIME + max(anchor_height, 0) * spacing
    if anchor_nbits is None:
        anchor_nbits = GENESIS_NBITS
    height_diff = parent_height - anchor_height
    time_error = parent_time - anchor_time - height_diff * spacing
    exp_q = toward_zero_div(time_error * ASERT_RADIX, half_life)
    exp_q = max(-ASERT_EXP_CLAMP, min(ASERT_EXP_CLAMP, exp_q))
    num_shifts = exp_q >> 16
    exp_rem = exp_q - (num_shifts << 16)
    e = exp_rem
    poly = ASERT_POLY_P1 * e + ASERT_POLY_P2 * e * e + ASERT_POLY_P3 * e * e * e + ASERT_POLY_BIAS
    factor = (poly >> 48) + ASERT_RADIX
    target_ref = set_compact(anchor_nbits)
    next_target = target_ref * factor
    if num_shifts >= 0:
        next_target <<= min(num_shifts, 64)
    else:
        next_target >>= min(-num_shifts, 64)
    next_target >>= 16
    if next_target <= 0:
        next_target = 1
    if next_target > pow_limit:
        next_target = pow_limit
    prev = set_compact(parent_nbits)
    if prev > 0:
        max_t = min(prev * MAX_ADJ_PPM // PPM, pow_limit)
        min_t = max(prev * MIN_ADJ_PPM // PPM, 1)
        next_target = min(max(next_target, min_t), max_t)
    if next_target > pow_limit:
        next_target = pow_limit
    if next_target <= 0:
        next_target = 1
    return get_compact(next_target), next_target


def target_hex(n: int) -> str:
    return f"{n:064x}"


def rows() -> list[dict]:
    out: list[dict] = []
    if PINS.is_file():
        for pin in json.loads(PINS.read_text()):
            nbits = int(pin["expected_nbits"], 16)
            out.append(
                {
                    "height": int(pin["parent_height"]) + 1,
                    "previous_time": pin["parent_time"],
                    "previous_bits": pin["parent_nbits"],
                    "next_bits": pin["expected_nbits"],
                    "expected_target": target_hex(set_compact(nbits)),
                    "source": "cpp_unit_test_pin",
                    "case": pin["id"],
                }
            )

    cases = [
        ("normal_on_schedule", 2, GENESIS_TIME + 2 * MAINNET_SPACING, GENESIS_NBITS),
        ("normal_height_100", 100, GENESIS_TIME + 100 * MAINNET_SPACING, GENESIS_NBITS),
        ("boundary_height_1", 1, GENESIS_TIME + MAINNET_SPACING, GENESIS_NBITS),
        ("boundary_height_0", 0, GENESIS_TIME, GENESIS_NBITS),
        ("extreme_future_parent", 2, 0xFFFFFFFF, GENESIS_NBITS),
        ("extreme_behind_schedule", 512, GENESIS_TIME + MAINNET_SPACING, GENESIS_NBITS),
        ("min_target_tight_parent", 3, GENESIS_TIME + 3 * MAINNET_SPACING, 0x1C00FFFF),
        ("max_target_powlimit_parent", 3, GENESIS_TIME + 3 * MAINNET_SPACING, get_compact(MAINNET_POW_LIMIT)),
    ]
    for name, height, ptime, pbits in cases:
        nxt, tgt = asert_next(height, ptime, pbits)
        out.append(
            {
                "height": height + 1,
                "previous_time": ptime,
                "previous_bits": f"{pbits:08x}",
                "next_bits": f"{nxt:08x}",
                "expected_target": target_hex(tgt),
                "source": "formula_observation",
                "case": name,
            }
        )
    return out


def write_csv(path: Path, extra: list[dict] | None = None) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    fieldnames = [
        "height",
        "previous_time",
        "previous_bits",
        "next_bits",
        "expected_target",
        "source",
        "case",
    ]
    data = rows()
    if extra:
        data.extend(extra)
    with path.open("w", newline="", encoding="utf-8") as fh:
        w = csv.DictWriter(fh, fieldnames=fieldnames)
        w.writeheader()
        for row in data:
            w.writerow(row)


def main() -> int:
    write_csv(OUT)
    print(f"wrote {OUT} rows={sum(1 for _ in OUT.open()) - 1}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

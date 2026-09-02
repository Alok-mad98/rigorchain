#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Compare a fresh performance_results.json against the pinned baseline.

Reports every latency delta. Fails on fingerprint / scenario / consensus
gate drift (silent semantic regression). Host ns numbers are printed only;
absolute latency is not a cross-machine fail (GitHub runners differ).
Catastrophic 20x slowdown on the same host shape is a fail.
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BASELINE = ROOT / "docs/performance/performance_baseline.json"
RESULTS = ROOT / "docs/performance/performance_results.json"
CATASTROPHIC = 20.0


def key(row: dict) -> tuple:
    return (
        row.get("bench", ""),
        row.get("component", ""),
        row.get("scenario", ""),
        row.get("path", ""),
    )


def load(path: Path) -> dict:
    if not path.is_file():
        raise SystemExit(f"missing {path}")
    return json.loads(path.read_text(encoding="utf-8"))


def main() -> int:
    base = load(BASELINE)
    fresh = load(RESULTS)
    errors: list[str] = []

    print("== PERFORMANCE_DELTA (reported, not silent) ==")
    base_rows = {key(r): r for r in base.get("samples", [])}
    fresh_rows = {key(r): r for r in fresh.get("samples", [])}

    missing = sorted(set(base_rows) - set(fresh_rows))
    extra = sorted(set(fresh_rows) - set(base_rows))
    if missing:
        errors.append(f"missing scenarios vs baseline: {missing[:8]}")
    if extra:
        print(f"note: new sample keys (reported): {extra[:8]}")

    for k in sorted(set(base_rows) & set(fresh_rows)):
        b, n = base_rows[k], fresh_rows[k]
        b_ns = float(b.get("latency_ns") or 0)
        n_ns = float(n.get("latency_ns") or 0)
        if b_ns > 0:
            delta = 100.0 * (n_ns - b_ns) / b_ns
        else:
            delta = 0.0
        label = "/".join(k)
        print(f"  {label}: {b_ns:.3f} -> {n_ns:.3f} ns ({delta:+.1f}%)")
        b_iters = int(b.get("iters") or 0)
        n_iters = int(n.get("iters") or 0)
        if b.get("fingerprint") != n.get("fingerprint"):
            if b_iters and b_iters == n_iters:
                errors.append(
                    f"fingerprint changed for {label}: {b.get('fingerprint')} -> {n.get('fingerprint')}"
                )
            else:
                print(
                    f"  note: fingerprint/iters differ for {label} "
                    f"(iters {b_iters}->{n_iters}); reported, not a pin fail"
                )
        if b_ns > 0 and n_ns > b_ns * CATASTROPHIC:
            errors.append(f"catastrophic latency regression for {label}: {b_ns} -> {n_ns}")

    bg = base.get("gates", {})
    fg = fresh.get("gates", {})
    for flag in ("CONSENSUS_BEHAVIOR_CHANGED", "STATE_TRANSITION_CHANGED", "HASH_CHANGED"):
        if fg.get(flag) is True:
            errors.append(f"{flag}=true in fresh results")
        if bg.get(flag) is True:
            errors.append(f"{flag}=true in baseline (corrupt pin)")
    if fg.get("CRASH_FOUND") not in (None, "NO", False):
        errors.append(f"CRASH_FOUND={fg.get('CRASH_FOUND')}")
    if fg.get("PERFORMANCE_BASELINE") == "FAIL":
        errors.append("fresh PERFORMANCE_BASELINE=FAIL")

    if errors:
        print("PERFORMANCE_BASELINE=FAIL")
        for e in errors:
            print(f"  - {e}")
        return 1
    print("PERFORMANCE_BASELINE=PASS")
    print("PERFORMANCE_CHANGES=REPORTED")
    return 0


if __name__ == "__main__":
    sys.exit(main())

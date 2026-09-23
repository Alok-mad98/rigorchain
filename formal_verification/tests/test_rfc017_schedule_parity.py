#!/usr/bin/env python3
"""RFC-020 schedule parity: C++ ResDestroyCostAtHeight ≡ pinned Coq table.

Does not call coqc (pins match ResSchedule.destroy_cost_pin_table).
Python L1 lottery spec was removed in Batch A; this gate is C++ ≡ Coq only.
Exit 1 on mismatch.

The filename is historical (RFC-017 lottery table). The live rule is RFC-020.
"""

from __future__ import annotations

import subprocess
import sys
import tempfile
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]

# Pinned by ResSchedule.destroy_cost_pin_table / cap_from_flat_subsidy_and_pole
COQ_PRICE_PINS = {
    0: 102_400_000_000,
    1: 102_400_006_103,
    210_000: 103_685_749_152,
    420_000: 104_979_520_049,
    630_000: 106_281_312_688,
}
COQ_SUM = 419_430_400_000_000
EPOCH_HEIGHTS = [k * 210_000 for k in range(9)]


def cpp_table(heights: list[int]) -> tuple[int, dict[int, int]]:
    src = r"""
#include <consensus/res_issuance.h>
#include <cstdio>
int main() {
  using namespace Consensus;
  std::printf("%lld\n", (long long)ResDerivedMaxSupply());
"""
    for h in heights:
        src += f'  std::printf("%lld\\n", (long long)ResDestroyCostAtHeight({h}));\n'
    src += "  return 0;\n}\n"
    with tempfile.TemporaryDirectory() as td:
        cpp = Path(td) / "sched.cpp"
        bin_path = Path(td) / "sched"
        cpp.write_text(src)
        cmd = [
            "g++",
            "-std=c++17",
            f"-I{REPO / 'core' / 'src'}",
            "-O0",
            "-o",
            str(bin_path),
            str(cpp),
        ]
        subprocess.check_call(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
        out = subprocess.check_output([str(bin_path)], text=True).strip().splitlines()
    total = int(out[0])
    values = {h: int(out[i + 1]) for i, h in enumerate(heights)}
    return total, values


def main() -> int:
    errors: list[str] = []
    heights = sorted(set(COQ_PRICE_PINS) | set(EPOCH_HEIGHTS))
    cpp_sum, cpp_vals = cpp_table(heights)
    if cpp_sum != COQ_SUM:
        errors.append(f"cap mismatch C++={cpp_sum} Coq={COQ_SUM}")

    for h, pin in COQ_PRICE_PINS.items():
        cpp = cpp_vals.get(h)
        if cpp != pin:
            errors.append(f"h={h}: C++={cpp} Coq pin={pin}")

    for k in range(8):
        cur = cpp_vals[EPOCH_HEIGHTS[k]]
        nxt = cpp_vals[EPOCH_HEIGHTS[k + 1]]
        if nxt < cur:
            errors.append(f"price decreased at observational k={k}: {cur} -> {nxt}")

    if errors:
        print("FAIL: RFC-020 schedule parity")
        for e in errors[:20]:
            print(f"  - {e}")
        if len(errors) > 20:
            print(f"  ... and {len(errors) - 20} more")
        return 1

    print("PASS: RFC-020 schedule parity (C++ ≡ Coq pins)")
    print(f"  ResDerivedMaxSupply = {cpp_sum}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

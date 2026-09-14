#!/usr/bin/env python3
"""RFC-020 schedule parity gate: C++ ≡ Python ≡ pinned Coq table.

Compares ResDestroyCostAtHeight at the golden heights and ResDerivedMaxSupply.
Does not call coqc (pins match ResSchedule.destroy_cost_pin_table). Exit 1 on mismatch.

The filename is historical (RFC-017 lottery table). The live rule is RFC-020.
"""

from __future__ import annotations

import subprocess
import sys
import tempfile
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
SPEC = REPO / "formal_verification" / "spec"
sys.path.insert(0, str(SPEC))

from rigor_consensus_spec import (  # noqa: E402
    RES_SCHEDULE_SUM,
    res_destroy_cost_at_height,
    res_destroy_cost_for_epoch,
)

# Pinned by ResSchedule.destroy_cost_pin_table / cap_from_flat_subsidy_and_pole
COQ_PRICE_PINS = {
    0: 102_400_000_000,
    1: 102_400_006_103,
    210_000: 103_685_749_152,
    420_000: 104_979_520_049,
    630_000: 106_281_312_688,
}
COQ_EPOCH_PINS = {
    0: 102_400_000_000,
    1: 103_685_749_152,
}
COQ_SUM = 419_430_400_000_000


def cpp_table() -> tuple[int, dict[int, int]]:
    heights = sorted(COQ_PRICE_PINS)
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
    cpp_sum, cpp_vals = cpp_table()
    if cpp_sum != COQ_SUM or cpp_sum != RES_SCHEDULE_SUM:
        errors.append(f"cap mismatch C++={cpp_sum} Python={RES_SCHEDULE_SUM} Coq={COQ_SUM}")

    for h, pin in COQ_PRICE_PINS.items():
        py = res_destroy_cost_at_height(h)
        cpp = cpp_vals.get(h)
        if py != cpp:
            errors.append(f"h={h}: Python={py} C++={cpp}")
        if py != pin:
            errors.append(f"h={h}: Python={py} Coq pin={pin}")

    for k, pin in COQ_EPOCH_PINS.items():
        py = res_destroy_cost_for_epoch(k)
        if py != pin:
            errors.append(f"k={k}: Python={py} Coq pin={pin}")

    # Display destroy cost is nondecreasing on the observational calendar.
    for k in range(8):
        if res_destroy_cost_for_epoch(k + 1) < res_destroy_cost_for_epoch(k):
            errors.append(f"price decreased at observational k={k}")

    if errors:
        print("FAIL: RFC-020 schedule parity")
        for e in errors[:20]:
            print(f"  - {e}")
        if len(errors) > 20:
            print(f"  ... and {len(errors) - 20} more")
        return 1

    print("PASS: RFC-020 schedule parity (C++ ≡ Python ≡ Coq pins)")
    print(f"  ResDerivedMaxSupply = {cpp_sum}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""RFC-017 schedule parity gate: C++ formula mirror ≡ Python ≡ pinned Coq table.

Compares ResRewardForEpoch(k) for k in 0..251. Does not call coqc (pins are in
ResSchedule.reward_pin_table / schedule_sum_value). Exit 1 on any mismatch.
"""

from __future__ import annotations

import math
import subprocess
import sys
import tempfile
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
SPEC = REPO / "formal_verification" / "spec"
sys.path.insert(0, str(SPEC))

from rigor_consensus_spec import (  # noqa: E402
    RES_SCHEDULE_SUM,
    res_reward_for_epoch,
)

# Pinned by ResSchedule.reward_pin_table / schedule_sum_value
COQ_PINS = {
    0: 40_000_000,
    1: 14_142_196,
    10: 1_096_405,
    100: 39_407,
    250: 10_058,
    251: 0,
}
COQ_SUM = 99_450_437


def cpp_table() -> tuple[int, list[int]]:
    src = r"""
#include <consensus/rigor_consensus_lock.h>
#include <cstdio>
int main() {
  using namespace Consensus;
  std::printf("%lld\n", (long long)ResScheduleSum());
  for (int k = 0; k <= 251; ++k)
    std::printf("%lld\n", (long long)ResRewardForEpoch(k));
  return 0;
}
"""
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
    values = [int(x) for x in out[1:]]
    return total, values


def main() -> int:
    errors: list[str] = []
    cpp_sum, cpp_vals = cpp_table()
    if len(cpp_vals) != 252:
        errors.append(f"C++ emitted {len(cpp_vals)} epochs, expected 252")
    if cpp_sum != COQ_SUM or cpp_sum != RES_SCHEDULE_SUM:
        errors.append(f"sum mismatch C++={cpp_sum} Python={RES_SCHEDULE_SUM} Coq={COQ_SUM}")

    for k in range(252):
        py = res_reward_for_epoch(k)
        cpp = cpp_vals[k] if k < len(cpp_vals) else None
        if py != cpp:
            errors.append(f"k={k}: Python={py} C++={cpp}")
        if k in COQ_PINS and py != COQ_PINS[k]:
            errors.append(f"k={k}: Python={py} Coq pin={COQ_PINS[k]}")

    # Cost-floor side: W nonincreasing on paying epochs
    for k in range(250):
        if res_reward_for_epoch(k + 1) > res_reward_for_epoch(k):
            errors.append(f"reward increased at k={k}")

    if errors:
        print("FAIL: RFC-017 schedule parity")
        for e in errors[:20]:
            print(f"  - {e}")
        if len(errors) > 20:
            print(f"  ... and {len(errors) - 20} more")
        return 1

    print("PASS: RFC-017 schedule parity (C++ ≡ Python ≡ Coq pins; k=0..251)")
    print(f"  ResScheduleSum = {cpp_sum}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

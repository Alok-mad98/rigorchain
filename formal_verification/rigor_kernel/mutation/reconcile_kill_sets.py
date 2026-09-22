#!/usr/bin/env python3
"""Compare the measured kill sets against what the mutants declare.

`mutation/kill_set.sh` applies each mutant and then admits whichever
proof fails, recompiling until the library builds; what it admitted is
what the mutant invalidates. That is ground truth. The theorem list
written next to each `run_mutation` call is a claim about the same
thing, and the two had never been compared across the whole suite.

The gap runs one way in practice. A mutant stops as soon as it has
seen its declared theorems fail, so anything else it kills is never
recorded, and a statement can be covered by an existing mutant for
years while a coverage gate reports it as untested. M42 was killing
`surface_validate_kernel_eq` and `validate_kernel_res` without naming
either.

Reports, for one kill-set log:

  UNDECLARED   killed by a mutant, named by none
  UNKILLED     declared by a mutant the log shows not killing it
  RETIRABLE    UNDECLARED and currently in the Category B debt file,
               so declaring it retires debt with no new mutant

Usage:
    python3 mutation/reconcile_kill_sets.py /tmp/killset_full.log
"""
from __future__ import annotations

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]


def measured(log: pathlib.Path) -> dict[str, set[str]]:
    out: dict[str, set[str]] = {}
    for line in log.read_text().splitlines():
        m = re.match(r"^KILL_SET id=(\S+) :\s*(.*)$", line)
        if m:
            out.setdefault(m.group(1), set()).update(m.group(2).split())
    return out


def declared() -> dict[str, set[str]]:
    out: dict[str, set[str]] = {}
    for script in sorted((ROOT / "mutation").glob("mutate_*.sh")):
        for m in re.finditer(r'run_mutation\s+(M\d+)\s+("[^"]*"|\S+)',
                             script.read_text()):
            out.setdefault(m.group(1), set()).update(
                m.group(2).strip('"').split())
    return out


def debt() -> set[str]:
    path = ROOT / "release/CategoryBCoverageDebt.md"
    if not path.is_file():
        return set()
    return {line.split()[0] for line in path.read_text().splitlines()
            if line and not line.startswith(("#", " ")) and "  " in line}


def main() -> int:
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    log = pathlib.Path(sys.argv[1])
    meas, decl, owed = measured(log), declared(), debt()

    missing_from_log = sorted(set(decl) - set(meas))
    if missing_from_log:
        print(f"NOTE: log has no kill set for {len(missing_from_log)} "
              f"mutants: {' '.join(missing_from_log)}")

    all_killed = {n for names in meas.values() for n in names}
    all_named = {n for names in decl.values() for n in names}

    undeclared = sorted(all_killed - all_named)
    print(f"\nUNDECLARED={len(undeclared)}  "
          "(killed by some mutant, named by none)")
    for name in undeclared:
        by = " ".join(sorted(mid for mid, ns in meas.items() if name in ns))
        mark = "  <- in debt" if name in owed else ""
        print(f"  {name:52s} {by}{mark}")

    unkilled = sorted(
        (mid, n) for mid, names in decl.items()
        for n in names - meas.get(mid, set()) if mid in meas)
    print(f"\nUNKILLED={len(unkilled)}  "
          "(declared by a mutant the log shows not killing it)")
    for mid, name in unkilled:
        print(f"  {mid:6s} {name}")

    retirable = sorted(set(undeclared) & owed)
    print(f"\nRETIRABLE={len(retirable)} of {len(owed)} debt entries "
          "(declaring an existing mutant is enough)")
    for name in retirable:
        by = " ".join(sorted(mid for mid, ns in meas.items() if name in ns))
        print(f"  {name:52s} {by}")

    still = sorted(owed - set(undeclared))
    print(f"\nSTILL_UNCOVERED={len(still)} (need a new mutant, a "
          "reclassification, or a restatement)")
    for name in still:
        print(f"  {name}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

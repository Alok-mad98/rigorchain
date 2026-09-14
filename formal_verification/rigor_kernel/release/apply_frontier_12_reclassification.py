#!/usr/bin/env python3
"""Reclassify what frontier item 1.2 changed, in both copies.

Removing the mutation-only switches (`kernel_step_unique`,
`executable_step_unique`, `runtime_step_unique`, `machine_step_unique`,
`concrete_oru_bypass`) settles what the determinism theorems were
worth.

Each transition relation used to carry a second constructor guarded by
a constant that only a mutant ever cleared. The constructor was
unreachable, so the relation was already the graph of a function and
determinism was immediate. What the switch bought was falsifiability:
flip it and determinism breaks, which made the theorem look like a
claim about consensus rules. It was not.

So two theorems leave Category A. They are not weaker than they were
-- their proofs are unchanged and still close -- but a Category A
theorem is one whose failure would mean the consensus rules are wrong,
and no assignment of the kernel's rules can make a function
non-deterministic.

What replaces them is the correspondence itself: the relation admits
exactly the steps the function takes, on exactly the inputs the guard
accepts. That is a real constraint on the rules, and removing the
guard breaks it -- which is what M29, M35 and M40 now do.
"""
from __future__ import annotations

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
TARGETS = [ROOT / "release/TheoremClassification.md",
           ROOT / "evidence/TheoremClassification.md"]

# theorem -> (from, to)
DEMOTIONS = {
    "machine_step_deterministic": ("A", "C"),
    "script_step_deterministic": ("A", "C"),
}

# theorem -> (file, domain, category)
ADDITIONS = {
    "kernel_apply_is_graph":
        ("Kernel/UnifiedTransition.v", "Unified Kernel", "B"),
    "executable_apply_is_graph":
        ("ExecutionBoundary/ExecutableTransition.v", "Execution Boundary", "B"),
    "runtime_apply_is_graph":
        ("Runtime/RuntimeTransition.v", "Runtime", "B"),
    "machine_step_is_graph":
        ("Execution/KernelMachine.v", "Execution", "B"),
}

SECTION = re.compile(r"^## (?:Category|Class) ([ABC])\b.*$", re.M)
ROW = re.compile(r"^\|\s*`([^`]+)`\s*\|", re.M)


def section_spans(text: str) -> dict[str, tuple[int, int]]:
    marks = [(m.group(1), m.end()) for m in SECTION.finditer(text)]
    if len(marks) != 3:
        sys.exit("apply_frontier_12: expected three category sections")
    starts = [m.start() for m in SECTION.finditer(text)]
    spans = {}
    for idx, (cat, end) in enumerate(marks):
        stop = starts[idx + 1] if idx + 1 < len(starts) else len(text)
        spans[cat] = (end, stop)
    return spans


def row_for(name: str, text: str) -> str | None:
    m = re.search(rf"^\|\s*`{re.escape(name)}`\s*\|.*$", text, re.M)
    return m.group(0) if m else None


def category_of(name: str, text: str) -> str | None:
    row = row_for(name, text)
    if row is None:
        return None
    at = text.index(row)
    for cat, (lo, hi) in section_spans(text).items():
        if lo <= at < hi:
            return cat
    return None


def insert_row(text: str, cat: str, row: str) -> str:
    """Insert a row in alphabetical order without touching the header."""
    lo, hi = section_spans(text)[cat]
    block = text[lo:hi]
    name = row.split("`")[1]
    at = None
    for m in ROW.finditer(block):
        if m.group(1) > name:
            at = m.start()
            break
    if at is None:
        rows = list(ROW.finditer(block))
        if not rows:
            sys.exit(f"apply_frontier_12: no rows in category {cat}")
        last = rows[-1]
        at = block.index("\n", last.start()) + 1
    return text[:lo] + block[:at] + row + "\n" + block[at:] + text[hi:]


def main() -> int:
    for path in TARGETS:
        if not path.is_file():
            sys.exit(f"apply_frontier_12: missing {path}")
        text = path.read_text()

        for name, (src, dst) in DEMOTIONS.items():
            if category_of(name, text) != src:
                continue                       # already moved
            row = row_for(name, text)
            text = text.replace(row + "\n", "", 1)
            text = insert_row(text, dst, row)

        for name, (relfile, domain, cat) in ADDITIONS.items():
            if row_for(name, text):
                continue
            text = insert_row(text, cat,
                              f"| `{name}` | `{relfile}` | {domain} |")

        path.write_text(text)
        print(f"updated {path.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

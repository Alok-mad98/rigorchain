#!/usr/bin/env python3
"""Frontier 2.1: classify the chain-level layer.

Every consensus theorem in this package used to be about a single
step. `Kernel/Chain/ChainTransition.v` adds the fold over a list of
inputs and proves the invariants by induction over it;
`Kernel/Chain/ChainRefinement.v` lifts the refinement stack so the
machine layer runs a whole chain rather than one block.

The A/B line is drawn the same way as in 1.4: by what a mutant can
kill. Twelve statements do work a wrong definition can break, and
M77-M87 break them (`reachable_preserves_invariants` is the one
deferral, and the coverage gate verifies its components rather than
taking the deferral on trust). The rest either forward to those or are
the non-vacuity witnesses, which this package has always kept in
Category B --- `machine_refinement_is_partial` and
`checker_is_not_constant` are the existing precedent.

Applies the same edit to both classification tables.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TABLES = [ROOT / "release" / "TheoremClassification.md",
          ROOT / "evidence" / "TheoremClassification.md"]

SECTION = re.compile(r"^##+\s+(?:Category|Class)\s+([ABC])\b", re.I)
ROW = re.compile(r"^\|\s*`([^`]+)`\s*\|\s*`([^`]+)`\s*\|")

CHAIN = "Kernel/Chain/ChainTransition.v"
REFINE = "Kernel/Chain/ChainRefinement.v"

ADD_A = [
    ("chain_no_hidden_creation", CHAIN,
     "Over a run, the ORU stock moves by subsidy plus transfers in, "
     "minus transfers out and burn, and by nothing else. M81"),
    ("chain_oru_conservation", CHAIN,
     "Tip stock equals start stock plus the summed per-block flow. The "
     "chain-level form of conservation; a single step cannot state it. "
     "M80"),
    ("chain_res_cap_fixed", CHAIN,
     "The RES cap a run is measured against is the cap it started "
     "with. Rules out a chain that raises its own ceiling block by "
     "block, which no single-step theorem can see. M82"),
    ("chain_res_monotone", CHAIN,
     "RES issuance never runs backwards along an acceptable run. M83"),
    ("concrete_chain_commute", REFINE,
     "The concrete layer's run projects to the abstract run. M84"),
    ("executable_chain_commute", REFINE,
     "The executable layer's run projects to the concrete run. M85"),
    ("executable_chain_preserves_invariants", REFINE,
     "Executable invariants survive a whole run, with acceptability "
     "transported through the projection at each step. M86"),
    ("kernel_chain_is_graph", CHAIN,
     "The chain relation is exactly the chain fold on acceptable runs. "
     "M77 mis-evaluates acceptability at the initial state and this "
     "stops proving"),
    ("kernel_chain_preserves_invariants", CHAIN,
     "The six kernel invariants survive a run of arbitrary length, by "
     "induction on the run rather than by iterating the single-step "
     "result at the call site. M78"),
    ("kernel_chain_states_invariant", CHAIN,
     "Every state the run passes through satisfies the invariants, not "
     "only the tip. Strictly stronger than the preceding row. M79"),
    ("machine_chain_refines_executable_chain", REFINE,
     "A run the bounded-word machine completes decodes to the run the "
     "model computes. M87"),
    ("reachable_preserves_invariants", CHAIN,
     "Nothing an acceptable run can reach violates the invariants. "
     "Deferral to `kernel_chain_is_graph` and "
     "`kernel_chain_preserves_invariants`; the coverage gate checks "
     "the components"),
]

ADD_B = [
    ("chain_ok_accepts_funded_spend", CHAIN,
     "Non-vacuity: the spend rejected above is accepted once an "
     "earlier block funds it. The pair is what shows acceptability "
     "reads the running state and not the initial one. M88 covers the "
     "rejection half"),
    ("chain_ok_inhabited", CHAIN,
     "Non-vacuity: acceptable runs of every length exist, so the chain "
     "theorems are not empty past the first block"),
    ("chain_ok_inhabited_with_issuance", CHAIN,
     "Non-vacuity: runs that actually issue exist, so the previous row "
     "is not an artefact of the all-zero input"),
    ("chain_ok_rejects_overspend", CHAIN,
     "Non-vacuity, other direction: the acceptability condition "
     "rejects a block spending more than the running stock. M88"),
    ("chain_res_capped", CHAIN,
     "The cap holds at the tip, measured against the starting cap. "
     "Forwards to `chain_res_cap_fixed` and "
     "`kernel_chain_preserves_invariants`"),
    ("executable_chain_refines_kernel_chain", REFINE,
     "Executable run projects to the abstract run. Composes "
     "`executable_chain_commute` and `concrete_chain_commute`; the "
     "killable content is in those two"),
    ("kernel_chain_app", CHAIN,
     "Running two chains in sequence is running their concatenation. "
     "Forwards to `kernel_chain_is_graph`"),
    ("kernel_chain_deterministic", CHAIN,
     "A run has one endpoint. Forwards to `kernel_chain_is_graph`, as "
     "`kernel_transition_deterministic` does one layer down"),
    ("machine_chain_completes_on_idle", REFINE,
     "Non-vacuity: idle runs of every length complete on the machine, "
     "so the guard on `machine_chain_sound` is satisfiable at every "
     "chain length and not only for the empty chain"),
    ("machine_chain_refinement_is_partial", REFINE,
     "Non-vacuity, other direction: an acceptable run the machine "
     "cannot finish. The failure is representation, not a rule "
     "violation. M89"),
    ("machine_chain_sound", REFINE,
     "End to end: a completed machine run decodes, projects onto the "
     "abstract run, and satisfies the invariants there. A bundle of "
     "the four Category A results it composes; no mutation can make "
     "its own proof fail"),
]


def section_spans(lines: list[str]) -> dict[str, tuple[int, int]]:
    """Map A/B/C to the [start, end) line range of that section."""
    marks: list[tuple[int, str]] = []
    for i, line in enumerate(lines):
        m = SECTION.match(line.strip())
        if m:
            marks.append((i, m.group(1).upper()))
    spans: dict[str, tuple[int, int]] = {}
    for j, (i, cat) in enumerate(marks):
        end = marks[j + 1][0] if j + 1 < len(marks) else len(lines)
        spans[cat] = (i, end)
    return spans


def row_bounds(lines: list[str], span: tuple[int, int]) -> tuple[int, int]:
    """First and last data-row index within a section, exclusive end."""
    start, end = span
    first = last = None
    for i in range(start, end):
        if ROW.match(lines[i]):
            if first is None:
                first = i
            last = i
    if first is None:
        raise SystemExit(f"apply_frontier_21: no rows in section {span}")
    return first, last + 1


def apply(path: Path) -> int:
    lines = path.read_text().splitlines(keepends=True)
    spans = section_spans(lines)
    if set(spans) != {"A", "B", "C"}:
        raise SystemExit(f"apply_frontier_21: {path} has sections {sorted(spans)}")

    present = {(m.group(1).strip(), m.group(2).strip())
               for m in (ROW.match(l) for l in lines) if m}

    for cat, additions in (("A", ADD_A), ("B", ADD_B)):
        for name, rel, note in additions:
            if (name, rel) in present:
                continue
            spans = section_spans(lines)
            first, end = row_bounds(lines, spans[cat])
            new = f"| `{name}` | `{rel}` | {note} |\n"
            at = end
            for i in range(first, end):
                m = ROW.match(lines[i])
                if m and m.group(1).strip() > name:
                    at = i
                    break
            lines.insert(at, new)
            present.add((name, rel))

    path.write_text("".join(lines))
    return 0


def main() -> int:
    for path in TABLES:
        if not path.exists():
            raise SystemExit(f"apply_frontier_21: missing {path}")
        apply(path)
        print(f"apply_frontier_21: updated {path.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

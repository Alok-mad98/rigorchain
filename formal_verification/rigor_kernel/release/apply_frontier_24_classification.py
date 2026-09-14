#!/usr/bin/env python3
"""Frontier 2.4: classify the ASERT fixed-point width and wrap bounds.

2.4 turned out to be narrower than the roadmap entry implied. The
factor range, the monotonicity and the clamp bounds -- "the clamps keep
the result inside the representable target range" -- were already
proved: `factor_range`, `factor_mono`, `clamp_to_prev_bounds`,
`asert_next_wf`. What was missing was everything about the intermediate
polynomial, which is where the C++ does two unchecked casts, and the
soundness of the shift/remainder split at the point where it wraps.

The A/B line is drawn as in 1.4 and 2.1, by what a mutant can kill.
M90-M94 kill the three Category A rows. The three `_on_exponent` rows
discharge a hypothesis and forward; `factor_wrap_step_is_one` quantifies
a Category A row rather than adding to it. Those go in B, as forwarding
statements have throughout this package.

`asert_poly_exceeds_int64` goes in C. A mutant kills it (M93), but it
asserts nothing about the model: it records that a 64-bit signed
evaluation of this polynomial would overflow, which is a constraint on
implementations, not a consensus invariant. Category C is where the
package keeps statements that are about the boundary rather than about
the kernel.

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

CORR = "proofs/Pow_Correctness/ASERTCorrectness.v"

ADD_A = [
    ("asert_poly_pos", CORR,
     "The ASERT cubic is strictly positive on its whole domain, so the "
     "unsigned cast of `poly >> 48` in the C++ cannot wrap. A negative "
     "polynomial would not yield a small target but an enormous one, "
     "and nothing downstream checks it. M91"),
    ("asert_poly_fits_uint64", CORR,
     "The same polynomial stays under 2^64, by under one part in 10^5 "
     "at the top of the domain. Bounds the width the evaluation needs. "
     "M90"),
    ("factor_wrap_continuous", CORR,
     "`factor 65536 = 2 * factor 0` exactly. The exponent is split into "
     "a shift count and a remainder, and this is the statement that the "
     "two pieces agree where they meet, so the piecewise target has no "
     "jump as the remainder wraps. The most coefficient-sensitive "
     "result here. M90, M92, M93, M94"),
]

ADD_B = [
    ("asert_poly_fits_uint64_on_exponent", CORR,
     "`asert_poly_fits_uint64` with the domain hypothesis discharged by "
     "`exp_rem_range`, hence holding for every exponent including a "
     "hostile one. Forwards; the killable content is in the parent"),
    ("asert_poly_pos_on_exponent", CORR,
     "`asert_poly_pos` with the domain hypothesis discharged by "
     "`exp_rem_range`. Forwards"),
    ("factor_range_on_exponent", CORR,
     "`factor_range` with the domain hypothesis discharged by "
     "`exp_rem_range`. Forwards"),
    ("factor_wrap_step_is_one", CORR,
     "Quantifies `factor_wrap_continuous`: the step taken at the wrap "
     "is one unit, the same as an ordinary interior step, so the seam "
     "is not merely absent but invisible"),
]

ADD_C = [
    ("asert_poly_exceeds_int64", CORR,
     "Implementation constraint, not a kernel invariant: at the top of "
     "the domain the polynomial exceeds 2^63, so the `__int128` "
     "promotion in `core/src/pow.cpp` is load-bearing and an `int64_t` "
     "evaluation would be signed overflow. Recorded so the requirement "
     "cannot be optimised away silently. M93"),
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
        raise SystemExit(f"apply_frontier_24: no rows in section {span}")
    return first, last + 1


def apply(path: Path) -> int:
    lines = path.read_text().splitlines(keepends=True)
    spans = section_spans(lines)
    if set(spans) != {"A", "B", "C"}:
        raise SystemExit(f"apply_frontier_24: {path} has sections {sorted(spans)}")

    present = {(m.group(1).strip(), m.group(2).strip())
               for m in (ROW.match(l) for l in lines) if m}

    for cat, additions in (("A", ADD_A), ("B", ADD_B), ("C", ADD_C)):
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
            raise SystemExit(f"apply_frontier_24: missing {path}")
        apply(path)
        print(f"apply_frontier_24: updated {path.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

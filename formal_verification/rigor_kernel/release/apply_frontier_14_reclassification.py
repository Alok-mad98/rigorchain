#!/usr/bin/env python3
"""Frontier 1.4: reclassify the script classifier after totality/partition.

Before 1.4 the classifier's public-facing statements were
[classify_total] (exists t, classify_script s = t), [classify_type_total]
(same at the asset level) and [classify_authority_free]
(classify_asset s = classify_asset s). All three hold of any function
whatsoever, so none of them constrained this classifier.

They are now range membership, exact range, and payload independence,
proved from first-byte recognisers that are shown to be pairwise
disjoint. That moves them into Category A and brings the supporting
partition lemmas in as Category B. The two remaining self-equalities
are deleted from the source, so their rows go with them.

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

# Category A is reserved for the statements a wrong classifier could
# actually fail, which here means the ones a mutant can kill: the range
# results, the three recognizer-disjointness lemmas, and the fact that
# an arbitrary 20-byte payload really does build a mint script. The
# statements above them --- the hit count, the ladder-order result, the
# payload-independence restatements --- are proved by forwarding to
# these, so no mutation of any definition can make their proofs fail;
# they are Category B with a pointer to where the content is.
ADD_A = [
    ("classify_range_exact", "Script/ScriptClassifier.v", "Script"),
    ("classify_total", "Script/ScriptClassifier.v", "Script"),
    ("classify_type_total",
     "proofs/Script_Correctness/ScriptCorrectness.v", "Script"),
    ("converge_not_null_data", "Script/ScriptClassifier.v", "Script"),
    ("mint_not_converge", "Script/ScriptClassifier.v", "Script"),
    ("mint_not_null_data", "Script/ScriptClassifier.v", "Script"),
    ("mint_script_of_is_mint", "Script/ScriptClassifier.v", "Script"),
]

ADD_B = [
    ("classify_authority_free",
     "proofs/Script_Correctness/ScriptCorrectness.v",
     "Payload independence at the asset level. Forwards to "
     "`mint_shape_classifies`; the killable content is "
     "`mint_script_of_is_mint`"),
    ("classify_payload_independent", "Script/ScriptClassifier.v",
     "Payload independence at the script-type level. Same forwarding, "
     "same source of content"),
    ("ladder_order_immaterial", "Script/ScriptClassifier.v",
     "Reordering the mint/converge/null-data ladder cannot change a "
     "class. Forwards to `shape_hits_le_one`"),
    ("mint_shape_classifies", "Script/ScriptClassifier.v",
     "Every 20-byte mint payload lands in `T_MINT`. Forwards to "
     "`mint_script_of_is_mint`"),
    ("no_shape_is_nonstandard", "Script/ScriptClassifier.v",
     "A script matching no recognizer is `T_NONSTANDARD`"),
    ("shape_hits_le_one", "Script/ScriptClassifier.v",
     "At most one shape matches. Counts the three Class-A disjointness "
     "results; adds no constraint they do not already carry"),
]

DROP = [
    ("classify_deterministic", "proofs/Script_Correctness/ScriptCorrectness.v"),
    ("classify_total", "proofs/Script_Correctness/ScriptCorrectness.v"),
    ("classify_deterministic", "Script/ScriptClassifier.v"),
    # Rows this script may have written under an earlier split of the
    # A/B line; re-inserted below from the lists above.
    ("classify_authority_free", "proofs/Script_Correctness/ScriptCorrectness.v"),
    ("classify_payload_independent", "Script/ScriptClassifier.v"),
    ("shape_hits_le_one", "Script/ScriptClassifier.v"),
]

RENOTE = {
    ("script_correctness", "proofs/Script_Correctness/ScriptCorrectness.v"):
        "Bundles the classifier range, surjectivity, payload independence "
        "and recognizer disjointness. The Class-A content is in those four",
}


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
        raise SystemExit(f"apply_frontier_14: no rows in section {span}")
    return first, last + 1


def apply(path: Path) -> int:
    lines = path.read_text().splitlines(keepends=True)
    spans = section_spans(lines)
    if set(spans) != {"A", "B", "C"}:
        raise SystemExit(f"apply_frontier_14: {path} has sections {sorted(spans)}")

    # Deletions first: they do not move a section boundary by more than
    # the number of rows removed, and re-deriving spans afterwards keeps
    # the insertion arithmetic honest.
    keep = []
    for i, line in enumerate(lines):
        m = ROW.match(line)
        if m and (m.group(1).strip(), m.group(2).strip()) in DROP:
            continue
        keep.append(line)
    lines = keep

    # Note rewrites, in place.
    for i, line in enumerate(lines):
        m = ROW.match(line)
        if not m:
            continue
        key = (m.group(1).strip(), m.group(2).strip())
        if key in RENOTE:
            lines[i] = f"| `{key[0]}` | `{key[1]}` | {RENOTE[key]} |\n"

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
            raise SystemExit(f"apply_frontier_14: missing {path}")
        apply(path)
        print(f"apply_frontier_14: updated {path.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

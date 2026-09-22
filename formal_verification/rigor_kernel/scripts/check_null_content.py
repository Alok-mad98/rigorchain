#!/usr/bin/env python3
"""Gate: no theorem states something that holds of any function.

Frontier item 1.4 found four of these in the classifier —
`classify_deterministic : classify_script s = classify_script s` and
three siblings — and fixed them there. Nothing swept the rest of the
tree. Restating the same shapes over an opaque `Parameter f` and
proving them with the original tactics shows the class is still
present in 22 places.

Four shapes are rejected, because each is provable with `f` unknown:

  self-equality   forall x, f x = f x
  self-iff        forall x, P x <-> P x
  congruence      forall x y, x = y -> f x = f y
  function-ness   forall a r1 r2, f a = r1 -> f a = r2 -> r1 = r2

A statement of one of these shapes constrains nothing. It is true of
the checker that answers `true` everywhere, so it is not evidence
about the rule whose name it carries. That is the whole content of
stopping criterion 4 in `release/FormalizationFrontier.md`.

The existing instances are pinned in
`release/NullContentInventory.md` with their category, so the set can
only shrink. A new one fails the gate.

Usage:
    python3 scripts/check_null_content.py           # gate
    python3 scripts/check_null_content.py --write   # regenerate
    python3 scripts/check_null_content.py --self-test
"""
from __future__ import annotations

import pathlib
import re
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))

from check_mutation_coverage import (                  # noqa: E402
    ROOT,
    SKIP_PARTS,
    read_source,
)

INVENTORY = ROOT / "release/NullContentInventory.md"

DECL = re.compile(
    r"^\s*(?:Theorem|Lemma|Corollary|Remark|Fact|Example|Proposition)\s+"
    r"([A-Za-z_][A-Za-z0-9_']*)\s*(.*?)\.\s*Proof\.", re.S | re.M)
PROP_DEF = re.compile(
    r"^\s*Definition\s+([A-Za-z_][A-Za-z0-9_']*)\s*(?:\([^)]*\)\s*)*"
    r":\s*Prop\s*:=\s*(.*?)\.\s*$", re.S | re.M)
SPLIT_EQ = re.compile(r"(?<![<>=!])=(?!=)")

HEADER = """# Null Content Inventory

Theorems whose statement holds of **any** function, and therefore
says nothing about the rule named in it. Detected by
`scripts/check_null_content.py`; see that file for the four shapes.

Frontier 1.4 removed this class from the classifier. These are the
instances elsewhere in the tree. None of them is in Category A, so
the public claim does not rest on any of them, but a statement of
this shape is not a supporting lemma either and should be replaced by
one with content or moved to Category C as a boundary marker.

Rows marked `abstract-section` sit inside a `Section` that declares a
`Variable`, almost always the uninterpreted PoW digest `H`. Over an
abstract function the only available facts *are* the free ones, so
these were an attempt to state something where nothing can be stated.
Their honest form is a Category C boundary marker saying `H` is
uninterpreted, which is what `Audit/KernelNonClaims.v` already does
with `kernel_does_not_assume_sha3_security`.

Generated. Do not edit by hand: CI regenerates and compares the set.
A name leaves this file by being restated with content or deleted. A
new name appearing here fails the gate.

"""


def prop_definitions() -> dict[str, str]:
    out: dict[str, str] = {}
    for path in source_files():
        for m in PROP_DEF.finditer(read_source(path)):
            out[m.group(1)] = " ".join(m.group(2).split())
    return out


def source_files() -> list[pathlib.Path]:
    return [p for p in sorted(ROOT.rglob("*.v"))
            if not any(part in SKIP_PARTS
                       for part in p.relative_to(ROOT).parts)]


def strip_binders(text: str) -> str:
    text = text.strip()
    if text.startswith(":"):
        text = text[1:]
    while True:
        m = re.match(r"\s*forall\b[^,]*,", text)
        if not m:
            break
        text = text[m.end():]
    return " ".join(text.split())


def norm(text: str) -> str:
    return " ".join(text.replace("(", " ( ").replace(")", " ) ").split())


def unwrap(text: str) -> str:
    """Drop enclosing parentheses only when they wrap the whole term."""
    text = text.strip()
    while text.startswith("(") and text.endswith(")"):
        depth = 0
        whole = True
        for i, ch in enumerate(text):
            depth += ch == "("
            depth -= ch == ")"
            if depth == 0 and i < len(text) - 1:
                whole = False
                break
        if not whole:
            break
        text = text[1:-1].strip()
    return text


def shape_of(body: str) -> str | None:
    """Name the null shape a statement has, or None if it has content."""
    for part in (re.split(r"/\\", body) if "/\\" in body else [body]):
        part = unwrap(part)
        if "->" in part or "<->" in part:
            continue
        sides = SPLIT_EQ.split(part)
        if len(sides) == 2 and norm(sides[0]) and norm(sides[0]) == norm(sides[1]):
            return "self-equality"
    for part in (re.split(r"/\\", body) if "/\\" in body else [body]):
        part = unwrap(part)
        if "<->" in part and "->" not in part.replace("<->", ""):
            left, right = part.split("<->", 1)
            if norm(left) == norm(right):
                return "self-iff"
    m = re.match(r"^\s*([A-Za-z0-9_']+)\s*=\s*([A-Za-z0-9_']+)\s*->\s*(.*)$", body)
    if m:
        x, y, concl = m.groups()
        sides = SPLIT_EQ.split(concl)
        if len(sides) == 2 and norm(sides[0].replace(x, "@")) == norm(
                sides[1].replace(y, "@")):
            return "congruence"
    if re.search(r"=\s*(r1|res1|o1)\s*->.*=\s*(r2|res2|o2)\s*->\s*\1\s*=\s*\2",
                 body):
        return "function-ness"
    return None


def scan(text: str) -> list[tuple[str, str]]:
    """(name, shape) for every null-content statement in one source."""
    defs = scan.defs
    found = []
    for m in DECL.finditer(text):
        name, body = m.group(1), strip_binders(m.group(2))
        shape = shape_of(body)
        if shape is None:
            # One level of `Definition ... : Prop` hides the same shapes.
            alias = re.fullmatch(r"([A-Za-z_][A-Za-z0-9_']*)"
                                 r"((?:\s+[A-Za-z0-9_']+)*)", body)
            if alias and alias.group(1) in defs:
                shape = shape_of(strip_binders(defs[alias.group(1)]))
                if shape:
                    shape += f" (via {alias.group(1)})"
        if shape:
            found.append((name, shape))
    return found


SCOPE = re.compile(
    r"^\s*(?:(Section)\s+(\w+)|(End)\s+\w+|(Variables?)\s+[^.]*\.|"
    r"(?:Theorem|Lemma|Corollary|Remark|Fact|Example|Proposition)\s+"
    r"([A-Za-z_][A-Za-z0-9_']*))", re.M)


def abstract_scoped(text: str) -> set[str]:
    """Names proved inside a Section that declares a `Variable`.

    Over an abstract function the free facts are the only facts, so
    this distinguishes a statement that could have had content from
    one that could not.
    """
    open_sections: list[list] = []
    found: set[str] = set()
    for m in SCOPE.finditer(text):
        if m.group(1):
            open_sections.append([m.group(2), False])
        elif m.group(3):
            if open_sections:
                open_sections.pop()
        elif m.group(4):
            if open_sections:
                open_sections[-1][1] = True
        elif m.group(5) and any(has_var for _, has_var in open_sections):
            found.add(m.group(5))
    return found


def categories() -> dict[str, str]:
    text = (ROOT / "release/TheoremClassification.md").read_text()
    out: dict[str, str] = {}
    for letter in "ABC":
        start = text.find(f"## Category {letter}")
        if start < 0:
            continue
        rest = text[start + 1:]
        stop = re.search(r"^## ", rest, re.M)
        body = rest[:stop.start()] if stop else rest
        for m in re.finditer(r"^\|\s*`([^`]+)`\s*\|", body, re.M):
            if m.group(1) != "Theorem":
                out[m.group(1)] = letter
    return out


def collect() -> list[tuple[str, str, str, str]]:
    scan.defs = prop_definitions()
    cat = categories()
    rows = []
    for path in source_files():
        rel = str(path.relative_to(ROOT))
        text = read_source(path)
        abstract = abstract_scoped(text)
        for name, shape in scan(text):
            scope = "abstract-section" if name in abstract else "concrete"
            rows.append((name, rel, shape, cat.get(name, "unclassified"), scope))
    return sorted(rows)


def recorded() -> set[str]:
    if not INVENTORY.exists():
        return set()
    return {line.split("  ")[0].strip()
            for line in INVENTORY.read_text().splitlines()
            if line and not line.startswith(("#", " ")) and "  " in line}


def self_test() -> int:
    """The gate must reject a statement it is supposed to reject."""
    scan.defs = {}
    probes = {
        "Theorem probe : forall x, f x = f x. Proof. reflexivity. Qed.":
            "self-equality",
        "Theorem probe : forall x y, x = y -> f x = f y. Proof. auto. Qed.":
            "congruence",
        "Theorem probe : forall a r1 r2, f a = r1 -> f a = r2 -> r1 = r2. "
        "Proof. auto. Qed.": "function-ness",
        "Theorem probe : forall x, P x <-> P x. Proof. tauto. Qed.":
            "self-iff",
    }
    bad = []
    for src, want in probes.items():
        got = scan(src)
        if not got or got[0][1] != want:
            bad.append(f"  missed {want}: {src.splitlines()[0]}")
    # and it must not reject a statement with content
    keep = "Theorem probe : forall x, f x = g x. Proof. auto. Qed."
    if scan(keep):
        bad.append(f"  false positive on: {keep}")
    if bad:
        print("FAIL: check_null_content self-test", file=sys.stderr)
        for line in bad:
            print(line, file=sys.stderr)
        return 1
    print("NULL_CONTENT_SELF_TEST=PASS")
    return 0


def main() -> int:
    if "--self-test" in sys.argv:
        return self_test()
    if self_test() != 0:
        return 1

    rows = collect()
    by_cat: dict[str, int] = {}
    for _, _, _, cat, _ in rows:
        by_cat[cat] = by_cat.get(cat, 0) + 1

    print(f"NULL_CONTENT_TOTAL={len(rows)}")
    for cat in sorted(by_cat):
        print(f"NULL_CONTENT_CATEGORY_{cat.upper()}={by_cat[cat]}")
    print("NULL_CONTENT_IN_ABSTRACT_SECTION="
          f"{sum(1 for r in rows if r[4] == 'abstract-section')}")

    if "--write" in sys.argv:
        lines = [HEADER]
        for name, rel, shape, cat, scope in rows:
            lines.append(f"{name}  {rel}  {shape}  category={cat}  {scope}")
        INVENTORY.write_text("\n".join(lines) + "\n")
        print(f"WROTE {INVENTORY.relative_to(ROOT)}")
        return 0

    found = {r[0] for r in rows}
    listed = recorded()
    new = sorted(found - listed)
    stale = sorted(listed - found)
    if new:
        print("FAIL: statements that hold of any function, not inventoried:",
              file=sys.stderr)
        for name in new:
            print(f"  {name}", file=sys.stderr)
    if stale:
        print("FAIL: inventoried names no longer present; rerun --write:",
              file=sys.stderr)
        for name in stale:
            print(f"  {name}", file=sys.stderr)
    if by_cat.get("A"):
        print("FAIL: a Category A theorem holds of any function",
              file=sys.stderr)
    if new or stale or by_cat.get("A"):
        return 1

    print("NULL_CONTENT=INVENTORIED_NONE_IN_CATEGORY_A")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""SECURITY-001-F / F-4 regression guard: reject unmarked vacuous results.

A "vacuous" result is one that is true no matter what the definitions it names
actually do:

  * reflexivity-shaped -- the conclusion is literally `X = X`;
  * True-bodied        -- the conclusion is literally `True`;
  * `Definition <n> : Prop := True.` bookkeeping markers.

Such statements are not defects in themselves, but citing them as evidence is.
Finding F-4 of the pre-mainnet security review found `reorg_preserves_supply`
stated as `total_supply s_end = total_supply s_end` and presented as a supply
conservation guarantee.

This tool fails if a vacuous result exists WITHOUT an adjacent
`VACUOUS-PLACEHOLDER` comment, so new ones cannot be added silently.

Usage:
    cd formal_verification && python3 tools/check_vacuous.py
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

COQ_DIR = Path(__file__).resolve().parents[1] / "coq"
MARKER = "VACUOUS-PLACEHOLDER"
LOOKBACK = 600  # characters of preceding text searched for the marker

RESULT_RE = re.compile(
    r"^(Theorem|Lemma|Corollary|Proposition|Remark|Fact|Example)\s+([\w']+)\s*:(.*?)(?=^\s*Proof)",
    re.S | re.M,
)
PROP_TRUE_RE = re.compile(r"^Definition\s+([\w']+)\s*:\s*Prop\s*:=\s*True\.", re.M)


def strip_comments(text: str) -> str:
    """Blank out comments, preserving offsets so line numbers stay correct."""
    return re.sub(r"\(\*.*?\*\)", lambda m: re.sub(r"[^\n]", " ", m.group(0)), text, flags=re.S)


def classify(statement: str) -> str | None:
    stmt = " ".join(statement.split()).rstrip(".").strip()
    core = re.sub(r"^forall[^,]*,\s*", "", stmt)
    if core.strip() == "True" or core.strip().endswith("-> True"):
        return "True-bodied"
    if "=" in core:
        lhs, _, rhs = core.partition("=")
        if lhs.strip() and lhs.strip() == rhs.strip():
            return "reflexivity-shaped"
    return None


def marked(raw: str, offset: int) -> bool:
    return MARKER in raw[max(0, offset - LOOKBACK):offset]


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--list", action="store_true", help="list all vacuous results, marked or not")
    args = ap.parse_args()

    unmarked: list[tuple[str, int, str, str]] = []
    all_found: list[tuple[str, int, str, str]] = []

    for path in sorted(COQ_DIR.rglob("*.v")):
        raw = path.read_text(encoding="utf-8", errors="replace")
        body = strip_comments(raw)
        rel = str(path.relative_to(COQ_DIR))

        for m in RESULT_RE.finditer(body):
            kind = classify(m.group(3))
            if kind is None:
                continue
            line = body[: m.start()].count("\n") + 1
            rec = (rel, line, m.group(2), kind)
            all_found.append(rec)
            if not marked(raw, m.start()):
                unmarked.append(rec)

        for m in PROP_TRUE_RE.finditer(body):
            line = body[: m.start()].count("\n") + 1
            rec = (rel, line, m.group(1), "Prop := True")
            all_found.append(rec)
            if not marked(raw, m.start()):
                unmarked.append(rec)

    if args.list:
        for rel, line, name, kind in all_found:
            flag = " " if (rel, line, name, kind) not in unmarked else "!"
            print(f"{flag} {rel}:{line}  {name}  [{kind}]")
        print()

    print(f"vacuous results found   : {len(all_found)}")
    print(f"explicitly marked       : {len(all_found) - len(unmarked)}")
    print(f"UNMARKED (failures)     : {len(unmarked)}")

    if unmarked:
        print()
        print(f"The following are vacuous but carry no '{MARKER}' comment:")
        for rel, line, name, kind in unmarked:
            print(f"  {rel}:{line}  {name}  [{kind}]")
        print()
        print("Either strengthen the statement or add the marker comment above it.")
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())

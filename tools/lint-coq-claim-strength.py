#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""R1: Coq header 'Proves A = B' must match a forall-equality theorem."""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
COQ = ROOT / "formal_verification/coq"

CLAIM = re.compile(r"Proves\s+forall\s+(\w+)\s*,\s*(\w+)\s+\1\s*=\s*(\w+)\s+\1")
WEAK_CLAIM = re.compile(r"Proves\s+(\w+)\s*=\s*(\w+)(?!\s+\w)")

SKIP = {"Phase4GRefinement", "Phase5Equivalence", "extracted"}


def header_and_body(text: str) -> tuple[str, str]:
    if text.startswith("(**"):
        end = text.find("*)")
        if end != -1:
            return text[: end + 2], text[end + 2 :]
    return "", text


def has_full_eq(body: str, left: str, right: str) -> bool:
    pat = re.compile(
        rf"Theorem\s+\w+\s*:\s*forall\s+(\w+)\s*,\s*{re.escape(left)}\s+\1\s*=\s*{re.escape(right)}\s+\1\s*\.",
        re.DOTALL,
    )
    return bool(pat.search(body))


def main() -> int:
    errors: list[str] = []
    for path in sorted(COQ.rglob("*.v")):
        if any(p in SKIP for p in path.parts):
            continue
        text = path.read_text(encoding="utf-8")
        header, body = header_and_body(text)
        if not header:
            continue
        for m in CLAIM.finditer(header):
            left, right = m.group(2), m.group(3)
            if not has_full_eq(body, left, right):
                errors.append(
                    f"{path.relative_to(ROOT)}: header proves forall {m.group(1)}, "
                    f"{left} = {right} but no matching theorem"
                )
        for m in WEAK_CLAIM.finditer(header):
            left, right = m.group(1), m.group(2)
            if left in {"refinement_proved", "equivalence_proved"}:
                continue
            if has_full_eq(body, left, right):
                continue
            # Unguarded "Proves A = B" without a forall theorem is an overclaim.
            errors.append(
                f"{path.relative_to(ROOT)}: header 'Proves {left} = {right}' "
                f"is not backed by 'forall i, {left} i = {right} i'"
            )

    if errors:
        print("FAIL: Coq claim strength does not match theorems")
        for e in errors:
            print(f"  - {e}")
        return 1

    print("COQ_CLAIM_STRENGTH=PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())

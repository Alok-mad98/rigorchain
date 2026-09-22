#!/usr/bin/env python3
# Copyright (c) 2026 The RigorChain developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Gate 3 — Publication boundary check.

Documentation integrity, not consensus. Requires the public narrative to keep
declaring the three non-claims, and rejects an unnegated “RIGOR is formally
verified” slogan on the publication surface.

Reproduce:
  python3 tools/check-publication-boundary.py
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

FLAGS = (
    "equivalence_claimed = false",
    "cpp_formal_equivalence = false",
    "documentation_consensus_authority = false",
)

REQUIRED_FILES = (
    ROOT / "README.md",
    ROOT / "docs/WHITEPAPER.md",
    ROOT / "docs/whitepaper/RIGOR_CONVERGENCE_MATHEMATICAL_NOTE.md",
)

SCAN_GLOBS = (
    "README.md",
    "docs/WHITEPAPER.md",
    "docs/whitepaper/**/*.md",
    "web/rigorchain-site/src/**/*.ts",
    "web/rigorchain-site/src/**/*.tsx",
)

FORBIDDEN = (
    re.compile(r"\brigor is formally verified\b", re.I),
    re.compile(
        r"\bformally verified (chain|node|binary|implementation|protocol)\b",
        re.I,
    ),
)

NEG = re.compile(
    r"not |no |false|never|forbid|do not|don't|must not|not claimed|"
    r"does not|without|unproved|not a claim|not mean",
    re.I,
)

ALLOWED_MODEL = re.compile(
    r"\b(protocol model|kernel model|coq consensus kernel model)\b",
    re.I,
)


def publication_files() -> list[Path]:
    found: set[Path] = set()
    for pattern in SCAN_GLOBS:
        found.update(ROOT.glob(pattern))
    return sorted(p for p in found if p.is_file())


def main() -> int:
    errors: list[str] = []

    for path in REQUIRED_FILES:
        if not path.is_file():
            errors.append(f"missing {path.relative_to(ROOT)}")
            continue
        text = path.read_text(encoding="utf-8")
        rel = str(path.relative_to(ROOT))
        for flag in FLAGS:
            if flag not in text:
                errors.append(f"{rel}: missing `{flag}`")

    for path in publication_files():
        rel = str(path.relative_to(ROOT)).replace("\\", "/")
        try:
            lines = path.read_text(encoding="utf-8").splitlines()
        except OSError:
            continue
        prev = ""
        for i, line in enumerate(lines, 1):
            window = f"{prev} {line}"
            if NEG.search(window) or ALLOWED_MODEL.search(window):
                prev = line
                continue
            for pat in FORBIDDEN:
                if pat.search(line):
                    errors.append(f"{rel}:{i}: forbidden slogan: {line.strip()[:120]}")
            prev = line

    if errors:
        print("PUBLICATION_BOUNDARY_FAILED")
        for e in errors[:40]:
            print(f"  - {e}")
        if len(errors) > 40:
            print(f"  … {len(errors) - 40} more")
        return 1

    print("PUBLICATION_BOUNDARY_CHECK=PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())

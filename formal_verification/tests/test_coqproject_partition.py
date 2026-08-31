#!/usr/bin/env python3
"""A2: protocol-core cone must stay a declared subset of the full _CoqProject.

A new .v in _CoqProject that is neither protocol-core nor a Phase catalog
(or the re-export DrawBridgeProgress.v) fails this test, so F-8 cannot
silently miss a load-bearing module.
"""

from __future__ import annotations

import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
COQ = REPO / "formal_verification" / "coq"
FULL = COQ / "_CoqProject"
CORE = COQ / "_CoqProject.protocol-core"

CATALOG_PREFIXES = (
    "Phase4FConsensus/",
    "Phase4GRefinement/",
    "Phase5Equivalence/",
)
CATALOG_EXACT = frozenset({"DrawBridgeProgress.v"})


def project_v_entries(path: Path) -> list[str]:
    out: list[str] = []
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line or line.startswith("#") or line.startswith("-"):
            continue
        if line.endswith(".v"):
            out.append(line)
    return out


def is_catalog(rel: str) -> bool:
    return rel in CATALOG_EXACT or any(rel.startswith(p) for p in CATALOG_PREFIXES)


def main() -> int:
    errors: list[str] = []
    if not FULL.is_file():
        print(f"FAIL: missing {FULL}")
        return 1
    if not CORE.is_file():
        print(f"FAIL: missing {CORE}")
        return 1

    full = project_v_entries(FULL)
    core = project_v_entries(CORE)
    full_set = set(full)
    core_set = set(core)

    if len(core) != len(core_set):
        errors.append("protocol-core lists a module more than once")
    if len(full) != len(full_set):
        errors.append("_CoqProject lists a module more than once")

    for rel in core:
        disk = COQ / rel
        if not disk.is_file():
            errors.append(f"protocol-core entry missing on disk: {rel}")
        if rel not in full_set:
            errors.append(f"protocol-core entry not in _CoqProject: {rel}")
        if is_catalog(rel):
            errors.append(f"protocol-core must not contain catalog module: {rel}")

    full_core_order = [rel for rel in full if rel in core_set]
    if full_core_order != core:
        errors.append(
            "protocol-core order does not match _CoqProject relative order "
            f"(core has {len(core)} modules, filtered full has {len(full_core_order)})"
        )

    unclassified = [rel for rel in full if rel not in core_set and not is_catalog(rel)]
    for rel in unclassified:
        errors.append(
            f"_CoqProject module is neither protocol-core nor a Phase catalog: {rel}"
        )

    if errors:
        print("FAIL: Coq project partition (A2)")
        for e in errors:
            print(f"  - {e}")
        return 1

    catalog = [rel for rel in full if is_catalog(rel)]
    print(
        f"PASS: Coq project partition "
        f"(protocol-core {len(core)}, catalog {len(catalog)}, full {len(full)})"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Fail if FORMAL_THEOREM_INVENTORY.json is behind the Coq sources."""

from __future__ import annotations

import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "formal_verification" / "tools"))
from gen_theorem_inventory import DEFAULT_OUT, build_inventory  # noqa: E402


def main() -> int:
    live = build_inventory()
    if not DEFAULT_OUT.is_file():
        print(f"FAIL: missing {DEFAULT_OUT.relative_to(ROOT)}")
        return 1
    committed = json.loads(DEFAULT_OUT.read_text(encoding="utf-8"))
    if committed.get("count") != live["count"]:
        print(
            f"FAIL: inventory count {committed.get('count')} != live {live['count']}; "
            "run python3 formal_verification/tools/gen_theorem_inventory.py"
        )
        return 1
    live_names = [(t["file"], t["name"]) for t in live["theorems"]]
    committed_names = [(t["file"], t["name"]) for t in committed.get("theorems", [])]
    if live_names != committed_names:
        print("FAIL: inventory names drifted; regenerate FORMAL_THEOREM_INVENTORY.json")
        return 1
    print(f"THEOREM_INVENTORY=CURRENT count={live['count']}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""encode(decode(bytes)) == original for legal TASK-005 vectors."""
from __future__ import annotations

import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

from codec import try_decode_block, try_decode_tx  # noqa: E402


def main() -> int:
    cases_dir = HERE / "cases"
    if not cases_dir.is_dir():
        print("missing cases/; run generate_corpus.py")
        return 2
    failed = []
    checked = 0
    for path in sorted(cases_dir.glob("*.json")):
        row = json.loads(path.read_text())
        raw = row.get("raw_block_hex") or row.get("raw_tx_hex")
        kind = row["kind"]
        if kind == "block":
            _, err, leftover, equal = try_decode_block(raw)
        else:
            _, err, leftover, equal = try_decode_tx(raw)
        if err:
            continue
        if leftover:
            continue
        checked += 1
        if equal is not True:
            failed.append(row["case_id"])
    print(f"legal_roundtrip_checked={checked} fail={failed}")
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())

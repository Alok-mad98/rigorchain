#!/usr/bin/env python3
"""Pack consensus traces for generator replay. Not a consensus change."""
from __future__ import annotations

import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
LIVE = ROOT / "tests" / "attack_lab" / "data_task008" / "mine" / "regtest" / "consensus_trace"
DATASET = ROOT / "audit" / "formal_bridge" / "kernel_candidate_dataset"
OUT = DATASET / "traces.jsonl"

KEEP = (
    "block_hash",
    "height",
    "header_result",
    "pow_result",
    "asert_result",
    "tx_results",
    "block_result",
    "block_reason",
    "rejection_layer",
    "case_id",
)


def slim(obj: dict, source: str) -> dict:
    row = {k: obj.get(k) for k in KEEP}
    row["source"] = source
    return row


def main() -> int:
    rows = []
    seen = set()

    def add(obj: dict, source: str) -> None:
        s = slim(obj, source)
        key = json.dumps(s, sort_keys=True, separators=(",", ":"))
        if key in seen:
            return
        seen.add(key)
        rows.append(s)

    if LIVE.is_dir():
        for p in sorted(LIVE.glob("*-consensus_trace.json")):
            add(json.loads(p.read_text()), "regtest_mining")
    for p in sorted(DATASET.rglob("consensus_trace.json")):
        add(json.loads(p.read_text()), str(p.parent.relative_to(DATASET)))
    if len(rows) < 2000:
        print(f"only {len(rows)} unique traces; need live harvest or data_task008", file=sys.stderr)
    OUT.write_text("".join(json.dumps(r, separators=(",", ":")) + "\n" for r in rows))
    print(f"wrote {OUT} rows {len(rows)}")
    return 0 if len(rows) >= 2000 else 1


if __name__ == "__main__":
    raise SystemExit(main())

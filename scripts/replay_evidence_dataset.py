#!/usr/bin/env python3
"""Replay saved evidence JSON → same normalized record and evidence_id."""
from __future__ import annotations

import argparse
import importlib.util
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
API = ROOT / "tools" / "formal_bridge" / "evidence_collector.py"
DEFAULT = ROOT / "audit" / "formal_bridge" / "evidence_v1" / "fixtures" / "replay_block.json"


def load_col():
    spec = importlib.util.spec_from_file_location("evidence_collector", API)
    mod = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(mod)
    return mod


def write_default_fixture(col, path: Path) -> dict:
    rec = col.create_block_evidence(
        block_hash="aa" * 32,
        height=1,
        prev_hash="00" * 32,
        nBits="207fffff",
        tx_count=1,
        accept=True,
        source="test_harness",
        collected_at="2026-09-07T00:00:00Z",
        scenario="replay",
    )
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(rec, indent=2, sort_keys=True) + "\n")
    return rec


def replay_path(path: Path) -> dict:
    col = load_col()
    if not path.is_file():
        write_default_fixture(col, path)
    saved = json.loads(path.read_text())
    replayed = col.replay_saved_evidence(saved)
    return {"saved": saved, "replayed": replayed}


def main() -> int:
    p = argparse.ArgumentParser(description="Replay saved evidence JSON")
    p.add_argument("paths", nargs="*", type=Path, default=[DEFAULT])
    args = p.parse_args()
    col = load_col()
    failed = 0
    for path in args.paths:
        if not path.is_file():
            write_default_fixture(col, path)
        saved = json.loads(path.read_text())
        replayed = col.replay_saved_evidence(saved)
        same_id = saved.get("evidence_id") == replayed.get("evidence_id")
        errs = col.validate_evidence_record(replayed)
        status = "PASS" if same_id and not errs else "FAIL"
        print(f"{path}: {status} evidence_id={replayed.get('evidence_id')}")
        if not same_id:
            print(" - evidence_id mismatch")
            failed += 1
        for e in errs:
            print(" -", e)
            failed += 1
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())

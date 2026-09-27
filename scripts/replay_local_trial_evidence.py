#!/usr/bin/env python3
"""Replay saved local trial evidence. Same record / same evidence_id."""
from __future__ import annotations

import argparse
import importlib.util
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
API = ROOT / "tools" / "formal_bridge" / "evidence_collector.py"
DEFAULT = ROOT / "audit" / "formal_bridge" / "local_trial_dataset"


def load_col():
    spec = importlib.util.spec_from_file_location("evidence_collector", API)
    mod = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(mod)
    return mod


def replay_dataset(dataset: Path) -> int:
    col = load_col()
    if not dataset.is_dir():
        print(f"missing {dataset}")
        return 1
    failed = 0
    files = sorted(p for p in dataset.glob("*.json") if p.name != "index.json")
    if not files:
        print("no evidence JSON in dataset")
        return 1
    for path in files:
        saved = json.loads(path.read_text())
        if "evidence_id" not in saved:
            print(f"{path.name}: skip (not an evidence record)")
            continue
        replayed = col.replay_saved_evidence(saved)
        same = saved.get("evidence_id") == replayed.get("evidence_id")
        errs = col.validate_evidence_record(replayed)
        if same and not errs:
            print(f"{path.name}: PASS {saved['evidence_id'][:16]}")
            continue
        print(f"{path.name}: FAIL")
        failed += 1
        if not same:
            print(" - evidence_id mismatch")
        for e in errs:
            print(" -", e)
    return 1 if failed else 0


def main() -> int:
    p = argparse.ArgumentParser(description="Replay local trial evidence JSON")
    p.add_argument("dataset", nargs="?", type=Path, default=DEFAULT)
    args = p.parse_args()
    rc = replay_dataset(args.dataset)
    print(f"REPLAY_STATUS = {'PASS' if rc == 0 else 'FAIL'}")
    return rc


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Replay gate for local trial dataset. Does not start a node."""
from __future__ import annotations

import importlib.util
import json
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DATASET = ROOT / "audit" / "formal_bridge" / "local_trial_dataset"
REPLAY = ROOT / "scripts" / "replay_local_trial_evidence.py"
COLLECT = ROOT / "tools" / "formal_bridge" / "evidence_collector.py"


def load(path: Path, name: str):
    spec = importlib.util.spec_from_file_location(name, path)
    mod = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(mod)
    return mod


class LocalTrialReplayTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        if not DATASET.is_dir():
            raise unittest.SkipTest("run scripts/run_local_evidence_trial.py first")
        cls.col = load(COLLECT, "evidence_collector")
        cls.files = [p for p in sorted(DATASET.glob("*.json")) if p.name != "index.json"]
        if not cls.files:
            raise unittest.SkipTest("empty local_trial_dataset")

    def test_replay_same_id(self) -> None:
        rp = load(REPLAY, "replay_local_trial_evidence")
        self.assertEqual(rp.replay_dataset(DATASET), 0)

    def test_each_file_roundtrip(self) -> None:
        for path in self.files:
            saved = json.loads(path.read_text())
            if "evidence_id" not in saved:
                continue
            replayed = self.col.replay_saved_evidence(saved)
            self.assertEqual(saved["evidence_id"], replayed["evidence_id"], path.name)
            self.assertEqual(self.col.validate_evidence_record(replayed), [], path.name)

    def test_no_network_in_class_a(self) -> None:
        for path in self.files:
            if not path.name.startswith("A_"):
                continue
            rec = json.loads(path.read_text())
            self.assertNotIn("network_context", rec, path.name)
            self.assertNotIn("client_action", rec, path.name)
            self.assertEqual(self.col.isolation_errors(rec), [], path.name)


if __name__ == "__main__":
    unittest.main()

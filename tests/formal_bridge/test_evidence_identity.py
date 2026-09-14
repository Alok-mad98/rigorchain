#!/usr/bin/env python3
"""Identity: same trace+scenario+payload ⇒ same evidence_id. timestamp not hashed."""
from __future__ import annotations

import importlib.util
import json
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
COLLECT = ROOT / "tools" / "formal_bridge" / "evidence_collector.py"
SAMPLE = (
    ROOT
    / "audit"
    / "formal_bridge"
    / "kernel_candidate_dataset"
    / "live_connectblock"
    / "h1-dc73e2e40d8ae06b-accept"
    / "consensus_trace.json"
)
TRACES = ROOT / "audit" / "formal_bridge" / "kernel_candidate_dataset" / "traces.jsonl"


def load_col():
    spec = importlib.util.spec_from_file_location("evidence_collector", COLLECT)
    mod = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(mod)
    return mod


def sample_trace() -> dict:
    if SAMPLE.is_file():
        return json.loads(SAMPLE.read_text())
    if TRACES.is_file():
        return json.loads(TRACES.read_text().splitlines()[0])
    raise unittest.SkipTest("no consensus_trace sample")


class EvidenceIdentityTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.col = load_col()

    def test_timestamp_not_in_hash(self) -> None:
        kw = dict(
            block_hash="aa",
            height=1,
            prev_hash="00",
            nBits="207fffff",
            tx_count=1,
            accept=True,
            source="test_harness",
            scenario="identity",
        )
        a = self.col.create_block_evidence(**kw, collected_at="2020-01-01T00:00:00Z")
        b = self.col.create_block_evidence(**kw, collected_at="2030-01-01T00:00:00Z")
        self.assertEqual(a["evidence_id"], b["evidence_id"])
        self.assertNotEqual(a["timestamp"], b["timestamp"])

    def test_same_trace_scenario_same_id(self) -> None:
        tr = sample_trace()
        a = self.col.create_block_evidence_from_trace(tr, collected_at="t1", scenario="ce-identity")
        b = self.col.create_block_evidence_from_trace(tr, collected_at="t2", scenario="ce-identity")
        self.assertEqual(a["evidence_id"], b["evidence_id"])
        self.assertEqual(self.col.validate_evidence_record(a), [])

    def test_different_scenario_different_id(self) -> None:
        tr = sample_trace()
        a = self.col.create_block_evidence_from_trace(tr, scenario="invalid_block")
        b = self.col.create_block_evidence_from_trace(tr, scenario="invalid_tx")
        self.assertNotEqual(a["evidence_id"], b["evidence_id"])

    def test_different_event_different_id(self) -> None:
        a = self.col.create_block_evidence(
            block_hash="aa",
            height=1,
            prev_hash="00",
            nBits=1,
            tx_count=1,
            accept=True,
            source="rpc",
        )
        b = self.col.create_block_evidence(
            block_hash="bb",
            height=2,
            prev_hash="aa",
            nBits=1,
            tx_count=1,
            accept=False,
            rejection_layer="CheckProofOfWork",
            source="rpc",
        )
        self.assertNotEqual(a["evidence_id"], b["evidence_id"])


if __name__ == "__main__":
    unittest.main()

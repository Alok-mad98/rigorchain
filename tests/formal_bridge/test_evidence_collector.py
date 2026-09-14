#!/usr/bin/env python3
"""same trace = same evidence_id. No consensus change. No node required."""
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
ST = ROOT / "audit" / "formal_bridge" / "state_transition_dataset" / "ST-001_accept_connect.json"


def load_col():
    spec = importlib.util.spec_from_file_location("evidence_collector", COLLECT)
    mod = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(mod)
    return mod


class EvidenceCollectorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.col = load_col()

    def test_same_args_same_id(self) -> None:
        kw = dict(
            block_hash="aa",
            height=1,
            prev_hash="00",
            nBits="207fffff",
            header_time=10,
            tx_count=1,
            accept=True,
            source="test_harness",
        )
        a = self.col.create_block_evidence(**kw, collected_at="2020-01-01T00:00:00Z")
        b = self.col.create_block_evidence(**kw, collected_at="2021-01-01T00:00:00Z")
        self.assertEqual(a["evidence_id"], b["evidence_id"])
        self.assertEqual(self.col.validate_evidence_record(a), [])
        self.assertEqual(self.col.validate_evidence_record(b), [])

    def test_same_trace_same_evidence(self) -> None:
        if SAMPLE.is_file():
            trace = json.loads(SAMPLE.read_text())
        elif TRACES.is_file():
            trace = json.loads(TRACES.read_text().splitlines()[0])
        else:
            self.skipTest("no consensus_trace sample")
        a = self.col.create_block_evidence_from_trace(trace, collected_at="t1")
        b = self.col.create_block_evidence_from_trace(trace, collected_at="t2")
        self.assertEqual(a["evidence_id"], b["evidence_id"], "same trace must yield same evidence_id")
        self.assertEqual(a["source"], "consensus_trace")
        self.assertEqual(a["schema_version"], "v1")
        self.assertEqual(self.col.validate_evidence_record(a), [])

    def test_source_changes_id(self) -> None:
        kw = dict(
            block_hash="aa",
            height=1,
            prev_hash="00",
            nBits=1,
            header_time=1,
            tx_count=1,
            accept=True,
        )
        a = self.col.create_block_evidence(**kw, source="rpc")
        b = self.col.create_block_evidence(**kw, source="test_harness")
        self.assertNotEqual(a["evidence_id"], b["evidence_id"])

    def test_state_and_network_validate(self) -> None:
        st = self.col.create_state_transition_evidence(
            before_utxo_hash="b1",
            operation="connect",
            after_utxo_hash="b2",
            undo_hash="u",
            source="rpc",
        )
        env = self.col.create_network_observation(peer_count=2, orphan=False, source="node_observer")
        self.assertEqual(self.col.validate_evidence_record(st), [])
        self.assertEqual(self.col.validate_evidence_record(env), [])
        self.assertEqual(st["future_theorem"], "connect_disconnect_inverse")
        self.assertEqual(env["future_theorem"], "network assumption")

    def test_normalize_state_aliases(self) -> None:
        if not ST.is_file():
            self.skipTest("no ST-001")
        rec = self.col.normalize_evidence(json.loads(ST.read_text()))
        self.assertEqual(rec["operation"], "connect")
        self.assertEqual(rec["class"], "state_transition")
        self.assertTrue(rec["before_utxo_hash"])
        self.assertTrue(rec["after_utxo_hash"])
        self.assertEqual(self.col.validate_evidence_record(rec), [])

    def test_no_env_in_candidate(self) -> None:
        leaked = self.col.candidate_pollution({"ki_pow_ok": True, "peer_count": 3})
        self.assertTrue(any("peer_count" in e for e in leaked))
        clean = self.col.candidate_pollution({"ki_pow_ok": True, "ki_utxo_ok": True})
        self.assertEqual(clean, [])

    def test_class_a_rejects_env_field(self) -> None:
        rec = self.col.create_block_evidence(
            block_hash="aa",
            height=1,
            prev_hash="00",
            nBits=1,
            header_time=1,
            tx_count=1,
            accept=True,
            source="rpc",
        )
        rec["peer_count"] = 4
        rec["evidence_id"] = self.col.compute_evidence_id(rec)
        errs = self.col.validate_evidence_record(rec)
        self.assertTrue(any("peer_count" in e or "environment" in e or "must not carry" in e for e in errs))

    def test_reject_needs_layer(self) -> None:
        rec = self.col.create_block_evidence(
            block_hash="aa",
            height=1,
            prev_hash="00",
            nBits=0,
            header_time=1,
            tx_count=1,
            accept=False,
            rejection_layer="CheckProofOfWork",
            source="test_harness",
        )
        self.assertEqual(self.col.validate_evidence_record(rec), [])
        rec["rejection_layer"] = None
        rec["evidence_id"] = self.col.compute_evidence_id(rec)
        self.assertTrue(any("rejection_layer" in e for e in self.col.validate_evidence_record(rec)))

    def test_forbidden_source(self) -> None:
        rec = self.col.create_block_evidence(
            block_hash="aa",
            height=1,
            prev_hash="00",
            nBits=1,
            header_time=1,
            tx_count=1,
            accept=True,
            source="test_harness",
        )
        rec["source"] = "consensus_patch"
        rec["evidence_id"] = self.col.compute_evidence_id(rec)
        self.assertTrue(any("source" in e for e in self.col.validate_evidence_record(rec)))


if __name__ == "__main__":
    unittest.main()

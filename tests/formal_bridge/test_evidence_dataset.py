#!/usr/bin/env python3
"""Dataset integrity: mapping, isolation, replay. No consensus change."""
from __future__ import annotations

import importlib.util
import json
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
COLLECT = ROOT / "tools" / "formal_bridge" / "evidence_collector.py"
MAP = ROOT / "audit" / "formal_bridge" / "evidence_theorem_mapping_v1.json"
MATRIX = ROOT / "audit" / "formal_bridge" / "evidence_coverage_matrix.md"
FIX = ROOT / "audit" / "formal_bridge" / "evidence_v1" / "fixtures"


def load_col():
    spec = importlib.util.spec_from_file_location("evidence_collector", COLLECT)
    mod = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(mod)
    return mod


class EvidenceDatasetTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.col = load_col()
        cls.mapping = json.loads(MAP.read_text())

    def test_mapping_covers_required(self) -> None:
        for key in (
            "invalid_block",
            "invalid_tx",
            "bad_pow",
            "bad_asert",
            "connect",
            "disconnect",
            "restart",
            "reorg",
            "decode",
            "encode",
            "malformed_input",
            "connect_disconnect",
        ):
            self.assertIn(key, self.mapping)
            row = self.mapping[key]
            self.assertIn(row["evidence_class"], {"A", "B", "C", "A+B", "B+C"})
            self.assertTrue(row["future_theorem"])

    def test_coverage_matrix_rows(self) -> None:
        text = MATRIX.read_text()
        for needle in (
            "double spend",
            "bad nBits",
            "restart",
            "reorg",
            "network partition",
            "input_sound",
            "pow_soundness",
            "persistence_correctness",
            "chain_selection",
            "network assumption",
        ):
            self.assertIn(needle, text)

    def test_abc_isolation(self) -> None:
        a = self.col.create_block_evidence(
            block_hash="aa",
            height=1,
            prev_hash="00",
            nBits=1,
            tx_count=1,
            accept=True,
            source="test_harness",
        )
        b = self.col.create_state_transition_evidence(
            before_utxo_hash="s0",
            operation="connect",
            after_utxo_hash="s1",
            source="rpc",
        )
        c = self.col.create_network_observation(peer_count=3, latency=5, source="node_observer")
        self.assertEqual(self.col.isolation_errors(a), [])
        self.assertEqual(self.col.isolation_errors(b), [])
        self.assertIn("network_context", c)
        self.assertNotIn("network_context", a)
        self.assertNotIn("network_context", b)

    def test_kernelinput_pollution_none(self) -> None:
        self.assertEqual(self.col.candidate_pollution({"ki_pow_ok": True, "ki_utxo_ok": False}), [])
        self.assertTrue(self.col.candidate_pollution({"ki_pow_ok": True, "network_context": {}}))

    def test_replay_fixture(self) -> None:
        path = FIX / "replay_block.json"
        if not path.is_file():
            self.skipTest("fixture not written yet")
        saved = json.loads(path.read_text())
        replayed = self.col.replay_saved_evidence(saved)
        self.assertEqual(saved["evidence_id"], replayed["evidence_id"])
        self.assertEqual(self.col.validate_evidence_record(replayed), [])


if __name__ == "__main__":
    unittest.main()

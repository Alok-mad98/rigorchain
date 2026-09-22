"""Stratum V1 rgrpow01 handshake, job, and share hash."""
from __future__ import annotations

import json
import unittest

from rgrpow_lib import opuhash, vector_by_id


def requires_rgrpow01_agent(agent: str) -> bool:
    return "rgrpow01" in agent.lower()


def requires_opushash_agent(agent: str) -> bool:
    return "opushash" in agent.lower()


class StratumCompatibilityTests(unittest.TestCase):
    def test_subscribe_requires_rgrpow01(self):
        line = '{"id":1,"method":"mining.subscribe","params":["rigor-miner/1.0-rgrpow01",null]}'
        req = json.loads(line)
        self.assertEqual(req["method"], "mining.subscribe")
        agent = req["params"][0]
        self.assertTrue(requires_rgrpow01_agent(agent))
        self.assertFalse(requires_rgrpow01_agent("legacy-miner"))
        self.assertFalse(requires_rgrpow01_agent("rigor-miner/1.0-opushash"))
        self.assertTrue(requires_opushash_agent("rigor-miner/1.0-opushash"))

    def test_notify_field_order(self):
        params = [
            "job1",
            "11" * 32,
            "coinb1",
            "coinb2",
            [],
            "20000000",
            "1f00ffff",
            "6553f100",
            True,
        ]
        keys = [
            "job_id",
            "prevhash",
            "coinb1",
            "coinb2",
            "merkle_branch",
            "version",
            "nbits",
            "ntime",
            "clean_jobs",
        ]
        job = dict(zip(keys, params))
        self.assertEqual(job["job_id"], "job1")
        self.assertEqual(len(job["prevhash"]), 64)
        self.assertEqual(job["nbits"], "1f00ffff")

    def test_submit_includes_extranonce2_ntime_nonce(self):
        params = ["alice.rig1", "job1", "00000001", "6553f100", "00015792"]
        worker, job_id, extranonce2, ntime, nonce = params
        self.assertEqual(worker, "alice.rig1")
        self.assertEqual(len(extranonce2), 8)
        self.assertEqual(int(nonce, 16), 87954)
        self.assertEqual(int(ntime, 16), 1700000000)

    def test_share_uses_same_rgrpow01_hash(self):
        v = vector_by_id("003")
        header = bytes.fromhex(v["header_hex"])
        self.assertEqual(opuhash(header).hex(), v["digest_hex"])


if __name__ == "__main__":
    unittest.main()

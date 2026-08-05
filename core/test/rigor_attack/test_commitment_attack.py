#!/usr/bin/env python3
"""Category D: commitment attacks — wrong winner or wrong nBits."""

import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "rigor"))
from rigor_model import RigorCommitment, convergenceCandidate, compute_winner, validate_invalid_block_cases

from attack_sim import assert_attack, verdict


class TestCommitmentAttack(unittest.TestCase):
    """Adversary lies in coinbase commitment about winner or parent target."""

    def setUp(self):
        self.seed = bytes.fromhex("c0" * 32)
        self.height = 900
        self.target_prev = 0x1D00FFFF
        self.a = convergenceCandidate(bytes.fromhex("c1" * 32))
        self.b = convergenceCandidate(bytes.fromhex("c2" * 32))

    def test_wrong_winner_rejected(self):
        winner = compute_winner([self.a, self.b], self.seed, self.height)
        self.assertIsNotNone(winner)
        wrong = bytes.fromhex("d0" * 32)
        c = RigorCommitment(winner_txid=wrong, nbits=self.target_prev)
        ok = not validate_invalid_block_cases(c, winner, self.target_prev, [winner])
        assert_attack(ok, "wrong winner commitment still validates with correct RES")
        self.assertEqual(verdict(ok, "winner"), "PASS")

    def test_wrong_nbits_rejected(self):
        winner = compute_winner([self.a, self.b], self.seed, self.height)
        self.assertIsNotNone(winner)
        c = RigorCommitment(winner_txid=winner, nbits=0x207FFFFF)
        ok = not validate_invalid_block_cases(c, winner, self.target_prev, [winner])
        assert_attack(ok, "wrong target nBits in commitment still validates")
        self.assertEqual(verdict(ok, "nbits"), "PASS")


if __name__ == "__main__":
    unittest.main()

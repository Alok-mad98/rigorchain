#!/usr/bin/env python3
"""Category C: RES front-run — claim before winner is knowable on-chain."""

import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "rigor"))
from rigor_model import RigorCommitment, convergenceCandidate, compute_winner, validate_invalid_block_cases

from attack_sim import assert_attack, verdict


class TestResFrontRun(unittest.TestCase):
    """Adversary pins RES early to a guess; commitment and the revealed seed must reject the mismatch."""

    def setUp(self):
        self.seed = bytes.fromhex("fa" * 32)
        self.height = 800
        self.target_prev = 0x1D00FFFF
        self.a = convergenceCandidate(bytes.fromhex("0a" * 32))
        self.b = convergenceCandidate(bytes.fromhex("0b" * 32))

    def test_front_run_guess_fails_validation(self):
        """Winner from the true seed; attacker RES guesses another txid -> invalid block."""
        winner = compute_winner([self.a, self.b], self.seed, self.height)
        self.assertIsNotNone(winner)
        guess = self.a.txid if winner != self.a.txid else self.b.txid
        commitment = RigorCommitment(winner_txid=winner, nbits=self.target_prev)
        ok = not validate_invalid_block_cases(commitment, winner, self.target_prev, [guess])
        assert_attack(
            ok,
            "front-run RES: wrong txid still validates against committed winner",
        )
        self.assertEqual(verdict(ok, "front"), "PASS")

    def test_only_post_commit_winner_res_pair_valid(self):
        """Once commitment matches computed winner, RES mint must be that same txid."""
        winner = compute_winner([self.a, self.b], self.seed, self.height)
        self.assertIsNotNone(winner)
        ok = validate_invalid_block_cases(
            RigorCommitment(winner_txid=winner, nbits=self.target_prev),
            winner,
            self.target_prev,
            [winner],
        )
        assert_attack(ok, "valid winner+RES mint pair rejected after seed and candidate knowledge")
        self.assertEqual(verdict(ok, "pair"), "PASS")


if __name__ == "__main__":
    unittest.main()

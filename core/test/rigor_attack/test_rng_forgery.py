#!/usr/bin/env python3
"""Category C: fake RES mint / mint txid != draw winner."""

import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "rigor"))
from rigor_model import convergenceCandidate, compute_winner, validate_res_mint_binding

from attack_sim import assert_attack, verdict


class TestResMintForgery(unittest.TestCase):
    """Adversary places RES body mint on arbitrary txid != committed winner."""

    def setUp(self):
        self.seed = bytes.fromhex("de" * 32)
        self.height = 700
        self.target_prev = 0x1D00FFFF
        self.convergence_a = convergenceCandidate(bytes.fromhex("05" * 32))
        self.convergence_b = convergenceCandidate(bytes.fromhex("06" * 32))
        self.fake_res_mint_txid = bytes.fromhex("99" * 32)

    def test_res_mint_must_equal_winner(self):
        winner = compute_winner([self.convergence_a, self.convergence_b], self.seed, self.height)
        self.assertIsNotNone(winner)
        ok = not validate_res_mint_binding([self.fake_res_mint_txid], winner)
        assert_attack(ok, "convergenced RES mint accepted when mint txid != committed winner")
        self.assertEqual(verdict(ok, "reject"), "PASS")

    def test_correct_binding_passes(self):
        winner = compute_winner([self.convergence_a, self.convergence_b], self.seed, self.height)
        self.assertIsNotNone(winner)
        ok = validate_res_mint_binding([winner], winner)
        assert_attack(ok, "honest RES mint binding rejected")
        self.assertEqual(verdict(ok, "accept"), "PASS")


if __name__ == "__main__":
    unittest.main()

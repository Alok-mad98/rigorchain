#!/usr/bin/env python3
"""Category D (partial): what pins σ to real work, after SECURITY-001-A8.

This file used to assert that ``target_prev`` was mixed into the digest, on the theory that
folding the parent's compact nBits in forced the adversary to know the parent's real work.
Sigma-PoW-01 replaced that with something strictly stronger: the digest now takes the parent
block hash itself, which commits to the parent's nBits and to everything else in its header.
``target_prev`` was dropped from the preimage as redundant.

So the assertions invert. σ must NOT move when only an assumed ``target_prev`` changes — there
is no such input any more — and it MUST move when the seed does, which on a real chain costs a
full proof-of-work solution per re-roll.
"""

import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "rigor"))
from rigor_model import convergenceCandidate, compute_convergence_hash, compute_convergence_root_bytes, compute_winner

from attack_sim import assert_attack, verdict


class TestTargetManipulation(unittest.TestCase):
    def setUp(self):
        self.seed = bytes.fromhex("ee" * 32)
        self.height = 600
        self.a = convergenceCandidate(bytes.fromhex("03" * 32))
        self.b = convergenceCandidate(bytes.fromhex("04" * 32))

    def test_sigma_seed_is_the_parent_block_hash(self):
        """Changing the seed by one bit must reshuffle σ — one re-roll, one proof of work."""
        fr = compute_convergence_root_bytes([self.a, self.b])
        h1 = compute_convergence_hash(self.a.txid, self.seed, self.height, fr)
        h2 = compute_convergence_hash(self.a.txid, bytes.fromhex("ef" + "ee" * 31), self.height, fr)
        ok = h1 != h2
        assert_attack(
            ok,
            "parent block hash not mixed into σ: attacker can grind the winner without parent work",
        )
        self.assertEqual(verdict(ok, "mix"), "PASS")

    def test_winner_is_fixed_once_the_parent_is_fixed(self):
        """With the seed, the set and the height fixed, nothing the next producer does moves σ."""
        fr = compute_convergence_root_bytes([self.a, self.b])
        w1 = compute_winner([self.a, self.b], self.seed, self.height)
        w2 = compute_winner([self.a, self.b], self.seed, self.height)
        ok = w1 is not None and w1 == w2
        ok = ok and (
            compute_convergence_hash(self.a.txid, self.seed, self.height, fr)
            == compute_convergence_hash(self.a.txid, self.seed, self.height, fr)
        )
        # A different payout height is a different draw, which is what stops replay across forks.
        ok = ok and (
            compute_convergence_hash(self.a.txid, self.seed, self.height, fr)
            != compute_convergence_hash(self.a.txid, self.seed, self.height + 1, fr)
        )
        assert_attack(
            ok,
            "σ is not fully determined by the parent: the paying block retains a free variable",
        )
        self.assertEqual(verdict(ok, "winner"), "PASS")


if __name__ == "__main__":
    unittest.main()

#!/usr/bin/env python3
"""Category B: precomputed txids — the adversary picks txids before the seed is known.

SECURITY-001-A8 renamed what the seed is. It used to be `bind_commit`, a hash over an
ancestor ten blocks back and that block's target, which was known long before the draw
resolved. It is now the parent block hash, which does not exist until the parent has
been mined. The property under test is unchanged and the guarantee behind it is
strictly stronger: an adversary who fixes its txids in advance cannot know the seed
those txids will be ranked under, so no precomputed ordering survives.
"""

import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "rigor"))
from rigor_model import convergenceCandidate, compute_convergence_hash, compute_convergence_root_bytes, compute_winner

from attack_sim import assert_attack, verdict


class TestPreimageAttack(unittest.TestCase):
    """Adversary fixes txids before the seed is known; ordering must not stay exploitable."""

    def setUp(self):
        self.height = 500
        self.a = convergenceCandidate(bytes.fromhex("01" * 32))
        self.b = convergenceCandidate(bytes.fromhex("02" * 32))

    def test_seed_changes_all_digests(self):
        """Changing the seed changes every candidate digest (no static precomputed ranking)."""
        seed1 = bytes.fromhex("11" * 32)
        seed2 = bytes.fromhex("22" * 32)
        fr = compute_convergence_root_bytes([self.a, self.b])
        h1a = compute_convergence_hash(self.a.txid, seed1, self.height, fr)
        h1b = compute_convergence_hash(self.b.txid, seed1, self.height, fr)
        h2a = compute_convergence_hash(self.a.txid, seed2, self.height, fr)
        h2b = compute_convergence_hash(self.b.txid, seed2, self.height, fr)
        ok = (h1a, h1b) != (h2a, h2b) and h1a != h2a and h1b != h2b
        assert_attack(
            ok,
            "seed does not re-key convergence draw: precomputed txid ordering could survive",
        )
        self.assertEqual(verdict(ok, "rekey"), "PASS")

    def test_adversary_cannot_precommit_to_one_seed(self):
        """Same txids, different parent: the digests differ, so no seed can be committed to early."""
        w_x = compute_winner([self.a, self.b], bytes.fromhex("aa" * 32), self.height)
        w_y = compute_winner([self.a, self.b], bytes.fromhex("bb" * 32), self.height)
        ok = w_x is not None and w_y is not None
        # The winner need not flip on every pair of seeds, but the digests must differ,
        # which is what makes committing to a single seed in advance unsafe.
        fr = compute_convergence_root_bytes([self.a, self.b])
        hx = compute_convergence_hash(self.a.txid, bytes.fromhex("aa" * 32), self.height, fr)
        hy = compute_convergence_hash(self.a.txid, bytes.fromhex("bb" * 32), self.height, fr)
        ok = ok and hx != hy
        assert_attack(
            ok,
            "precomputed txid strategy: convergence digest independent of the seed (fatal RES seeding failure)",
        )
        self.assertEqual(verdict(ok, "preimage"), "PASS")


if __name__ == "__main__":
    unittest.main()

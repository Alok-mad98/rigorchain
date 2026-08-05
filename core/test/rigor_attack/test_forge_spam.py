#!/usr/bin/env python3
"""Category A: convergence spam / many candidates — adversarial volume."""

import unittest

from attack_sim import COIN, assert_attack, collect_convergence_winner_txid, verdict


class TestconvergenceSpamAttack(unittest.TestCase):
    """Adversary floods the block with convergence transactions to bias or crash selection."""

    def setUp(self):
        self.seed = bytes.fromhex("cd" * 32)
        self.height = 400
        self.target_prev = 0x1D00FFFF
        self.T = bytes.fromhex("ff" * 32)

    def test_many_convergence_txs_deterministic_winner(self):
        """Many eligible convergence transactions: winner is unique argmin H (no undefined state)."""
        txs = []
        for i in range(80):
            tid = i.to_bytes(4, "little") + bytes(28)
            # convergence_amount must dominate COIN vs worst-case h≈T so inequality stays satisfiable.
            txs.append((tid, 2 * COIN + i * 1000))
        w = collect_convergence_winner_txid(txs, self.seed, self.height, self.T)
        w2 = collect_convergence_winner_txid(txs, self.seed, self.height, self.T)
        ok = w is not None and w == w2 and len(w) == 32
        assert_attack(ok, "spam convergence set yields non-deterministic or missing winner")
        self.assertEqual(verdict(ok, "determinism"), "PASS")

    def test_spam_below_min_all_ignored(self):
        """Spam txs under 1 ORU minimum never produce a winner."""
        txs = [(i.to_bytes(4, "little") + bytes(28), COIN // 2) for i in range(100)]
        w = collect_convergence_winner_txid(txs, self.seed, self.height, self.T)
        ok = w is None
        assert_attack(ok, "spam below min_convergence could still trigger winner selection")
        self.assertEqual(verdict(ok, "ignore"), "PASS")


if __name__ == "__main__":
    unittest.main()

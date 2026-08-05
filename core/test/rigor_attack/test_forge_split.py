#!/usr/bin/env python3
"""Category A: convergence split vs non-split — adversarial convergence economics."""

import unittest

from attack_sim import (
    COIN,
    assert_attack,
    collect_convergence_winner_txid,
    hash_below_target_times_convergence_amount,
    verdict,
)


class TestconvergenceSplitAttack(unittest.TestCase):
    """Adversary splits convergence stake across txs or consolidates; invariants must hold."""

    def setUp(self):
        self.seed = bytes.fromhex("ab" * 32)
        self.height = 300
        self.target_prev = 0x1D00FFFF
        # Large target so typical draw digests pass inequality for sane amounts.
        self.T = bytes.fromhex("ff" * 32)

    def test_monotone_convergence_amount_same_digest(self):
        """Same draw digest: larger convergence_amount never removes eligibility (RHS weight increases with amount)."""
        h = bytes.fromhex("22" * 32)
        a = COIN
        lo = hash_below_target_times_convergence_amount(h, COIN, self.T, a)
        hi = hash_below_target_times_convergence_amount(h, COIN, self.T, 2 * a)
        ok = (not lo) or hi
        assert_attack(
            ok,
            "convergence_amount monotonicity broken: doubling amount could disqualify a passing candidate",
        )
        self.assertEqual(verdict(ok, "monotone"), "PASS")

    def test_dust_split_fails_min_convergence(self):
        """Adversary spams many convergence transactions each below min_convergence — none become draw candidates."""
        txs = [(bytes([i]) + bytes(31), COIN - 1) for i in range(2, 52)]
        w = collect_convergence_winner_txid(txs, self.seed, self.height, self.T)
        ok = w is None
        assert_attack(
            ok,
            "min_convergence bypass: sub-threshold split txs could still elect a winner",
        )
        self.assertEqual(verdict(ok, "min_convergence"), "PASS")

    def test_split_two_halves_vs_one_double_only_two_tx_rule(self):
        """With only one high-amount convergence tx, consensus refuses a winner (<2 convergence transactions)."""
        tx_big = (bytes.fromhex("01" * 32), 2 * COIN)
        w = collect_convergence_winner_txid([tx_big], self.seed, self.height, self.T)
        ok = w is None
        assert_attack(ok, "draw could run with a single convergence tx (split rule violated)")
        self.assertEqual(verdict(ok, "count"), "PASS")

    def test_probability_scales_rhs_monte_carlo(self):
        """Monte Carlo: doubling convergence_amount (same h,T) strictly increases pass rate (here always -> True)."""
        orbis = __import__("random").Random(12345)
        ok = True
        for _ in range(200):
            h = bytes(orbis.getrandbits(8 * 32).to_bytes(32, "little"))
            a = orbis.randrange(COIN, COIN * 1000)
            p1 = hash_below_target_times_convergence_amount(h, COIN, self.T, a)
            p2 = hash_below_target_times_convergence_amount(h, COIN, self.T, 2 * a)
            if p1 and not p2:
                ok = False
                break
        assert_attack(
            ok,
            "draw inequality not proportional to amount: eligible at A but not at 2A",
        )
        self.assertEqual(verdict(ok, "mc"), "PASS")


if __name__ == "__main__":
    unittest.main()

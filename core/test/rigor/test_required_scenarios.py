#!/usr/bin/env python3
import unittest

from rigor_model import (
    RigorCommitment,
    convergenceCandidate,
    compute_winner,
    validate_commitment,
    validate_res_mint_binding,
)


class TestRequiredRigorScenarios(unittest.TestCase):
    """Required scenario set for RIGOR trust baseline."""

    def setUp(self):
        self.seed = bytes.fromhex("42" * 32)
        self.height = 200
        self.target_prev = 0x1D00FFFF

        self.convergence_a = convergenceCandidate(bytes.fromhex("01" * 32))
        self.convergence_b = convergenceCandidate(bytes.fromhex("02" * 32))
        self.wrong_winner_txid = bytes.fromhex("ff" * 32)

    def test_1_minimal_convergence_no_winner(self):
        # 1 convergence tx -> no winner -> no RES mint -> valid
        winner = compute_winner([self.convergence_a], self.seed, self.height)
        self.assertIsNone(winner)
        self.assertTrue(validate_res_mint_binding([], winner))
        c = RigorCommitment(winner_txid=bytes(32), nbits=self.target_prev)
        self.assertTrue(validate_commitment(c, winner, self.target_prev))

    def test_2_normal_winner(self):
        # 2 convergence tx -> has winner -> RES mint txid == winner -> valid
        winner = compute_winner([self.convergence_a, self.convergence_b], self.seed, self.height)
        self.assertIsNotNone(winner)
        self.assertTrue(validate_res_mint_binding([winner], winner))
        c = RigorCommitment(winner_txid=winner, nbits=self.target_prev)
        self.assertTrue(validate_commitment(c, winner, self.target_prev))

    def test_3_res_mint_wrong(self):
        # 2 convergence tx -> RES mint txid != winner -> reject
        winner = compute_winner([self.convergence_a, self.convergence_b], self.seed, self.height)
        self.assertIsNotNone(winner)
        self.assertFalse(validate_res_mint_binding([self.wrong_winner_txid], winner))

    def test_4_commitment_wrong(self):
        # winner correct but commitment wrong -> reject
        winner = compute_winner([self.convergence_a, self.convergence_b], self.seed, self.height)
        self.assertIsNotNone(winner)
        c = RigorCommitment(winner_txid=self.wrong_winner_txid, nbits=self.target_prev)
        self.assertFalse(validate_commitment(c, winner, self.target_prev))

    def test_5_target_wrong(self):
        # nBits wrong -> reject
        winner = compute_winner([self.convergence_a, self.convergence_b], self.seed, self.height)
        self.assertIsNotNone(winner)
        c = RigorCommitment(winner_txid=winner, nbits=0x207FFFFF)
        self.assertFalse(validate_commitment(c, winner, self.target_prev))

    def test_6_multiple_res_mints(self):
        # 2 RES mint txs -> reject
        winner = compute_winner([self.convergence_a, self.convergence_b], self.seed, self.height)
        self.assertIsNotNone(winner)
        self.assertFalse(validate_res_mint_binding([winner, winner], winner))

    def test_7_res_mint_without_winner(self):
        # convergence < 2 but has RES mint binding -> reject
        winner = compute_winner([self.convergence_a], self.seed, self.height)
        self.assertIsNone(winner)
        self.assertFalse(validate_res_mint_binding([self.wrong_winner_txid], winner))


if __name__ == "__main__":
    unittest.main()

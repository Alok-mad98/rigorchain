#!/usr/bin/env python3
import unittest

from rigor_model import RigorCommitment, validate_commitment


class TestCommitment(unittest.TestCase):
    """Layer 4: commitment invariants only."""

    def test_commitment_matches_winner_and_nbits(self):
        winner = bytes.fromhex("12" * 32)
        c = RigorCommitment(winner_txid=winner, nbits=0x1D00FFFF)
        self.assertTrue(validate_commitment(c, winner, 0x1D00FFFF))

    def test_wrong_commitment_is_rejected(self):
        winner = bytes.fromhex("34" * 32)
        wrong_winner = bytes.fromhex("56" * 32)
        c1 = RigorCommitment(winner_txid=wrong_winner, nbits=0x1D00FFFF)
        c2 = RigorCommitment(winner_txid=winner, nbits=0x207FFFFF)
        self.assertFalse(validate_commitment(c1, winner, 0x1D00FFFF))
        self.assertFalse(validate_commitment(c2, winner, 0x1D00FFFF))


if __name__ == "__main__":
    unittest.main()

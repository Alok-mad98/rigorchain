#!/usr/bin/env python3
import unittest

from rigor_model import validate_res_mint_binding


class TestResBinding(unittest.TestCase):
    """Layer 3: Res binding invariants only."""

    def test_res_must_be_at_most_one_and_equal_winner(self):
        winner = bytes.fromhex("ab" * 32)
        self.assertTrue(validate_res_mint_binding([winner], winner))
        self.assertFalse(validate_res_mint_binding([bytes.fromhex("cd" * 32)], winner))
        self.assertFalse(validate_res_mint_binding([winner, winner], winner))

    def test_res_without_winner_is_rejected(self):
        res = bytes.fromhex("ef" * 32)
        self.assertFalse(validate_res_mint_binding([res], None))
        self.assertTrue(validate_res_mint_binding([], None))


if __name__ == "__main__":
    unittest.main()

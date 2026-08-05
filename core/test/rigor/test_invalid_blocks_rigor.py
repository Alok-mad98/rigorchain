#!/usr/bin/env python3
import unittest

from rigor_model import RigorCommitment, validate_invalid_block_cases


class TestInvalidBlocksRigor(unittest.TestCase):
    """Explicit invalid block cases for Rigor consensus invariants."""

    def test_invalid_cases_are_rejected(self):
        winner = bytes.fromhex("77" * 32)
        other = bytes.fromhex("88" * 32)
        nbits = 0x1D00FFFF

        # wrong winner commitment
        self.assertFalse(
            validate_invalid_block_cases(
                RigorCommitment(winner_txid=other, nbits=nbits),
                computed_winner=winner,
                computed_nbits=nbits,
                res_mint_txids=[winner],
            )
        )

        # wrong RES mint txid
        self.assertFalse(
            validate_invalid_block_cases(
                RigorCommitment(winner_txid=winner, nbits=nbits),
                computed_winner=winner,
                computed_nbits=nbits,
                res_mint_txids=[other],
            )
        )

        # multiple RES mint txs
        self.assertFalse(
            validate_invalid_block_cases(
                RigorCommitment(winner_txid=winner, nbits=nbits),
                computed_winner=winner,
                computed_nbits=nbits,
                res_mint_txids=[winner, winner],
            )
        )

        # wrong target
        self.assertFalse(
            validate_invalid_block_cases(
                RigorCommitment(winner_txid=winner, nbits=0x207FFFFF),
                computed_winner=winner,
                computed_nbits=nbits,
                res_mint_txids=[winner],
            )
        )

        # RES mint without winner
        self.assertFalse(
            validate_invalid_block_cases(
                RigorCommitment(winner_txid=bytes(32), nbits=nbits),
                computed_winner=None,
                computed_nbits=nbits,
                res_mint_txids=[winner],
            )
        )


if __name__ == "__main__":
    unittest.main()

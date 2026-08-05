#!/usr/bin/env python3
import unittest

from rigor_model import convergenceCandidate, _xor256_bytes, compute_convergence_hash, compute_convergence_root_bytes, compute_winner


class TestconvergenceWinner(unittest.TestCase):
    """Layer 2: convergence->winner invariants only."""

    def test_less_than_two_convergence_has_no_winner(self):
        seed = bytes.fromhex("11" * 32)
        winner = compute_winner([convergenceCandidate(bytes.fromhex("aa" * 32))], seed, 100)
        self.assertIsNone(winner)

    def test_two_or_more_convergence_has_argmin_winner(self):
        seed = bytes.fromhex("22" * 32)
        a = convergenceCandidate(bytes.fromhex("01" * 32))
        b = convergenceCandidate(bytes.fromhex("02" * 32))
        c = convergenceCandidate(bytes.fromhex("03" * 32))
        target_prev = 0x1D00FFFF
        winner = compute_winner([a, b, c], seed, 200)
        self.assertIsNotNone(winner)
        fr = compute_convergence_root_bytes([a, b, c])
        scores = {
            a.txid: compute_convergence_hash(a.txid, seed, 200, fr),
            b.txid: compute_convergence_hash(b.txid, seed, 200, fr),
            c.txid: compute_convergence_hash(c.txid, seed, 200, fr),
        }
        expected = min(
            scores.items(),
            key=lambda kv: (kv[1], _xor256_bytes(kv[0], fr)),
        )[0]
        self.assertEqual(winner, expected)

    def test_xor_tie_break_bytes_order_not_raw_txid_order(self):
        root = bytes.fromhex("ff" * 32)
        t1 = bytes.fromhex("00" * 31 + "01")
        t2 = bytes.fromhex("00" * 31 + "02")
        self.assertLess(t1, t2)
        self.assertGreater(_xor256_bytes(t1, root), _xor256_bytes(t2, root))


if __name__ == "__main__":
    unittest.main()

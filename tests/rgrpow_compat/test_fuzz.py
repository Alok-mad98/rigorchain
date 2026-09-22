"""Avalanche / mutation fuzz: any header byte change must change the digest."""
from __future__ import annotations

import random
import unittest

from rgrpow_lib import opuhash, vector_by_id


class FuzzTests(unittest.TestCase):
    def test_single_byte_flip_changes_digest(self):
        base = bytes.fromhex(vector_by_id("002")["header_hex"])
        baseline = opuhash(base)
        for i in range(80):
            mutated = bytearray(base)
            mutated[i] ^= 0x01
            self.assertNotEqual(opuhash(bytes(mutated)), baseline, f"byte {i}")

    def test_nonce_ntime_bits_sensitivity(self):
        base = bytearray(bytes.fromhex(vector_by_id("002")["header_hex"]))
        baseline = opuhash(bytes(base))
        for offset in (68, 72, 76):  # ntime, nBits, nonce
            mutated = bytearray(base)
            mutated[offset] ^= 0x80
            self.assertNotEqual(opuhash(bytes(mutated)), baseline, f"offset {offset}")

    def test_random_mutations(self):
        rng = random.Random(0x52475250)  # 'RGRP'
        base = bytes.fromhex(vector_by_id("001")["header_hex"])
        baseline = opuhash(base)
        for _ in range(64):
            mutated = bytearray(base)
            idx = rng.randrange(80)
            bit = 1 << rng.randrange(8)
            mutated[idx] ^= bit
            if mutated == base:
                continue
            self.assertNotEqual(opuhash(bytes(mutated)), baseline)


if __name__ == "__main__":
    unittest.main()

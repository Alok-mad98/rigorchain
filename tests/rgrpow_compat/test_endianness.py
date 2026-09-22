"""Digest endianness vs target."""
from __future__ import annotations

import unittest

from rgrpow_lib import meets_target, opuhash, uint256_le, vector_by_id


class EndiannessTests(unittest.TestCase):
    def test_byte0_is_lsb(self):
        v = vector_by_id("001")
        digest = bytes.fromhex(v["digest_hex"])
        self.assertEqual(digest[0], 0xDD)
        self.assertEqual(digest[0], int(v["lsb_byte"], 16))
        self.assertEqual(uint256_le(digest) & 0xFF, 0xDD)
        display = f"{uint256_le(digest):064x}"
        self.assertTrue(display.endswith("dd"))
        self.assertEqual(display, v["digest_be_display"])

    def test_reverse_digest_before_compare_fails(self):
        v = vector_by_id("003")
        digest = opuhash(bytes.fromhex(v["header_hex"]))
        target = bytes.fromhex(v["target_le_hex"])
        self.assertTrue(meets_target(digest, target))
        reversed_digest = digest[::-1]
        self.assertFalse(
            meets_target(reversed_digest, target),
            "reversing the digest before compare must not look like valid work",
        )

    def test_be_integer_compare_is_wrong(self):
        v = vector_by_id("001")
        digest = bytes.fromhex(v["digest_hex"])
        le = int.from_bytes(digest, "little")
        be = int.from_bytes(digest, "big")
        self.assertNotEqual(le, be)
        self.assertEqual(le, int(v["digest_le_int"]))


if __name__ == "__main__":
    unittest.main()

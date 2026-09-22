"""Domain-separation and wrong-primitive negative tests."""
from __future__ import annotations

import hashlib
import unittest

from rgrpow_lib import keccak256_cpp, load_vectors, opuhash, sha3_256, sha3_cpp, vector_by_id


class NegativeCaseTests(unittest.TestCase):
    def test_sha3_header_without_tag(self):
        v = vector_by_id("001")
        header = bytes.fromhex(v["header_hex"])
        honest = bytes.fromhex(v["digest_hex"])
        untagged = sha3_256(header)
        self.assertEqual(untagged.hex(), load_vectors()["negative"]["sha3_header_only_hex"])
        self.assertNotEqual(untagged, honest)
        self.assertNotEqual(untagged, opuhash(header))
        self.assertEqual(sha3_cpp(header), untagged)

    def test_wrong_domain_tag_rgrpow00(self):
        v = vector_by_id("001")
        header = bytes.fromhex(v["header_hex"])
        wrong = hashlib.sha3_256(b"RGRPOW00" + header).digest()
        self.assertEqual(wrong.hex(), load_vectors()["negative"]["sha3_rgrpow00_hex"])
        self.assertNotEqual(wrong.hex(), v["digest_hex"])
        self.assertEqual(sha3_cpp(b"RGRPOW00" + header), wrong)

    def test_keccak256_header_not_opuhash(self):
        v = vector_by_id("001")
        header = bytes.fromhex(v["header_hex"])
        k = keccak256_cpp(header)
        self.assertNotEqual(k.hex(), v["digest_hex"])
        # Ethereum empty-message KAT pins the 0x01 path.
        self.assertEqual(
            keccak256_cpp(b"").hex(),
            "c5d2460186f7233c927e7db2dcc703c0e500b653ca82273b7bfad8045d85a470",
        )

    def test_keccak256_preimage_not_opuhash(self):
        v = vector_by_id("001")
        pre = bytes.fromhex(v["preimage_hex"])
        k = keccak256_cpp(pre)
        self.assertEqual(k.hex(), load_vectors()["negative"]["keccak256_preimage_001_hex"])
        self.assertNotEqual(k.hex(), v["digest_hex"])

    def test_one_byte_header_change(self):
        v = vector_by_id("001")
        header = bytearray(80)
        header[76] = 1
        digest = opuhash(bytes(header))
        self.assertEqual(digest.hex(), load_vectors()["negative"]["opuhash_nonce1_hex"])
        self.assertNotEqual(digest.hex(), v["digest_hex"])


if __name__ == "__main__":
    unittest.main()

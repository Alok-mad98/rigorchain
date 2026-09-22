"""Golden OpuHash / rgrpow01 digest tests."""
from __future__ import annotations

import hashlib
import unittest

from rgrpow_lib import (
    DOMAIN_TAG,
    HEADER_LEN,
    PREIMAGE_LEN,
    load_vectors,
    meets_target,
    opuhash,
    serialize_header,
    set_compact,
    rpc_hex_to_internal,
    vector_by_id,
)


class GoldenHashTests(unittest.TestCase):
    def test_fips_primitive(self):
        vec = load_vectors()["fips"]
        self.assertEqual(hashlib.sha3_256(b"").hexdigest(), vec["empty"])
        self.assertEqual(hashlib.sha3_256(b"abc").hexdigest(), vec["abc"])

    def test_vector_001_zero_header(self):
        v = vector_by_id("001")
        header = bytes.fromhex(v["header_hex"])
        self.assertEqual(len(header), HEADER_LEN)
        pre = DOMAIN_TAG + header
        self.assertEqual(len(pre), PREIMAGE_LEN)
        self.assertEqual(pre.hex(), v["preimage_hex"])
        digest = opuhash(header)
        self.assertEqual(digest.hex(), v["digest_hex"])
        self.assertEqual(digest[0], int(v["lsb_byte"], 16))
        self.assertEqual(int.from_bytes(digest, "little"), int(v["digest_le_int"]))
        self.assertEqual(digest[::-1].hex(), v["digest_be_display"])

    def test_vector_002_typed_header(self):
        v = vector_by_id("002")
        f = v["fields"]
        header = serialize_header(
            int(f["version"], 16),
            rpc_hex_to_internal(f["prev_rpc_hex"]),
            rpc_hex_to_internal(f["merkle_rpc_hex"]),
            int(f["ntime"]),
            int(f["nbits"], 16),
            int(f["nonce"], 16),
        )
        self.assertEqual(header.hex(), v["header_hex"])
        digest = opuhash(header)
        self.assertEqual(digest.hex(), v["digest_hex"])
        self.assertEqual(digest[::-1].hex(), v["digest_be_display"])

    def test_vector_003_valid_pow(self):
        v = vector_by_id("003")
        self.assertTrue(v["valid_pow"])
        header = bytes.fromhex(v["header_hex"])
        digest = opuhash(header)
        self.assertEqual(digest.hex(), v["digest_hex"])
        target = bytes.fromhex(v["target_le_hex"])
        self.assertEqual(set_compact(int(v["nbits"], 16)), target)
        self.assertTrue(meets_target(digest, target))
        f = v["fields"]
        rebuilt = serialize_header(
            int(f["version"], 16),
            rpc_hex_to_internal(f["prev_rpc_hex"]),
            rpc_hex_to_internal(f["merkle_rpc_hex"]),
            int(f["ntime"]),
            int(f["nbits"], 16),
            int(f["nonce"], 16),
        )
        self.assertEqual(rebuilt, header)
        self.assertEqual(int.from_bytes(header[76:80], "little"), v["nonce"])


if __name__ == "__main__":
    unittest.main()

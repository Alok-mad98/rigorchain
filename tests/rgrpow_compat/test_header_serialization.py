"""80-byte Bitcoin-style header serialization."""
from __future__ import annotations

import struct
import unittest

from rgrpow_lib import opuhash, rpc_hex_to_internal, serialize_header, vector_by_id


class HeaderSerializationTests(unittest.TestCase):
    def test_field_layout_and_endian(self):
        v = vector_by_id("002")
        f = v["fields"]
        header = bytes.fromhex(v["header_hex"])
        self.assertEqual(len(header), 80)
        self.assertEqual(struct.unpack_from("<I", header, 0)[0], int(f["version"], 16))
        self.assertEqual(header[4:36], rpc_hex_to_internal(f["prev_rpc_hex"]))
        self.assertEqual(header[36:68], rpc_hex_to_internal(f["merkle_rpc_hex"]))
        self.assertEqual(struct.unpack_from("<I", header, 68)[0], int(f["ntime"]))
        self.assertEqual(struct.unpack_from("<I", header, 72)[0], int(f["nbits"], 16))
        self.assertEqual(struct.unpack_from("<I", header, 76)[0], int(f["nonce"], 16))

    def test_rpc_prev_must_be_reversed(self):
        v = vector_by_id("002")
        f = v["fields"]
        wrong_prev = bytes.fromhex(f["prev_rpc_hex"])  # display order, not internal
        header = serialize_header(
            int(f["version"], 16),
            wrong_prev,
            rpc_hex_to_internal(f["merkle_rpc_hex"]),
            int(f["ntime"]),
            int(f["nbits"], 16),
            int(f["nonce"], 16),
        )
        self.assertNotEqual(header.hex(), v["header_hex"])
        self.assertNotEqual(opuhash(header).hex(), v["digest_hex"])

    def test_big_endian_uint32_must_fail(self):
        v = vector_by_id("002")
        f = v["fields"]
        header = (
            struct.pack(">I", int(f["version"], 16))
            + rpc_hex_to_internal(f["prev_rpc_hex"])
            + rpc_hex_to_internal(f["merkle_rpc_hex"])
            + struct.pack(">I", int(f["ntime"]))
            + struct.pack(">I", int(f["nbits"], 16))
            + struct.pack(">I", int(f["nonce"], 16))
        )
        self.assertEqual(len(header), 80)
        self.assertNotEqual(header.hex(), v["header_hex"])
        self.assertNotEqual(opuhash(header).hex(), v["digest_hex"])

    def test_swapped_prev_and_merkle_must_fail(self):
        v = vector_by_id("002")
        f = v["fields"]
        header = serialize_header(
            int(f["version"], 16),
            rpc_hex_to_internal(f["merkle_rpc_hex"]),
            rpc_hex_to_internal(f["prev_rpc_hex"]),
            int(f["ntime"]),
            int(f["nbits"], 16),
            int(f["nonce"], 16),
        )
        self.assertNotEqual(opuhash(header).hex(), v["digest_hex"])


if __name__ == "__main__":
    unittest.main()

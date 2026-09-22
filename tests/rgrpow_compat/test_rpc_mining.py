"""GBT header assembly and submitblock blob layout (no consensus-rule change)."""
from __future__ import annotations

import unittest

from rgrpow_lib import (
    meets_target,
    opuhash,
    rpc_hex_to_internal,
    serialize_header,
    set_compact,
    vector_by_id,
)

REQUIRED_GBT = (
    "version",
    "previousblockhash",
    "transactions",
    "bits",
    "target",
    "height",
    "curtime",
    "default_coinbase_tx",
)


class RpcMiningTests(unittest.TestCase):
    def test_gbt_required_fields_present(self):
        tmpl = self._fixture_gbt()
        for key in REQUIRED_GBT:
            self.assertIn(key, tmpl)

    def test_miner_assembles_header80_from_gbt(self):
        v = vector_by_id("003")
        tmpl = self._fixture_gbt()
        header = serialize_header(
            tmpl["version"],
            rpc_hex_to_internal(tmpl["previousblockhash"]),
            rpc_hex_to_internal(tmpl["merkleroot"]),
            tmpl["curtime"],
            int(tmpl["bits"], 16),
            tmpl["nonce"],
        )
        self.assertEqual(len(header), 80)
        self.assertEqual(header.hex(), v["header_hex"])
        digest = opuhash(header)
        self.assertEqual(digest.hex(), v["digest_hex"])
        self.assertTrue(meets_target(digest, set_compact(int(tmpl["bits"], 16))))

    def test_submitblock_hex_starts_with_header(self):
        v = vector_by_id("003")
        header = bytes.fromhex(v["header_hex"])
        # compact-size tx count 1 + empty placeholder is enough to pin layout:
        # submitblock = header80 || txs_hex
        txs = bytes.fromhex("00")  # zero transactions would be invalid on node; layout only
        blob = header + txs
        self.assertEqual(blob[:80], header)
        self.assertEqual(blob[:80].hex(), v["header_hex"])

    def test_gbt_prevhash_is_rpc_display_endian(self):
        tmpl = self._fixture_gbt()
        internal = rpc_hex_to_internal(tmpl["previousblockhash"])
        self.assertEqual(internal.hex(), bytes.fromhex(tmpl["previousblockhash"])[::-1].hex())

    @staticmethod
    def _fixture_gbt() -> dict:
        v = vector_by_id("003")
        f = v["fields"]
        return {
            "version": int(f["version"], 16),
            "previousblockhash": f["prev_rpc_hex"],
            "merkleroot": f["merkle_rpc_hex"],
            "transactions": [],
            "bits": v["nbits"],
            "target": v["target_be_display"],
            "height": 1,
            "curtime": int(f["ntime"]),
            "nonce": int(f["nonce"], 16),
            "default_coinbase_tx": "00",
            "mutable": ["time"],
            "rigor_coinbase_immutable": True,
        }


if __name__ == "__main__":
    unittest.main()

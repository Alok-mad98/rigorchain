#!/usr/bin/env python3
# Copyright (c) 2018-2022 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the scantxoutset rpc call.

RigorChain is witness v0/v1 only for user-facing payment addresses; this test
funds witness outputs and exercises scantxoutset with wpkh/addr()/range scans.
"""
from test_framework.address import address_to_scriptpubkey
from test_framework.descriptors import descsum_create
from test_framework.messages import COIN
from test_framework.script import sha256
from test_framework.segwit_addr import encode_segwit_address
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, assert_raises_rpc_error
from test_framework.wallet import (
    MiniWallet,
    getnewdestination,
)

import json
import os
from decimal import Decimal


def descriptors(out):
    return sorted(u['desc'] for u in out['unspents'])


# Same xprv as historical Bitcoin Core scantxoutset vectors; paths fund wpkh() outputs only.
TPRV = (
    "tprv8ZgxMBicQKsPd7Uf69XL1XwhmjHopUGep8GuEiJDZmbQz6o58LninorQAfcKZWARbtRtfnLcJ5MQ2AtHcQJCCRUcMRvmDUjyEmNUWwx8UbK"
)


class ScantxoutsetTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1

    def sendtodestination(self, destination, amount):
        # interpret strings as addresses, assume scriptPubKey otherwise
        if isinstance(destination, str):
            destination = address_to_scriptpubkey(destination)
        self.wallet.send_to(from_node=self.nodes[0], scriptPubKey=destination, amount=int(COIN * amount))

    def wpkh_at_path(self, path_after_key):
        """path_after_key like /0'/0h/0h (no closing paren)."""
        return self.nodes[0].deriveaddresses(descsum_create(f"wpkh({TPRV}{path_after_key})"))[0]

    def run_test(self):
        self.wallet = MiniWallet(self.nodes[0])

        self.log.info("Test if we find coinbase outputs.")
        assert_equal(sum(u["coinbase"] for u in self.nodes[0].scantxoutset("start", [self.wallet.get_descriptor()])["unspents"]), 49)

        self.log.info("Create witness UTXOs (v0/v1) and HD-derived wpkh UTXOs...")
        pubk1, spk_b32_a, addr_b32_a = getnewdestination("bech32")
        pubk2, spk_b32_b, addr_b32_b = getnewdestination("bech32")
        pubk3, spk_b32m, addr_b32m = getnewdestination("bech32m")
        self.sendtodestination(spk_b32_a, Decimal("0.001"))
        self.sendtodestination(spk_b32_b, Decimal("0.002"))
        self.sendtodestination(spk_b32m, Decimal("0.004"))

        hd_funds = [
            ("/0'/0h/0h", Decimal("0.008")),
            ("/0'/0'/1h", Decimal("0.016")),
            ("/0h/0'/1500'", Decimal("0.032")),
            ("/0h/0h/0", Decimal("0.064")),
            ("/0'/0h/1", Decimal("0.128")),
            ("/0h/0'/1500", Decimal("0.256")),
            ("/1/1/0'", Decimal("0.512")),
            ("/1/1/1'", Decimal("1.024")),
            ("/1/1/1500h", Decimal("2.048")),
            ("/1/1/0", Decimal("4.096")),
            ("/1/1/1", Decimal("8.192")),
            ("/1/1/1500", Decimal("16.384")),
        ]
        for path, amt in hd_funds:
            self.sendtodestination(self.wpkh_at_path(path), amt)

        self.generate(self.nodes[0], 1)

        scan = self.nodes[0].scantxoutset("start", [])
        info = self.nodes[0].gettxoutsetinfo()
        assert_equal(scan['success'], True)
        assert_equal(scan['height'], info['height'])
        assert_equal(scan['txouts'], info['txouts'])
        assert_equal(scan['bestblock'], info['bestblock'])

        self.log.info("Test if we have found the expected witness and HD wpkh unspent outputs.")
        assert_equal(
            self.nodes[0].scantxoutset("start", ["wpkh(" + pubk1.hex() + ")", "wpkh(" + pubk2.hex() + ")", "wpkh(" + pubk3.hex() + ")"])['total_amount'],
            Decimal("0.003"),
        )
        assert_equal(
            self.nodes[0].scantxoutset("start", ["addr(" + addr_b32_a + ")", "addr(" + addr_b32_b + ")", "addr(" + addr_b32m + ")"])['total_amount'],
            Decimal("0.007"),
        )

        self.log.info("Test range validation.")
        assert_raises_rpc_error(-8, "End of range is too high", self.nodes[0].scantxoutset, "start", [{"desc": "desc", "range": -1}])
        assert_raises_rpc_error(-8, "Range should be greater or equal than 0", self.nodes[0].scantxoutset, "start", [{"desc": "desc", "range": [-1, 10]}])
        assert_raises_rpc_error(-8, "End of range is too high", self.nodes[0].scantxoutset, "start", [{"desc": "desc", "range": [(2 << 31 + 1) - 1000000, (2 << 31 + 1)]}])
        assert_raises_rpc_error(-8, "Range specified as [begin,end] must not have begin after end", self.nodes[0].scantxoutset, "start", [{"desc": "desc", "range": [2, 1]}])
        assert_raises_rpc_error(-8, "Range is too large", self.nodes[0].scantxoutset, "start", [{"desc": "desc", "range": [0, 1000001]}])

        self.log.info("Test extended key derivation (wpkh only).")
        assert_equal(self.nodes[0].scantxoutset("start", ["wpkh(" + TPRV + "/0'/0h/0h)"])['total_amount'], Decimal("0.008"))
        assert_equal(self.nodes[0].scantxoutset("start", ["wpkh(" + TPRV + "/0'/0'/1h)"])['total_amount'], Decimal("0.016"))
        assert_equal(self.nodes[0].scantxoutset("start", ["wpkh(" + TPRV + "/0h/0'/1500')"])['total_amount'], Decimal("0.032"))
        assert_equal(self.nodes[0].scantxoutset("start", ["wpkh(" + TPRV + "/0h/0h/0)"])['total_amount'], Decimal("0.064"))
        assert_equal(self.nodes[0].scantxoutset("start", ["wpkh(" + TPRV + "/0'/0h/1)"])['total_amount'], Decimal("0.128"))
        assert_equal(self.nodes[0].scantxoutset("start", ["wpkh(" + TPRV + "/0h/0'/1500)"])['total_amount'], Decimal("0.256"))
        assert_equal(self.nodes[0].scantxoutset("start", [{"desc": "wpkh(" + TPRV + "/0'/0h/*h)", "range": 1499}])['total_amount'], Decimal("0.024"))
        assert_equal(self.nodes[0].scantxoutset("start", [{"desc": "wpkh(" + TPRV + "/0'/0'/*h)", "range": 1500}])['total_amount'], Decimal("0.056"))
        assert_equal(self.nodes[0].scantxoutset("start", [{"desc": "wpkh(" + TPRV + "/0h/0'/*)", "range": 1499}])['total_amount'], Decimal("0.192"))
        assert_equal(self.nodes[0].scantxoutset("start", [{"desc": "wpkh(" + TPRV + "/0'/0h/*)", "range": 1500}])['total_amount'], Decimal("0.448"))
        assert_equal(self.nodes[0].scantxoutset("start", ["wpkh(" + TPRV + "/1/1/0')"])['total_amount'], Decimal("0.512"))
        assert_equal(self.nodes[0].scantxoutset("start", ["wpkh(" + TPRV + "/1/1/1')"])['total_amount'], Decimal("1.024"))
        assert_equal(self.nodes[0].scantxoutset("start", ["wpkh(" + TPRV + "/1/1/1500h)"])['total_amount'], Decimal("2.048"))
        assert_equal(self.nodes[0].scantxoutset("start", ["wpkh(" + TPRV + "/1/1/0)"])['total_amount'], Decimal("4.096"))
        assert_equal(self.nodes[0].scantxoutset("start", ["wpkh(" + TPRV + "/1/1/1)"])['total_amount'], Decimal("8.192"))
        assert_equal(self.nodes[0].scantxoutset("start", ["wpkh(" + TPRV + "/1/1/1500)"])['total_amount'], Decimal("16.384"))
        assert_equal(self.nodes[0].scantxoutset("start", [{"desc": "wpkh(" + TPRV + "/1/1/*')", "range": 1499}])['total_amount'], Decimal("1.536"))
        assert_equal(self.nodes[0].scantxoutset("start", [{"desc": "wpkh(" + TPRV + "/1/1/*')", "range": 1500}])['total_amount'], Decimal("3.584"))
        assert_equal(self.nodes[0].scantxoutset("start", [{"desc": "wpkh(" + TPRV + "/1/1/*)", "range": 1499}])['total_amount'], Decimal("12.288"))
        assert_equal(self.nodes[0].scantxoutset("start", [{"desc": "wpkh(" + TPRV + "/1/1/*)", "range": 1500}])['total_amount'], Decimal("28.672"))
        assert_equal(self.nodes[0].scantxoutset("start", [{"desc": "wpkh(" + TPRV + "/1/1/*)", "range": [1500, 1500]}])['total_amount'], Decimal("16.384"))

        self.log.info("Test the reported descriptors for a few matches (witness v0).")
        out0 = self.nodes[0].scantxoutset("start", [{"desc": "wpkh(" + TPRV + "/0h/0'/*)", "range": 1499}])
        d0 = descriptors(out0)
        assert_equal(len(d0), 2)
        assert all(x.startswith("wpkh(") for x in d0)
        assert any("026dbd8b2315f296d36e6b6920b1579ca75569464875c7ebe869b536a7d9503c8c" in x for x in d0)
        assert any("033e6f25d76c00bedb3a8993c7d5739ee806397f0529b1b31dda31ef890f19a60c" in x for x in d0)
        d1 = descriptors(self.nodes[0].scantxoutset("start", ["wpkh(" + TPRV + "/1/1/0)"]))
        assert_equal(len(d1), 1)
        assert d1[0].startswith("wpkh(")
        assert "03e1c5b6e650966971d7e71ef2674f80222752740fc1dfd63bbbd220d2da9bd0fb" in d1[0]

        self.log.info("BIP67 sortedmulti under wsh() matches native witness script hash.")

        def multisig_threshold(script_bytes):
            """Return m for OP_m ... OP_n OP_CHECKMULTISIG multisig redeemscript."""
            assert script_bytes[-1] == 0xae
            return script_bytes[0] - 0x50

        with open(os.path.join(os.path.dirname(os.path.realpath(__file__)), 'data/rpc_bip67.json'), encoding='utf-8') as f:
            vectors = json.load(f)
        for t in vectors:
            witness_script = bytes.fromhex(t["script"])
            expect_addr = encode_segwit_address("rgrt", 0, sha256(witness_script))
            m = multisig_threshold(witness_script)
            key_str = ",".join(t["keys"])
            desc_sorted = descsum_create("wsh(sortedmulti({},{}))".format(m, key_str))
            assert_equal(self.nodes[0].deriveaddresses(desc_sorted)[0], expect_addr)
            sorted_key_str = ",".join(t["sorted_keys"])
            desc_multi = descsum_create("wsh(multi({},{}))".format(m, sorted_key_str))
            assert_equal(self.nodes[0].deriveaddresses(desc_multi)[0], expect_addr)

        # Check that status and abort don't need second arg
        assert_equal(self.nodes[0].scantxoutset("status"), None)
        assert_equal(self.nodes[0].scantxoutset("abort"), False)

        # Check that second arg is needed for start
        assert_raises_rpc_error(-1, "scanobjects argument is required for the start action", self.nodes[0].scantxoutset, "start")


if __name__ == "__main__":
    ScantxoutsetTest().main()
